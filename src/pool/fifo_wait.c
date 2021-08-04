/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"

/* FIFO_WAIT pool implementation */

static int pool_init(ABT_pool pool, ABT_pool_config config);
static void pool_free(ABT_pool pool);
static ABT_bool pool_is_empty(ABT_pool pool);
static size_t pool_get_size(ABT_pool pool);
static void pool_push(ABT_pool pool, ABT_unit unit, ABT_pool_context context);
static ABT_thread pool_pop(ABT_pool pool, ABT_pool_context context);
static ABT_thread pool_pop_wait(ABT_pool pool, double time_secs,
                                ABT_pool_context context);
static void pool_push_many(ABT_pool pool, const ABT_unit *units,
                           size_t num_units, ABT_pool_context context);
static void pool_pop_many(ABT_pool pool, ABT_thread *threads,
                          size_t max_threads, size_t *num_popped,
                          ABT_pool_context context);
static void pool_print_all(ABT_pool pool, void *arg,
                           void (*print_fn)(void *, ABT_thread));
static ABT_unit pool_create_unit(ABT_pool pool, ABT_thread thread);
static void pool_free_unit(ABT_pool pool, ABT_unit unit);

/* For backward compatibility */
static int pool_remove(ABT_pool pool, ABT_unit unit);
static ABT_unit pool_pop_timedwait(ABT_pool pool, double abstime_secs);
static ABT_bool pool_unit_is_in_pool(ABT_unit unit);

struct data {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    size_t num_threads;
    ABTI_thread *p_head;
    ABTI_thread *p_tail;
    ABTD_atomic_int is_empty; /* Whether the pool is empty or not. */
};
typedef struct data data_t;

static inline data_t *pool_get_data_ptr(void *p_data)
{
    return (data_t *)p_data;
}

ABTU_ret_err int
ABTI_pool_get_fifo_wait_def(ABT_pool_access access,
                            ABTI_pool_required_def *p_required_def,
                            ABTI_pool_optional_def *p_optional_def,
                            ABTI_pool_deprecated_def *p_deprecated_def)
{
    p_optional_def->p_init = pool_init;
    p_optional_def->p_free = pool_free;
    p_required_def->p_is_empty = pool_is_empty;
    p_optional_def->p_get_size = pool_get_size;
    p_required_def->p_push = pool_push;
    p_required_def->p_pop = pool_pop;
    p_optional_def->p_pop_wait = pool_pop_wait;
    p_optional_def->p_push_many = pool_push_many;
    p_optional_def->p_pop_many = pool_pop_many;
    p_optional_def->p_print_all = pool_print_all;
    p_required_def->p_create_unit = pool_create_unit;
    p_required_def->p_free_unit = pool_free_unit;

    p_deprecated_def->p_pop_timedwait = pool_pop_timedwait;
    p_deprecated_def->u_is_in_pool = pool_unit_is_in_pool;
    p_deprecated_def->p_remove = pool_remove;
    return ABT_SUCCESS;
}

/* Pool functions */

static int pool_init(ABT_pool pool, ABT_pool_config config)
{
    ABTI_UNUSED(config);
    int ret, abt_errno = ABT_SUCCESS;
    ABTI_pool *p_pool = ABTI_pool_get_ptr(pool);

    data_t *p_data;
    abt_errno = ABTU_malloc(sizeof(data_t), (void **)&p_data);
    ABTI_CHECK_ERROR(abt_errno);

    ret = pthread_mutex_init(&p_data->mutex, NULL);
    if (ret != 0) {
        ABTU_free(p_data);
        return ABT_ERR_SYS;
    }
    ret = pthread_cond_init(&p_data->cond, NULL);
    if (ret != 0) {
        pthread_mutex_destroy(&p_data->mutex);
        ABTU_free(p_data);
        return ABT_ERR_SYS;
    }

    p_data->num_threads = 0;
    p_data->p_head = NULL;
    p_data->p_tail = NULL;
    ABTD_atomic_relaxed_store_int(&p_data->is_empty, 1);

    p_pool->data = p_data;

    return abt_errno;
}

static void pool_free(ABT_pool pool)
{
    ABTI_pool *p_pool = ABTI_pool_get_ptr(pool);
    data_t *p_data = pool_get_data_ptr(p_pool->data);

    pthread_mutex_destroy(&p_data->mutex);
    pthread_cond_destroy(&p_data->cond);
    ABTU_free(p_data);
}

static ABT_bool pool_is_empty(ABT_pool pool)
{
    ABTI_pool *p_pool = ABTI_pool_get_ptr(pool);
    data_t *p_data = pool_get_data_ptr(p_pool->data);
    return ABTD_atomic_acquire_load_int(&p_data->is_empty) ? ABT_TRUE
                                                           : ABT_FALSE;
}

static size_t pool_get_size(ABT_pool pool)
{
    ABTI_pool *p_pool = ABTI_pool_get_ptr(pool);
    data_t *p_data = pool_get_data_ptr(p_pool->data);
    return p_data->num_threads;
}

