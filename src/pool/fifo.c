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

typedef ABTI_thread unit_t;
static ABT_unit_type unit_get_type(ABT_unit unit);
static ABT_thread unit_get_thread(ABT_unit unit);
static ABT_task unit_get_task(ABT_unit unit);
static ABT_bool unit_is_in_pool(ABT_unit unit);
static ABT_unit unit_create_from_thread(ABT_thread thread);
static ABT_unit unit_create_from_task(ABT_task task);
static void unit_free(ABT_unit *unit);

struct data {
    ABTI_spinlock mutex;
    size_t num_units;
    unit_t *p_head;
    unit_t *p_tail;
};
typedef struct data data_t;

static inline data_t *pool_get_data_ptr(void *p_data)
{
    return (data_t *)p_data;
}

/* Obtain the FIFO pool definition according to the access type */
int ABTI_pool_get_fifo_def(ABT_pool_access access, ABT_pool_def *p_def)
{
    int abt_errno = ABT_SUCCESS;

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
            ABTI_CHECK_TRUE(0, ABT_ERR_INV_POOL_ACCESS);
    }

    /* Common definitions regardless of the access type */
    p_def->access = access;
    p_def->p_init = pool_init;
    p_def->p_free = pool_free;
    p_def->p_get_size = pool_get_size;
    p_def->p_pop_wait = pool_pop_wait;
    p_def->p_pop_timedwait = pool_pop_timedwait;
    p_def->p_print_all = pool_print_all;
    p_def->u_get_type = unit_get_type;
    p_def->u_get_thread = unit_get_thread;
    p_def->u_get_task = unit_get_task;
    p_def->u_is_in_pool = unit_is_in_pool;
    p_def->u_create_from_thread = unit_create_from_thread;
    p_def->u_create_from_task = unit_create_from_task;
    p_def->u_free = unit_free;

fn_exit:
    return abt_errno;

fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

/* Pool functions */

int pool_init(ABT_pool pool, ABT_pool_config config)
{
    ABTI_UNUSED(config);
    int abt_errno = ABT_SUCCESS;
    ABTI_pool *p_pool = ABTI_pool_get_ptr(pool);
    ABT_pool_access access;

    data_t *p_data = (data_t *)ABTU_malloc(sizeof(data_t));

    access = p_pool->access;

    if (access != ABT_POOL_ACCESS_PRIV) {
        /* Initialize the mutex */
        ABTI_spinlock_clear(&p_data->mutex);
    }

    p_data->num_units = 0;
    p_data->p_head = NULL;
    p_data->p_tail = NULL;

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
    return p_data->num_units;
}

static void pool_push_shared(ABT_pool pool, ABT_unit unit)
{
    ABTI_pool *p_pool = ABTI_pool_get_ptr(pool);
    data_t *p_data = pool_get_data_ptr(p_pool->data);
    unit_t *p_unit = (unit_t *)unit;

    ABTI_spinlock_acquire(&p_data->mutex);
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

    ABTD_atomic_release_store_int(&p_unit->is_in_pool, 1);
    ABTI_spinlock_release(&p_data->mutex);
}

static void pool_push_private(ABT_pool pool, ABT_unit unit)
{
    ABTI_pool *p_pool = ABTI_pool_get_ptr(pool);
    data_t *p_data = pool_get_data_ptr(p_pool->data);
    unit_t *p_unit = (unit_t *)unit;

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

    ABTD_atomic_release_store_int(&p_unit->is_in_pool, 1);
}

static ABT_unit pool_pop_wait(ABT_pool pool, double time_secs)
{
    ABTI_pool *p_pool = ABTI_pool_get_ptr(pool);
    data_t *p_data = pool_get_data_ptr(p_pool->data);
    unit_t *p_unit = NULL;
    ABT_unit h_unit = ABT_UNIT_NULL;

    double time_start = 0.0;

    do {
        ABTI_spinlock_acquire(&p_data->mutex);
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
            ABTD_atomic_release_store_int(&p_unit->is_in_pool, 0);

            h_unit = (ABT_unit)p_unit;
            ABTI_spinlock_release(&p_data->mutex);
        } else {
            ABTI_spinlock_release(&p_data->mutex);
            if (time_start == 0.0) {
                time_start = ABTI_get_wtime();
            } else {
                double elapsed = ABTI_get_wtime() - time_start;
                if (elapsed > time_secs)
                    break;
            }
            /* Sleep. */
            const int sleep_nsecs = 100;
            struct timespec ts = { 0, sleep_nsecs };
            nanosleep(&ts, NULL);
        }
    } while (h_unit == ABT_UNIT_NULL);

    return h_unit;
}

static ABT_unit pool_pop_timedwait(ABT_pool pool, double abstime_secs)
{
    ABTI_pool *p_pool = ABTI_pool_get_ptr(pool);
    data_t *p_data = pool_get_data_ptr(p_pool->data);
    unit_t *p_unit = NULL;
    ABT_unit h_unit = ABT_UNIT_NULL;

    do {
        ABTI_spinlock_acquire(&p_data->mutex);
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
            ABTD_atomic_release_store_int(&p_unit->is_in_pool, 0);

            h_unit = (ABT_unit)p_unit;
            ABTI_spinlock_release(&p_data->mutex);
        } else {
            ABTI_spinlock_release(&p_data->mutex);
            /* Sleep. */
            const int sleep_nsecs = 100;
            struct timespec ts = { 0, sleep_nsecs };
            nanosleep(&ts, NULL);

            if (ABTI_get_wtime() > abstime_secs)
                break;
        }
    } while (h_unit == ABT_UNIT_NULL);

    return h_unit;
}

