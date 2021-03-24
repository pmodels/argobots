/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"
#include <time.h>

/* FIFO pool implementation */

static int pool_init(ABT_pool pool, ABT_pool_config config);
static int pool_free(ABT_pool pool);
static size_t pool_get_size(ABT_pool pool);
static void pool_push_shared(ABT_pool pool, ABT_unit unit);
static void pool_push_private(ABT_pool pool, ABT_unit unit);
static ABT_unit pool_pop_shared(ABT_pool pool);
static ABT_unit pool_pop_private(ABT_pool pool);
static ABT_unit pool_pop_wait(ABT_pool pool, double time_secs);
static ABT_unit pool_pop_timedwait(ABT_pool pool, double abstime_secs);
static int pool_remove_shared(ABT_pool pool, ABT_unit unit);
static int pool_remove_private(ABT_pool pool, ABT_unit unit);
static int pool_print_all(ABT_pool pool, void *arg,
                          void (*print_fn)(void *, ABT_unit));

static ABT_bool unit_is_in_pool(ABT_unit unit);
static ABT_unit unit_create_from_thread(ABT_thread thread);
static void unit_free(ABT_unit *unit);

struct data {
    ABTD_spinlock mutex;
    size_t num_threads;
    ABTI_thread *p_head;
    ABTI_thread *p_tail;
    /* If the pool is empty, pop() accesses only is_empty so that pop() does not
     * slow down a push operation. */
    ABTD_atomic_int is_empty; /* Whether the pool is empty or not. */
};
typedef struct data data_t;

static inline data_t *pool_get_data_ptr(void *p_data)
{
    return (data_t *)p_data;
}

ABTU_ret_err static inline int spinlock_acquire_if_not_empty(data_t *p_data)
{
    if (ABTD_atomic_acquire_load_int(&p_data->is_empty)) {
        /* The pool is empty.  Lock is not taken. */
        return 1;
    }
    while (ABTD_spinlock_try_acquire(&p_data->mutex)) {
        /* Lock acquisition failed.  Check the size. */
        while (1) {
            if (ABTD_atomic_acquire_load_int(&p_data->is_empty)) {
                /* The pool becomes empty.  Lock is not taken. */
                return 1;
            } else if (!ABTD_spinlock_is_locked(&p_data->mutex)) {
                /* Lock seems released.  Let's try to take a lock again. */
                break;
            }
        }
    }
    /* Lock is acquired. */
    return 0;
}

/* Obtain the FIFO pool definition according to the access type */
ABTU_ret_err int ABTI_pool_get_fifo_def(ABT_pool_access access,
                                        ABTI_pool_def *p_def)
{
    /* Definitions according to the access type */
    /* FIXME: need better implementation, e.g., lock-free one */
    switch (access) {
        case ABT_POOL_ACCESS_PRIV:
            p_def->p_push = pool_push_private;
            p_def->p_pop = pool_pop_private;
            p_def->p_remove = pool_remove_private;
            break;

        case ABT_POOL_ACCESS_SPSC:
        case ABT_POOL_ACCESS_MPSC:
        case ABT_POOL_ACCESS_SPMC:
        case ABT_POOL_ACCESS_MPMC:
            p_def->p_push = pool_push_shared;
            p_def->p_pop = pool_pop_shared;
            p_def->p_remove = pool_remove_shared;
            break;

        default:
            ABTI_HANDLE_ERROR(ABT_ERR_INV_POOL_ACCESS);
    }

    /* Common definitions regardless of the access type */
    p_def->access = access;
    p_def->p_init = pool_init;
    p_def->p_free = pool_free;
    p_def->p_get_size = pool_get_size;
    p_def->p_pop_wait = pool_pop_wait;
    p_def->p_pop_timedwait = pool_pop_timedwait;
    p_def->p_print_all = pool_print_all;
    p_def->u_is_in_pool = unit_is_in_pool;
    p_def->u_create_from_thread = unit_create_from_thread;
    p_def->u_free = unit_free;

    return ABT_SUCCESS;
}

/* Pool functions */

static int pool_init(ABT_pool pool, ABT_pool_config config)
{
    ABTI_UNUSED(config);
    int abt_errno = ABT_SUCCESS;
    ABTI_pool *p_pool = ABTI_pool_get_ptr(pool);
    ABT_pool_access access;

    data_t *p_data;
    abt_errno = ABTU_malloc(sizeof(data_t), (void **)&p_data);
    ABTI_CHECK_ERROR(abt_errno);

    access = p_pool->access;

    if (access != ABT_POOL_ACCESS_PRIV) {
        /* Initialize the mutex */
        ABTD_spinlock_clear(&p_data->mutex);
    }

    p_data->num_threads = 0;
    p_data->p_head = NULL;
    p_data->p_tail = NULL;
    ABTD_atomic_relaxed_store_int(&p_data->is_empty, 1);

    p_pool->data = p_data;

    return abt_errno;
}

