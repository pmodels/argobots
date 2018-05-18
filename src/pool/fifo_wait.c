/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"

/* FIFO_WAIT pool implementation */

static int      pool_init(ABT_pool pool, ABT_pool_config config);
static int      pool_free(ABT_pool pool);
static size_t   pool_get_size(ABT_pool pool);
static void     pool_push(ABT_pool pool, ABT_unit unit);
static ABT_unit pool_pop(ABT_pool pool);
static ABT_unit pool_pop_timedwait(ABT_pool pool, double abstime_secs);
static int      pool_remove(ABT_pool pool, ABT_unit unit);

typedef ABTI_unit unit_t;
static ABT_unit_type unit_get_type(ABT_unit unit);
static ABT_thread unit_get_thread(ABT_unit unit);
static ABT_task unit_get_task(ABT_unit unit);
static ABT_bool unit_is_in_pool(ABT_unit unit);
static ABT_unit unit_create_from_thread(ABT_thread thread);
static ABT_unit unit_create_from_task(ABT_task task);
static void unit_free(ABT_unit *unit);

struct data {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    size_t num_units;
    unit_t *p_head;
    unit_t *p_tail;
};
typedef struct data data_t;

static inline data_t *pool_get_data_ptr(void *p_data)
{
    return (data_t *)p_data;
}

int ABTI_pool_get_fifo_wait_def(ABT_pool_access access, ABT_pool_def *p_def)
{
    p_def->access               = access;
    p_def->p_init               = pool_init;
    p_def->p_free               = pool_free;
    p_def->p_get_size           = pool_get_size;
    p_def->p_push               = pool_push;
    p_def->p_pop                = pool_pop;
    p_def->p_pop_timedwait      = pool_pop_timedwait;
    p_def->p_remove             = pool_remove;
    p_def->u_get_type           = unit_get_type;
    p_def->u_get_thread         = unit_get_thread;
    p_def->u_get_task           = unit_get_task;
    p_def->u_is_in_pool         = unit_is_in_pool;
    p_def->u_create_from_thread = unit_create_from_thread;
    p_def->u_create_from_task   = unit_create_from_task;
    p_def->u_free               = unit_free;

    return ABT_SUCCESS;
}


/* Pool functions */

int pool_init(ABT_pool pool, ABT_pool_config config)
{
    ABTI_UNUSED(config);
    int abt_errno = ABT_SUCCESS;

    data_t *p_data = (data_t *)ABTU_malloc(sizeof(data_t));

    pthread_mutex_init(&p_data->mutex, NULL);
    pthread_cond_init(&p_data->cond, NULL);

    p_data->num_units = 0;
    p_data->p_head = NULL;
    p_data->p_tail = NULL;

    ABT_pool_set_data(pool, p_data);

    return abt_errno;
}

static int pool_free(ABT_pool pool)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_pool *p_pool = ABTI_pool_get_ptr(pool);
    void *data = ABTI_pool_get_data(p_pool);
    data_t *p_data = pool_get_data_ptr(data);

    pthread_mutex_destroy(&p_data->mutex);
    pthread_cond_destroy(&p_data->cond);
    ABTU_free(p_data);

    return abt_errno;
}

static size_t pool_get_size(ABT_pool pool)
{
    ABTI_pool *p_pool = ABTI_pool_get_ptr(pool);
    void *data = ABTI_pool_get_data(p_pool);
    data_t *p_data = pool_get_data_ptr(data);
    return p_data->num_units;
}

static void pool_push(ABT_pool pool, ABT_unit unit)
{
    ABTI_pool *p_pool = ABTI_pool_get_ptr(pool);
    void *data = ABTI_pool_get_data(p_pool);
    data_t *p_data = pool_get_data_ptr(data);
    unit_t *p_unit = (unit_t *)unit;

    pthread_mutex_lock(&p_data->mutex);
    if (p_data->num_units == 0) {
        p_unit->p_prev = p_unit;
        p_unit->p_next = p_unit;
        p_data->p_head = p_unit;
        p_data->p_tail = p_unit;
    } else {
        unit_t *p_head = p_data->p_head;
        unit_t *p_tail = p_data->p_tail;
        p_tail->p_next = p_unit;
        p_head->p_prev = p_unit;
        p_unit->p_prev = p_tail;
        p_unit->p_next = p_head;
        p_data->p_tail = p_unit;
    }
    p_data->num_units++;

    p_unit->pool = pool;
    pthread_cond_signal(&p_data->cond);
    pthread_mutex_unlock(&p_data->mutex);
}

static inline void convert_double_sec_to_timespec(struct timespec *ts_out,
    double seconds)
{
    ts_out->tv_sec = (time_t)seconds;
    ts_out->tv_nsec = (long)((seconds-ts_out->tv_sec) * 1000000000.0);
}