static ABT_unit pool_pop_shared(ABT_pool pool)
{
    ABTI_pool *p_pool = ABTI_pool_get_ptr(pool);
    data_t *p_data = pool_get_data_ptr(p_pool->data);
    unit_t *p_unit = NULL;
    ABT_unit h_unit = ABT_UNIT_NULL;

    ABTI_spinlock_acquire(&p_data->mutex);
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
        ABTD_atomic_release_store_int(&p_unit->is_in_pool, 0);

        h_unit = (ABT_unit)p_unit;
    }
    ABTI_spinlock_release(&p_data->mutex);

    return h_unit;
}

static ABT_unit pool_pop_private(ABT_pool pool)
{
    ABTI_pool *p_pool = ABTI_pool_get_ptr(pool);
    data_t *p_data = pool_get_data_ptr(p_pool->data);
    unit_t *p_unit = NULL;
    ABT_unit h_unit = ABT_UNIT_NULL;

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
        ABTD_atomic_release_store_int(&p_unit->is_in_pool, 0);

        h_unit = (ABT_unit)p_unit;
    }

    return h_unit;
}

static int pool_remove_shared(ABT_pool pool, ABT_unit unit)
{
    ABTI_pool *p_pool = ABTI_pool_get_ptr(pool);
    data_t *p_data = pool_get_data_ptr(p_pool->data);
    unit_t *p_unit = (unit_t *)unit;

    ABTI_CHECK_TRUE_RET(p_data->num_units != 0, ABT_ERR_POOL);
    ABTI_CHECK_TRUE_RET(ABTD_atomic_acquire_load_int(&p_unit->is_in_pool) == 1,
                        ABT_ERR_POOL);

    ABTI_spinlock_acquire(&p_data->mutex);
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

    ABTD_atomic_release_store_int(&p_unit->is_in_pool, 0);
    ABTI_spinlock_release(&p_data->mutex);

    p_unit->p_prev = NULL;
    p_unit->p_next = NULL;

    return ABT_SUCCESS;
}

static int pool_remove_private(ABT_pool pool, ABT_unit unit)
{
    ABTI_pool *p_pool = ABTI_pool_get_ptr(pool);
    data_t *p_data = pool_get_data_ptr(p_pool->data);
    unit_t *p_unit = (unit_t *)unit;

    ABTI_CHECK_TRUE_RET(p_data->num_units != 0, ABT_ERR_POOL);
    ABTI_CHECK_TRUE_RET(ABTD_atomic_acquire_load_int(&p_unit->is_in_pool) == 1,
                        ABT_ERR_POOL);

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

    ABTD_atomic_release_store_int(&p_unit->is_in_pool, 0);
    p_unit->p_prev = NULL;
    p_unit->p_next = NULL;

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
        ABTI_spinlock_acquire(&p_data->mutex);
    }

    size_t num_units = p_data->num_units;
    unit_t *p_unit = p_data->p_head;
    while (num_units--) {
        ABTI_ASSERT(p_unit);
        ABT_unit unit = (ABT_unit)p_unit;
        print_fn(arg, unit);
        p_unit = p_unit->p_next;
    }

    if (access != ABT_POOL_ACCESS_PRIV) {
        ABTI_spinlock_release(&p_data->mutex);
    }

    return ABT_SUCCESS;
}

/* Unit functions */

static ABT_unit_type unit_get_type(ABT_unit unit)
{
    unit_t *p_unit = (unit_t *)unit;
    return ABTI_unit_type_get_type(p_unit->type);
}

static ABT_thread unit_get_thread(ABT_unit unit)
{
    ABT_thread h_thread;
    unit_t *p_unit = (unit_t *)unit;
    if (ABTI_unit_type_is_thread(p_unit->type)) {
        h_thread = ABTI_thread_get_handle(ABTI_unit_get_thread(p_unit));
    } else {
        h_thread = ABT_THREAD_NULL;
    }
    return h_thread;
}

static ABT_task unit_get_task(ABT_unit unit)
{
    ABT_task h_task;
    unit_t *p_unit = (unit_t *)unit;
    if (p_unit->type == ABTI_UNIT_TYPE_TASK) {
        h_task = ABTI_task_get_handle(ABTI_unit_get_task(p_unit));
    } else {
        h_task = ABT_TASK_NULL;
    }
    return h_task;
}

static ABT_bool unit_is_in_pool(ABT_unit unit)
{
    unit_t *p_unit = (unit_t *)unit;
    return ABTD_atomic_acquire_load_int(&p_unit->is_in_pool) ? ABT_TRUE
                                                             : ABT_FALSE;
}

static ABT_unit unit_create_from_thread(ABT_thread thread)
{
    ABTI_thread *p_thread = ABTI_thread_get_ptr(thread);
    unit_t *p_unit = p_thread;
    p_unit->p_prev = NULL;
    p_unit->p_next = NULL;
    ABTD_atomic_relaxed_store_int(&p_unit->is_in_pool, 0);
    ABTI_ASSERT(ABTI_unit_type_is_thread(p_unit->type));

    return (ABT_unit)p_unit;
}

static ABT_unit unit_create_from_task(ABT_task task)
{
    ABTI_thread *p_task = ABTI_task_get_ptr(task);
    unit_t *p_unit = p_task;
    p_unit->p_prev = NULL;
    p_unit->p_next = NULL;
    ABTD_atomic_relaxed_store_int(&p_unit->is_in_pool, 0);
    ABTI_ASSERT(p_unit->type == ABTI_UNIT_TYPE_TASK);

    return (ABT_unit)p_unit;
}

static void unit_free(ABT_unit *unit)
{
    *unit = ABT_UNIT_NULL;
}
