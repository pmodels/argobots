/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"

static ABT_unit_type unit_get_type(ABT_unit unit);
static ABT_thread unit_get_thread(ABT_unit unit);
static ABT_task unit_get_task(ABT_unit unit);
static ABT_unit unit_create_from_thread(ABT_thread thread);
static ABT_unit unit_create_from_task(ABT_task task);
static void unit_free(ABT_unit *unit);
static size_t pool_get_size(ABT_pool pool);
static void pool_push(ABT_pool pool, ABT_unit unit);
static ABT_unit pool_pop(ABT_pool pool);
static void pool_remove(ABT_pool pool, ABT_unit unit);
static int pool_create(ABT_pool *newpool);
static int pool_free(ABT_pool *pool);


int ABTI_sched_create_lifo(ABTI_sched **p_newsched)
{
    int abt_errno = ABT_SUCCESS;
    ABT_sched sched;
    ABT_pool pool;
    ABT_sched_funcs funcs;

    /* Create a work unit pool */
    abt_errno = pool_create(&pool);
    ABTI_CHECK_ERROR(abt_errno);

    /* Set up the scheduler funtions */
    funcs.u_get_type   = unit_get_type;
    funcs.u_get_thread = unit_get_thread;
    funcs.u_get_task   = unit_get_task;
    funcs.u_create_from_thread = unit_create_from_thread;
    funcs.u_create_from_task   = unit_create_from_task;
    funcs.u_free = unit_free;
    funcs.p_get_size = pool_get_size;
    funcs.p_push     = pool_push;
    funcs.p_pop      = pool_pop;
    funcs.p_remove   = pool_remove;

    /* Create a scheduler */
    abt_errno = ABT_sched_create(pool, &funcs, &sched);
    ABTI_CHECK_ERROR(abt_errno);

    /* Set this scheduler as BASIC */
    ABTI_sched *p_sched = ABTI_sched_get_ptr(sched);
    p_sched->type = ABTI_SCHED_TYPE_BASIC;
    p_sched->kind = ABT_SCHED_LIFO;

    /* Return value */
    *p_newsched = p_sched;

  fn_exit:
    return abt_errno;

  fn_fail:
    goto fn_exit;
}

int ABTI_sched_free_lifo(ABTI_sched *p_sched)
{
    return pool_free(&p_sched->pool);
}


typedef struct unit unit_t;
typedef struct pool pool_t;

struct unit {
    pool_t *p_pool;
    ABT_unit_type type;
    union {
        ABT_thread thread;
        ABT_task   task;
    };
    unit_t *p_prev;
    unit_t *p_next;
};

struct pool {
    size_t num_units;
    unit_t *p_head;
    unit_t *p_tail;
};

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

static ABT_unit unit_create_from_thread(ABT_thread thread)
{
    unit_t *p_unit = (unit_t *)ABTU_malloc(sizeof(unit_t));
    if (!p_unit) {
        HANDLE_ERROR("ABTU_malloc");
        return ABT_UNIT_NULL;
    }

    p_unit->p_pool = NULL;
    p_unit->type   = ABT_UNIT_TYPE_THREAD;
    p_unit->thread = thread;
    p_unit->p_prev = NULL;
    p_unit->p_next = NULL;

    return (ABT_unit)p_unit;
}

static ABT_unit unit_create_from_task(ABT_task task)
{
    unit_t *p_unit = (unit_t *)ABTU_malloc(sizeof(unit_t));
    if (!p_unit) {
        HANDLE_ERROR("ABTU_malloc");
        return ABT_UNIT_NULL;
    }

    p_unit->p_pool = NULL;
    p_unit->type   = ABT_UNIT_TYPE_TASK;
    p_unit->task   = task;
    p_unit->p_prev = NULL;
    p_unit->p_next = NULL;

    return (ABT_unit)p_unit;
}

static void unit_free(ABT_unit *unit)
{
    unit_t *p_unit = (unit_t *)(*unit);
    ABTU_free(p_unit);
    *unit = ABT_UNIT_NULL;
}

static size_t pool_get_size(ABT_pool pool)
{
    pool_t *p_pool = (pool_t *)pool;
    return p_pool->num_units;
}