static ABT_unit pool_pop_timedwait(ABT_pool pool, double abstime_secs)
{
    ABTI_pool *p_pool = ABTI_pool_get_ptr(pool);
    void *data = ABTI_pool_get_data(p_pool);
    data_t *p_data = pool_get_data_ptr(data);
    unit_t *p_unit = NULL;
    ABT_unit h_unit = ABT_UNIT_NULL;

    pthread_mutex_lock(&p_data->mutex);

    if(!p_data->num_units)
    {
        struct timespec ts;
        convert_double_sec_to_timespec(&ts, abstime_secs);
        pthread_cond_timedwait(&p_data->cond, &p_data->mutex, &ts);
    }

    if (p_data->num_units > 0) {
        p_unit = p_data->p_head;
        if (p_data->num_units == 1) {
            p_data->p_head = NULL;
            p_data->p_tail = NULL;
        } else {
            p_unit->p_prev->p_next = p_unit->p_next;
            p_unit->p_next->p_prev = p_unit->p_prev;
            p_data->p_head = p_unit->p_next;
        }
        p_data->num_units--;

        p_unit->p_prev = NULL;
        p_unit->p_next = NULL;
        p_unit->pool = ABT_POOL_NULL;

        h_unit = (ABT_unit)p_unit;
    }
    pthread_mutex_unlock(&p_data->mutex);

    return h_unit;
}

static ABT_unit pool_pop(ABT_pool pool)
{
    ABTI_pool *p_pool = ABTI_pool_get_ptr(pool);
    void *data = ABTI_pool_get_data(p_pool);
    data_t *p_data = pool_get_data_ptr(data);
    unit_t *p_unit = NULL;
    ABT_unit h_unit = ABT_UNIT_NULL;

    pthread_mutex_lock(&p_data->mutex);
    if (p_data->num_units > 0) {
        p_unit = p_data->p_head;
        if (p_data->num_units == 1) {
            p_data->p_head = NULL;
            p_data->p_tail = NULL;
        } else {
            p_unit->p_prev->p_next = p_unit->p_next;
            p_unit->p_next->p_prev = p_unit->p_prev;
            p_data->p_head = p_unit->p_next;
        }
        p_data->num_units--;

        p_unit->p_prev = NULL;
        p_unit->p_next = NULL;
        p_unit->pool = ABT_POOL_NULL;

        h_unit = (ABT_unit)p_unit;
    }
    pthread_mutex_unlock(&p_data->mutex);

    return h_unit;
}

static int pool_remove(ABT_pool pool, ABT_unit unit)
{
    ABTI_pool *p_pool = ABTI_pool_get_ptr(pool);
    void *data = ABTI_pool_get_data(p_pool);
    data_t *p_data = pool_get_data_ptr(data);
    unit_t *p_unit = (unit_t *)unit;

    ABTI_CHECK_TRUE_RET(p_data->num_units != 0, ABT_ERR_POOL);
    ABTI_CHECK_TRUE_RET(p_unit->pool != ABT_POOL_NULL, ABT_ERR_POOL);
    ABTI_CHECK_TRUE_MSG_RET(p_unit->pool == pool, ABT_ERR_POOL, "Not my pool");

    pthread_mutex_lock(&p_data->mutex);
    if (p_data->num_units == 1) {
        p_data->p_head = NULL;
        p_data->p_tail = NULL;
    } else {
        p_unit->p_prev->p_next = p_unit->p_next;
        p_unit->p_next->p_prev = p_unit->p_prev;
        if (p_unit == p_data->p_head) {
            p_data->p_head = p_unit->p_next;
        } else if (p_unit == p_data->p_tail) {
            p_data->p_tail = p_unit->p_prev;
        }
    }
    p_data->num_units--;

    p_unit->pool = ABT_POOL_NULL;
    pthread_mutex_unlock(&p_data->mutex);

    p_unit->p_prev = NULL;
    p_unit->p_next = NULL;

    return ABT_SUCCESS;
}

/* Unit functions */

static ABT_unit_type unit_get_type(ABT_unit unit)
{
   unit_t *p_unit = (unit_t *)unit;
   return p_unit->type;
}

static ABT_thread unit_get_thread(ABT_unit unit)
{
    ABT_thread h_thread;
    unit_t *p_unit = (unit_t *)unit;
    if (p_unit->type == ABT_UNIT_TYPE_THREAD) {
        h_thread = p_unit->thread;
    } else {
        h_thread = ABT_THREAD_NULL;
    }
    return h_thread;
}

static ABT_task unit_get_task(ABT_unit unit)
{
    ABT_task h_task;
    unit_t *p_unit = (unit_t *)unit;
    if (p_unit->type == ABT_UNIT_TYPE_TASK) {
        h_task = p_unit->task;
    } else {
        h_task = ABT_TASK_NULL;
    }
    return h_task;
}

static ABT_bool unit_is_in_pool(ABT_unit unit)
{
    unit_t *p_unit = (unit_t *)unit;
    return (p_unit->pool != ABT_POOL_NULL) ? ABT_TRUE : ABT_FALSE;
}

static ABT_unit unit_create_from_thread(ABT_thread thread)
{
    ABTI_thread *p_thread = ABTI_thread_get_ptr(thread);
    unit_t *p_unit = &p_thread->unit_def;
    p_unit->p_prev = NULL;
    p_unit->p_next = NULL;
    p_unit->pool   = ABT_POOL_NULL;
    p_unit->thread = thread;
    p_unit->type   = ABT_UNIT_TYPE_THREAD;

    return (ABT_unit)p_unit;
}

static ABT_unit unit_create_from_task(ABT_task task)
{
    ABTI_task *p_task = ABTI_task_get_ptr(task);
    unit_t *p_unit = &p_task->unit_def;
    p_unit->p_prev = NULL;
    p_unit->p_next = NULL;
    p_unit->pool   = ABT_POOL_NULL;
    p_unit->task   = task;
    p_unit->type   = ABT_UNIT_TYPE_TASK;

    return (ABT_unit)p_unit;
}

static void unit_free(ABT_unit *unit)
{
    *unit = ABT_UNIT_NULL;
}