static inline void pool_push_unsafe(data_t *p_data, ABTI_thread *p_thread)
{
    if (p_data->num_threads == 0) {
        p_thread->p_prev = p_thread;
        p_thread->p_next = p_thread;
        p_data->p_head = p_thread;
        p_data->p_tail = p_thread;
        p_data->num_threads = 1;
        ABTD_atomic_release_store_int(&p_data->is_empty, 0);
    } else {
        ABTI_thread *p_head = p_data->p_head;
        ABTI_thread *p_tail = p_data->p_tail;
        p_tail->p_next = p_thread;
        p_head->p_prev = p_thread;
        p_thread->p_prev = p_tail;
        p_thread->p_next = p_head;
        p_data->p_tail = p_thread;
        p_data->num_threads++;
    }
    ABTD_atomic_release_store_int(&p_thread->is_in_pool, 1);
}

static void pool_push(ABT_pool pool, ABT_unit unit, ABT_pool_context context)
{
    (void)context;
    ABTI_pool *p_pool = ABTI_pool_get_ptr(pool);
    data_t *p_data = pool_get_data_ptr(p_pool->data);
    ABTI_thread *p_thread = ABTI_unit_get_thread_from_builtin_unit(unit);

    pthread_mutex_lock(&p_data->mutex);
    pool_push_unsafe(p_data, p_thread);
    pthread_cond_signal(&p_data->cond);
    pthread_mutex_unlock(&p_data->mutex);
}

static void pool_push_many(ABT_pool pool, const ABT_unit *units,
                           size_t num_units, ABT_pool_context context)
{
    (void)context;
    ABTI_pool *p_pool = ABTI_pool_get_ptr(pool);
    data_t *p_data = pool_get_data_ptr(p_pool->data);

    if (num_units > 0) {
        pthread_mutex_lock(&p_data->mutex);
        size_t i;
        for (i = 0; i < num_units; i++) {
            ABTI_thread *p_thread =
                ABTI_unit_get_thread_from_builtin_unit(units[i]);
            pool_push_unsafe(p_data, p_thread);
        }
        if (num_units == 1) {
            /* Wake up a single waiter. */
            pthread_cond_signal(&p_data->cond);
        } else {
            /* Wake up all the waiters. */
            pthread_cond_broadcast(&p_data->cond);
        }
        pthread_mutex_unlock(&p_data->mutex);
    }
}

static inline ABT_thread pool_pop_unsafe(data_t *p_data)
{
    if (p_data->num_threads > 0) {
        ABTI_thread *p_thread = p_data->p_head;
        if (p_data->num_threads == 1) {
            p_data->p_head = NULL;
            p_data->p_tail = NULL;
            p_data->num_threads = 0;
            ABTD_atomic_release_store_int(&p_data->is_empty, 1);
        } else {
            p_thread->p_prev->p_next = p_thread->p_next;
            p_thread->p_next->p_prev = p_thread->p_prev;
            p_data->p_head = p_thread->p_next;
            p_data->num_threads--;
        }

        p_thread->p_prev = NULL;
        p_thread->p_next = NULL;
        ABTD_atomic_release_store_int(&p_thread->is_in_pool, 0);
        return ABTI_thread_get_handle(p_thread);
    } else {
        return ABT_THREAD_NULL;
    }
}

static ABT_thread pool_pop_wait(ABT_pool pool, double time_secs,
                                ABT_pool_context context)
{
    (void)context;
    ABTI_pool *p_pool = ABTI_pool_get_ptr(pool);
    data_t *p_data = pool_get_data_ptr(p_pool->data);
    pthread_mutex_lock(&p_data->mutex);
    if (!p_data->num_threads) {
#if defined(ABT_CONFIG_USE_CLOCK_GETTIME)
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += (time_t)time_secs;
        ts.tv_nsec += (long)((time_secs - (time_t)time_secs) * 1e9);
        if (ts.tv_nsec > 1e9) {
            ts.tv_sec += 1;
            ts.tv_nsec -= 1e9;
        }
        pthread_cond_timedwait(&p_data->cond, &p_data->mutex, &ts);
#else
        /* We cannot use pthread_cond_timedwait().  Let's use nanosleep()
         * instead */
        double start_time = ABTI_get_wtime();
        while (ABTI_get_wtime() - start_time < time_secs) {
            pthread_mutex_unlock(&p_data->mutex);
            const int sleep_nsecs = 100;
            struct timespec ts = { 0, sleep_nsecs };
            nanosleep(&ts, NULL);
            pthread_mutex_lock(&p_data->mutex);
            if (p_data->num_threads > 0)
                break;
        }
#endif
    }
    ABT_thread thread = pool_pop_unsafe(p_data);
    pthread_mutex_unlock(&p_data->mutex);
    return thread;
}

static inline void convert_double_sec_to_timespec(struct timespec *ts_out,
                                                  double seconds)
{
    ts_out->tv_sec = (time_t)seconds;
    ts_out->tv_nsec = (long)((seconds - ts_out->tv_sec) * 1000000000.0);
}

