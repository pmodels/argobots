/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"


ABTI_pool *ABTI_pool_get_ptr(ABT_pool pool)
{
    ABTI_pool *p_pool;
    if (pool == ABT_POOL_NULL) {
        p_pool = NULL;
    } else {
        p_pool = (ABTI_pool *)pool;
    }
    return p_pool;
}

ABT_pool ABTI_pool_get_handle(ABTI_pool *p_pool)
{
    ABT_pool h_pool;
    if (p_pool == NULL) {
        h_pool = ABT_POOL_NULL;
    } else {
        h_pool = (ABT_pool)p_pool;
    }
    return h_pool;
}

int ABTI_pool_create(ABT_pool *newpool)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_pool *p_pool;

    p_pool = (ABTI_pool *)ABTU_malloc(sizeof(ABTI_pool));
    if (!p_pool) {
        HANDLE_ERROR("ABTU_malloc");
        *newpool = ABT_POOL_NULL;
        abt_errno = ABT_ERR_MEM;
        goto fn_fail;
    }

    p_pool->num_units = 0;
    p_pool->p_head = NULL;
    p_pool->p_tail = NULL;

    *newpool = ABTI_pool_get_handle(p_pool);

  fn_exit:
    return abt_errno;

  fn_fail:
    goto fn_exit;
}

int ABTI_pool_free(ABT_pool *pool)
{
    int abt_errno = ABT_SUCCESS;
    ABT_pool h_pool = *pool;
    ABTI_pool *p_pool;

    if (pool == NULL || h_pool == ABT_POOL_NULL) goto fn_exit;

    p_pool = ABTI_pool_get_ptr(h_pool);
    while (p_pool->num_units > 0) {
        ABT_unit unit = ABTI_pool_pop(h_pool);

        switch (ABTI_unit_get_type(unit)) {
            case ABT_UNIT_TYPE_THREAD: {
                ABT_thread h_thread = ABTI_unit_get_thread(unit);
                ABTI_thread *p_thread = ABTI_thread_get_ptr(h_thread);
                abt_errno = ABTI_thread_free(p_thread);
                if (abt_errno != ABT_SUCCESS) {
                    HANDLE_ERROR("ABTI_thread_free");
                    goto fn_fail;
                }
                break;
            }
            case ABT_UNIT_TYPE_TASK: {
                ABT_task h_task = ABTI_unit_get_task(unit);
                ABTI_task *p_task = ABTI_task_get_ptr(h_task);
                abt_errno = ABTI_task_free(p_task);
                if (abt_errno != ABT_SUCCESS) {
                    HANDLE_ERROR("ABTI_task_free");
                    goto fn_fail;
                }
                break;
            }
            case ABT_UNIT_TYPE_XSTREAM: {
                ABT_xstream h_xstream = ABTI_unit_get_xstream(unit);
                ABTI_xstream *p_xstream = ABTI_xstream_get_ptr(h_xstream);
                abt_errno = ABTI_xstream_free(p_xstream);
                if (abt_errno != ABT_SUCCESS) {
                    HANDLE_ERROR("ABTI_xstream_free");
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

size_t ABTI_pool_get_size(ABT_pool pool)
{
    ABTI_pool *p_pool = ABTI_pool_get_ptr(pool);
    return p_pool->num_units;
}

void ABTI_pool_push(ABT_pool pool, ABT_unit unit)
{
    ABTI_pool *p_pool = ABTI_pool_get_ptr(pool);
    ABTI_unit *p_unit = ABTI_unit_get_ptr(unit);

    p_unit->p_pool = p_pool;

    if (p_pool->num_units == 0) {
        p_unit->p_prev = p_unit;
        p_unit->p_next = p_unit;
        p_pool->p_head = p_unit;
        p_pool->p_tail = p_unit;
    } else {
        ABTI_unit *p_head = p_pool->p_head;
        ABTI_unit *p_tail = p_pool->p_tail;
        p_tail->p_next = p_unit;
        p_head->p_prev = p_unit;
        p_unit->p_prev = p_tail;
        p_unit->p_next = p_head;
        p_pool->p_tail = p_unit;
    }
    p_pool->num_units++;
}

ABT_unit ABTI_pool_pop(ABT_pool pool)
{
    ABTI_pool *p_pool = ABTI_pool_get_ptr(pool);
    ABTI_unit *p_unit = NULL;
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

        h_unit = ABTI_unit_get_handle(p_unit);
    }
    return h_unit;
}

void ABTI_pool_remove(ABT_pool pool, ABT_unit unit)
{
    ABTI_pool *p_pool;
    ABTI_unit *p_unit;

    p_unit = ABTI_unit_get_ptr(unit);
    if (p_unit->p_pool == NULL) return;

    p_pool = ABTI_pool_get_ptr(pool);
    if (p_pool->num_units == 0) return;

    if (p_unit->p_pool != p_pool) {
        HANDLE_ERROR("Not my pool");
    }

    if (p_pool->num_units == 1) {
        p_pool->p_head = NULL;
        p_pool->p_tail = NULL;
    } else {
        p_unit->p_prev->p_next = p_unit->p_next;
        p_unit->p_next->p_prev = p_unit->p_prev;
        if (p_unit == p_pool->p_head)
            p_pool->p_head = p_unit->p_next;
        else if (p_unit == p_pool->p_tail)
            p_pool->p_tail = p_unit->p_prev;
    }
    p_pool->num_units--;

    p_unit->p_pool = NULL;
    p_unit->p_prev = NULL;
    p_unit->p_next = NULL;
}

int ABTI_pool_print(ABT_pool pool)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_pool *p_pool;
    size_t i;

    if (pool == ABT_POOL_NULL) {
        printf("NULL POOL\n");
        goto fn_exit;
    }

    p_pool = ABTI_pool_get_ptr(pool);
    printf("num_units: %zu ", p_pool->num_units);
    printf("{ ");
    ABTI_unit *p_current = p_pool->p_head;
    for (i = 0; i < p_pool->num_units; i++) {
        if (i != 0) printf(" -> ");
        ABT_unit h_unit = ABTI_unit_get_handle(p_current);
        ABTI_unit_print(h_unit);
    }
    printf(" }\n");

  fn_exit:
    return abt_errno;
}