static int pool_free(ABT_pool pool)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_pool *p_pool = ABTI_pool_get_ptr(pool);
    data_t *p_data = pool_get_data_ptr(p_pool->data);

    ABTU_free(p_data);

    return abt_errno;
}

static size_t pool_get_size(ABT_pool pool)
{
    ABTI_pool *p_pool = ABTI_pool_get_ptr(pool);
    data_t *p_data = pool_get_data_ptr(p_pool->data);
    return p_data->num_threads;
}

static void pool_push_shared(ABT_pool pool, ABT_unit unit)
{
    ABTI_pool *p_pool = ABTI_pool_get_ptr(pool);
    data_t *p_data = pool_get_data_ptr(p_pool->data);
    ABTI_thread *p_thread = ABTI_unit_get_thread_from_builtin_unit(unit);

    ABTD_spinlock_acquire(&p_data->mutex);
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
    ABTD_spinlock_release(&p_data->mutex);
}

static void pool_push_private(ABT_pool pool, ABT_unit unit)
{
    ABTI_pool *p_pool = ABTI_pool_get_ptr(pool);
    data_t *p_data = pool_get_data_ptr(p_pool->data);
    ABTI_thread *p_thread = ABTI_unit_get_thread_from_builtin_unit(unit);

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

static ABT_unit pool_pop_wait(ABT_pool pool, double time_secs)
{
    ABTI_pool *p_pool = ABTI_pool_get_ptr(pool);
    data_t *p_data = pool_get_data_ptr(p_pool->data);
    ABTI_thread *p_thread = NULL;

    double time_start = 0.0;

    while (1) {
        if (spinlock_acquire_if_not_empty(p_data) == 0) {
            ABT_unit h_unit = ABT_UNIT_NULL;
            if (p_data->num_threads > 0) {
                p_thread = p_data->p_head;
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

                h_unit = ABTI_unit_get_builtin_unit(p_thread);
            }
            ABTD_spinlock_release(&p_data->mutex);
            if (h_unit != ABT_UNIT_NULL)
                return h_unit;
        }
        if (time_start == 0.0) {
            time_start = ABTI_get_wtime();
        } else {
            double elapsed = ABTI_get_wtime() - time_start;
            if (elapsed > time_secs)
                return ABT_UNIT_NULL;
        }
        /* Sleep. */
        const int sleep_nsecs = 100;
        struct timespec ts = { 0, sleep_nsecs };
        nanosleep(&ts, NULL);
    }
}

static ABT_unit pool_pop_timedwait(ABT_pool pool, double abstime_secs)
{
    ABTI_pool *p_pool = ABTI_pool_get_ptr(pool);
    data_t *p_data = pool_get_data_ptr(p_pool->data);
    ABTI_thread *p_thread = NULL;

    while (1) {
        if (spinlock_acquire_if_not_empty(p_data) == 0) {
            ABT_unit h_unit = ABT_UNIT_NULL;
            if (p_data->num_threads > 0) {
                p_thread = p_data->p_head;
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

                h_unit = ABTI_unit_get_builtin_unit(p_thread);
            }
            ABTD_spinlock_release(&p_data->mutex);
            if (h_unit != ABT_UNIT_NULL)
                return h_unit;
        }
        const int sleep_nsecs = 100;
        struct timespec ts = { 0, sleep_nsecs };
        nanosleep(&ts, NULL);

        if (ABTI_get_wtime() > abstime_secs)
            return ABT_UNIT_NULL;
    }
}

static ABT_unit pool_pop_shared(ABT_pool pool)
{
    ABTI_pool *p_pool = ABTI_pool_get_ptr(pool);
    data_t *p_data = pool_get_data_ptr(p_pool->data);
    ABTI_thread *p_thread = NULL;

    if (spinlock_acquire_if_not_empty(p_data) == 0) {
        ABT_unit h_unit = ABT_UNIT_NULL;
        if (p_data->num_threads > 0) {
            p_thread = p_data->p_head;
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

            h_unit = ABTI_unit_get_builtin_unit(p_thread);
        }
        ABTD_spinlock_release(&p_data->mutex);
        return h_unit;
    } else {
        return ABT_UNIT_NULL;
    }
}

static ABT_unit pool_pop_private(ABT_pool pool)
{
    ABTI_pool *p_pool = ABTI_pool_get_ptr(pool);
    data_t *p_data = pool_get_data_ptr(p_pool->data);
    ABTI_thread *p_thread = NULL;

    ABT_unit h_unit = ABT_UNIT_NULL;
    if (p_data->num_threads > 0) {
        p_thread = p_data->p_head;
        if (p_data->num_threads == 1) {
            p_data->p_head = NULL;
            p_data->p_tail = NULL;
            p_data->num_threads = 0;
            ABTD_atomic_relaxed_store_int(&p_data->is_empty, 1);
        } else {
            p_thread->p_prev->p_next = p_thread->p_next;
            p_thread->p_next->p_prev = p_thread->p_prev;
            p_data->p_head = p_thread->p_next;
            p_data->num_threads--;
        }

        p_thread->p_prev = NULL;
        p_thread->p_next = NULL;
        ABTD_atomic_release_store_int(&p_thread->is_in_pool, 0);

        h_unit = ABTI_unit_get_builtin_unit(p_thread);
    }
    return h_unit;
}

static int pool_remove_shared(ABT_pool pool, ABT_unit unit)
{
    ABTI_pool *p_pool = ABTI_pool_get_ptr(pool);
    data_t *p_data = pool_get_data_ptr(p_pool->data);
    ABTI_thread *p_thread = ABTI_unit_get_thread_from_builtin_unit(unit);

    ABTI_CHECK_TRUE(p_data->num_threads != 0, ABT_ERR_POOL);
    ABTI_CHECK_TRUE(ABTD_atomic_acquire_load_int(&p_thread->is_in_pool) == 1,
                    ABT_ERR_POOL);

    ABTD_spinlock_acquire(&p_data->mutex);
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
    ABTD_spinlock_release(&p_data->mutex);

    p_thread->p_prev = NULL;
    p_thread->p_next = NULL;

    return ABT_SUCCESS;
}

static int pool_remove_private(ABT_pool pool, ABT_unit unit)
{
    ABTI_pool *p_pool = ABTI_pool_get_ptr(pool);
    data_t *p_data = pool_get_data_ptr(p_pool->data);
    ABTI_thread *p_thread = ABTI_unit_get_thread_from_builtin_unit(unit);

    ABTI_CHECK_TRUE(p_data->num_threads != 0, ABT_ERR_POOL);
    ABTI_CHECK_TRUE(ABTD_atomic_acquire_load_int(&p_thread->is_in_pool) == 1,
                    ABT_ERR_POOL);

    if (p_data->num_threads == 1) {
        p_data->p_head = NULL;
        p_data->p_tail = NULL;
        p_data->num_threads = 0;
        ABTD_atomic_relaxed_store_int(&p_data->is_empty, 1);
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
    p_thread->p_prev = NULL;
    p_thread->p_next = NULL;

    return ABT_SUCCESS;
}

static int pool_print_all(ABT_pool pool, void *arg,
                          void (*print_fn)(void *, ABT_unit))
{
    ABT_pool_access access;
    ABTI_pool *p_pool = ABTI_pool_get_ptr(pool);
    data_t *p_data = pool_get_data_ptr(p_pool->data);

    access = p_pool->access;
    if (access != ABT_POOL_ACCESS_PRIV) {
        ABTD_spinlock_acquire(&p_data->mutex);
    }

    size_t num_threads = p_data->num_threads;
    ABTI_thread *p_thread = p_data->p_head;
    while (num_threads--) {
        ABTI_ASSERT(p_thread);
        ABT_unit unit = ABTI_unit_get_builtin_unit(p_thread);
        print_fn(arg, unit);
        p_thread = p_thread->p_next;
    }

    if (access != ABT_POOL_ACCESS_PRIV) {
        ABTD_spinlock_release(&p_data->mutex);
    }

    return ABT_SUCCESS;
}

/* Unit functions */

static ABT_bool unit_is_in_pool(ABT_unit unit)
{
    ABTI_thread *p_thread = ABTI_unit_get_thread_from_builtin_unit(unit);
    return ABTD_atomic_acquire_load_int(&p_thread->is_in_pool) ? ABT_TRUE
                                                               : ABT_FALSE;
}

static ABT_unit unit_create_from_thread(ABT_thread thread)
{
    /* Call ABTI_unit_init_builtin() instead. */
    ABTI_ASSERT(0);
    return ABT_UNIT_NULL;
}

static void unit_free(ABT_unit *unit)
{
    /* A built-in unit does not need to be freed.  This function may not be
     * called. */
    ABTI_ASSERT(0);
}