static void pool_push(ABT_pool pool, ABT_unit unit)
{
    pool_t *p_pool = (pool_t *)pool;
    unit_t *p_unit = (unit_t *)unit;

    p_unit->p_pool = p_pool;

    if (p_pool->num_units == 0) {
        p_unit->p_prev = p_unit;
        p_unit->p_next = p_unit;
        p_pool->p_head = p_unit;
        p_pool->p_tail = p_unit;
    } else {
        unit_t *p_head = p_pool->p_head;
        unit_t *p_tail = p_pool->p_tail;
        p_tail->p_next = p_unit;
        p_head->p_prev = p_unit;
        p_unit->p_prev = p_tail;
        p_unit->p_next = p_head;
        p_pool->p_head = p_unit;
    }
    p_pool->num_units++;
}

static ABT_unit pool_pop(ABT_pool pool)
{
    pool_t *p_pool = (pool_t *)pool;
    unit_t *p_unit = NULL;
    ABT_unit h_unit = ABT_UNIT_NULL;

    if (p_pool->num_units > 0) {
        p_unit = p_pool->p_head;
        if (p_pool->num_units == 1) {
            p_pool->p_head = NULL;
            p_pool->p_tail = NULL;
        } else {
            p_unit->p_prev->p_next = p_unit->p_next;
            p_unit->p_next->p_prev = p_unit->p_prev;
            p_pool->p_head = p_unit->p_next;
        }
        p_pool->num_units--;

        p_unit->p_pool = NULL;
        p_unit->p_prev = NULL;
        p_unit->p_next = NULL;

        h_unit = (ABT_unit)p_unit;
    }
    return h_unit;
}

static void pool_remove(ABT_pool pool, ABT_unit unit)
{
    pool_t *p_pool = (pool_t *)pool;
    unit_t *p_unit = (unit_t *)unit;

    if (p_pool->num_units == 0) return;
    if (p_unit->p_pool == NULL) return;

    if (p_unit->p_pool != p_pool) {
        HANDLE_ERROR("Not my pool");
    }

    if (p_pool->num_units == 1) {
        p_pool->p_head = NULL;
        p_pool->p_tail = NULL;
    } else {
        p_unit->p_prev->p_next = p_unit->p_next;
        p_unit->p_next->p_prev = p_unit->p_prev;
        if (p_unit == p_pool->p_head) {
            p_pool->p_head = p_unit->p_next;
        } else if (p_unit == p_pool->p_tail) {
            p_pool->p_tail = p_unit->p_prev;
        }
    }
    p_pool->num_units--;

    p_unit->p_pool = NULL;
    p_unit->p_prev = NULL;
    p_unit->p_next = NULL;
}

static int pool_create(ABT_pool *newpool)
{
    int abt_errno = ABT_SUCCESS;
    pool_t *p_pool = (pool_t *)ABTU_malloc(sizeof(pool_t));
    if (!p_pool) {
        HANDLE_ERROR("ABTU_malloc");
        *newpool = ABT_POOL_NULL;
        abt_errno = ABT_ERR_MEM;
        goto fn_fail;
    }

    p_pool->num_units = 0;
    p_pool->p_head = NULL;
    p_pool->p_tail = NULL;

    *newpool = (ABT_pool)p_pool;

  fn_exit:
    return abt_errno;

  fn_fail:
    goto fn_exit;
}

static int pool_free(ABT_pool *pool)
{
    int abt_errno = ABT_SUCCESS;
    ABT_pool h_pool = *pool;
    pool_t *p_pool = (pool_t *)h_pool;

    if (p_pool == NULL || h_pool == ABT_POOL_NULL) goto fn_exit;

    while (p_pool->num_units > 0) {
        unit_t *p_unit = (unit_t *)pool_pop(h_pool);

        switch (p_unit->type) {
            case ABT_UNIT_TYPE_THREAD: {
                ABT_thread h_thread = p_unit->thread;
                ABTI_thread *p_thread = ABTI_thread_get_ptr(h_thread);
                abt_errno = ABTI_thread_free(p_thread);
                if (abt_errno != ABT_SUCCESS) {
                    HANDLE_ERROR("ABTI_thread_free");
                    goto fn_fail;
                }
                break;
            }
            case ABT_UNIT_TYPE_TASK: {
                ABT_task h_task = p_unit->task;
                ABTI_task *p_task = ABTI_task_get_ptr(h_task);
                abt_errno = ABTI_task_free(p_task);
                if (abt_errno != ABT_SUCCESS) {
                    HANDLE_ERROR("ABTI_task_free");
                    goto fn_fail;
                }
                break;
            }
            default:
                HANDLE_ERROR("Unknown unit type");
                break;
        }

    }

    ABTU_free(p_pool);

    *pool = ABT_POOL_NULL;

  fn_exit:
    return abt_errno;

  fn_fail:
    goto fn_exit;
}