static ABT_unit pool_pop_timedwait(ABT_pool pool, double abstime_secs)
{
    ABTI_pool *p_pool = ABTI_pool_get_ptr(pool);
    data_t *p_data = pool_get_data_ptr(p_pool->data);
    pthread_mutex_lock(&p_data->mutex);
    if (!p_data->num_threads) {
        struct timespec ts;
        convert_double_sec_to_timespec(&ts, abstime_secs);
        pthread_cond_timedwait(&p_data->cond, &p_data->mutex, &ts);
    }
    ABT_thread thread = pool_pop_unsafe(p_data);
    pthread_mutex_unlock(&p_data->mutex);
    if (thread != ABT_THREAD_NULL) {
        return ABTI_unit_get_builtin_unit(ABTI_thread_get_ptr(thread));
    } else {
        return ABT_UNIT_NULL;
    }
}

static ABT_thread pool_pop(ABT_pool pool, ABT_pool_context context)
{
    (void)context;
    ABTI_pool *p_pool = ABTI_pool_get_ptr(pool);
    data_t *p_data = pool_get_data_ptr(p_pool->data);
    if (ABTD_atomic_acquire_load_int(&p_data->is_empty) == 0) {
        pthread_mutex_lock(&p_data->mutex);
        ABT_thread thread = pool_pop_unsafe(p_data);
        pthread_mutex_unlock(&p_data->mutex);
        return thread;
    } else {
        return ABT_THREAD_NULL;
    }
}

static void pool_pop_many(ABT_pool pool, ABT_thread *threads,
                          size_t max_threads, size_t *num_popped,
                          ABT_pool_context context)
{
    (void)context;
    ABTI_pool *p_pool = ABTI_pool_get_ptr(pool);
    data_t *p_data = pool_get_data_ptr(p_pool->data);
    if (max_threads != 0 &&
        ABTD_atomic_acquire_load_int(&p_data->is_empty) == 0) {
        pthread_mutex_lock(&p_data->mutex);
        size_t i;
        for (i = 0; i < max_threads; i++) {
            ABT_thread thread = pool_pop_unsafe(p_data);
            if (thread == ABT_THREAD_NULL)
                break;
            threads[i] = thread;
        }
        *num_popped = i;
        pthread_mutex_unlock(&p_data->mutex);
    } else {
        *num_popped = 0;
    }
}

static int pool_remove(ABT_pool pool, ABT_unit unit)
{
    ABTI_pool *p_pool = ABTI_pool_get_ptr(pool);
    data_t *p_data = pool_get_data_ptr(p_pool->data);
    ABTI_thread *p_thread = ABTI_unit_get_thread_from_builtin_unit(unit);

    ABTI_CHECK_TRUE(p_data->num_threads != 0, ABT_ERR_POOL);
    ABTI_CHECK_TRUE(ABTD_atomic_acquire_load_int(&p_thread->is_in_pool) == 1,
                    ABT_ERR_POOL);

    pthread_mutex_lock(&p_data->mutex);
    if (p_data->num_threads == 1) {
        p_data->p_head = NULL;
        p_data->p_tail = NULL;
        p_data->num_threads = 0;
        ABTD_atomic_release_store_int(&p_data->is_empty, 1);
    } else {
        p_thread->p_prev->p_next = p_thread->p_next;
        p_thread->p_next->p_prev = p_thread->p_prev;
        if (p_thread == p_data->p_head) {
            p_data->p_head = p_thread->p_next;
        } else if (p_thread == p_data->p_tail) {
            p_data->p_tail = p_thread->p_prev;
        }
        p_data->num_threads--;
    }
    ABTD_atomic_release_store_int(&p_thread->is_in_pool, 0);
    pthread_mutex_unlock(&p_data->mutex);
    p_thread->p_prev = NULL;
    p_thread->p_next = NULL;
    return ABT_SUCCESS;
}

static void pool_print_all(ABT_pool pool, void *arg,
                           void (*print_fn)(void *, ABT_thread))
{
    ABTI_pool *p_pool = ABTI_pool_get_ptr(pool);
    data_t *p_data = pool_get_data_ptr(p_pool->data);

    pthread_mutex_lock(&p_data->mutex);
    size_t num_threads = p_data->num_threads;
    ABTI_thread *p_thread = p_data->p_head;
    while (num_threads--) {
        ABTI_ASSERT(p_thread);
        ABT_thread thread = ABTI_thread_get_handle(p_thread);
        print_fn(arg, thread);
        p_thread = p_thread->p_next;
    }
    pthread_mutex_unlock(&p_data->mutex);
}

/* Unit functions */

static ABT_bool pool_unit_is_in_pool(ABT_unit unit)
{
    ABTI_thread *p_thread = ABTI_unit_get_thread_from_builtin_unit(unit);
    return ABTD_atomic_acquire_load_int(&p_thread->is_in_pool) ? ABT_TRUE
                                                               : ABT_FALSE;
}

static ABT_unit pool_create_unit(ABT_pool pool, ABT_thread thread)
{
    /* Call ABTI_unit_init_builtin() instead. */
    ABTI_ASSERT(0);
    return ABT_UNIT_NULL;
}

static void pool_free_unit(ABT_pool pool, ABT_unit unit)
{
    /* A built-in unit does not need to be freed.  This function may not be
     * called. */
    ABTI_ASSERT(0);
}
