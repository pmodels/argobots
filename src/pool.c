/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"


ABTI_Pool *ABTI_Pool_get_ptr(ABT_Pool pool)
{
    ABTI_Pool *p_pool;
    if (pool == ABT_POOL_NULL) {
        p_pool = NULL;
    } else {
        p_pool = (ABTI_Pool *)pool;
    }
    return p_pool;
}

ABT_Pool ABTI_Pool_get_handle(ABTI_Pool *p_pool)
{
    ABT_Pool h_pool;
    if (p_pool == NULL) {
        h_pool = ABT_POOL_NULL;
    } else {
        h_pool = (ABT_Pool)p_pool;
    }
    return h_pool;
}

int ABTI_Pool_create(ABT_Pool *newpool)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_Pool *p_pool;

    p_pool = (ABTI_Pool *)ABTU_Malloc(sizeof(ABTI_Pool));
    if (!p_pool) {
        HANDLE_ERROR("ABTU_Malloc");
        *newpool = NULL;
        abt_errno = ABT_ERR_MEM;
        goto fn_fail;
    }

    p_pool->num_units = 0;
    p_pool->p_head = NULL;
    p_pool->p_tail = NULL;

    *newpool = ABTI_Pool_get_handle(p_pool);

  fn_exit:
    return abt_errno;

  fn_fail:
    goto fn_exit;
}

int ABTI_Pool_free(ABT_Pool *pool)
{
    int abt_errno = ABT_SUCCESS;
    ABT_Pool h_pool = *pool;
    ABTI_Pool *p_pool;

    if (pool == NULL || h_pool == ABT_POOL_NULL) goto fn_exit;

    p_pool = ABTI_Pool_get_ptr(h_pool);
    while (p_pool->num_units > 0) {
        ABT_Unit unit = ABTI_Pool_pop(h_pool);

        switch (ABTI_Unit_get_type(unit)) {
            case ABT_UNIT_TYPE_THREAD: {
                ABT_Thread h_thread = ABTI_Unit_get_thread(unit);
                ABTI_Thread *p_thread = ABTI_Thread_get_ptr(h_thread);
                abt_errno = ABTI_Thread_free(p_thread);
                if (abt_errno != ABT_SUCCESS) {
                    HANDLE_ERROR("ABTI_Thread_free");
                    goto fn_fail;
                }
                break;
            }
            case ABT_UNIT_TYPE_TASK: {
                ABT_Task h_task = ABTI_Unit_get_task(unit);
                ABTI_Task *p_task = ABTI_Task_get_ptr(h_task);
                abt_errno = ABTI_Task_free(p_task);
                if (abt_errno != ABT_SUCCESS) {
                    HANDLE_ERROR("ABTI_Task_free");
                    goto fn_fail;
                }
                break;
            }
            case ABT_UNIT_TYPE_OTHER: {
                ABT_Stream h_stream = ABTI_Unit_get_stream(unit);
                ABTI_Stream *p_stream = ABTI_Stream_get_ptr(h_stream);
                abt_errno = ABTI_Stream_free(p_stream);
                if (abt_errno != ABT_SUCCESS) {
                    HANDLE_ERROR("ABTI_Stream_free");
                    goto fn_fail;
                }
                break;
            }
            default:
                HANDLE_ERROR("Unknown unit type");
                break;
        }

    }

    ABTU_Free(p_pool);

    *pool = ABT_POOL_NULL;

  fn_exit:
    return abt_errno;

  fn_fail:
    goto fn_exit;
}

size_t ABTI_Pool_get_size(ABT_Pool pool)
{
    ABTI_Pool *p_pool = ABTI_Pool_get_ptr(pool);
    return p_pool->num_units;
}

void ABTI_Pool_push(ABT_Pool pool, ABT_Unit unit)
{
    ABTI_Pool *p_pool = ABTI_Pool_get_ptr(pool);
    ABTI_Unit *p_unit = ABTI_Unit_get_ptr(unit);

    p_unit->p_pool = p_pool;

    if (p_pool->num_units == 0) {
        p_unit->p_prev = p_unit;
        p_unit->p_next = p_unit;
        p_pool->p_head = p_unit;
        p_pool->p_tail = p_unit;
    } else {
        ABTI_Unit *p_head = p_pool->p_head;
        ABTI_Unit *p_tail = p_pool->p_tail;
        p_tail->p_next = p_unit;
        p_head->p_prev = p_unit;
        p_unit->p_prev = p_tail;
        p_unit->p_next = p_head;
        p_pool->p_tail = p_unit;
    }
    p_pool->num_units++;
}

ABT_Unit ABTI_Pool_pop(ABT_Pool pool)
{
    ABTI_Pool *p_pool = ABTI_Pool_get_ptr(pool);
    ABTI_Unit *p_unit = NULL;
    ABT_Unit h_unit = ABT_UNIT_NULL;

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

        h_unit = ABTI_Unit_get_handle(p_unit);
    }
    return h_unit;
}

void ABTI_Pool_remove(ABT_Pool pool, ABT_Unit unit)
{
    ABTI_Pool *p_pool;
    ABTI_Unit *p_unit;

    p_unit = ABTI_Unit_get_ptr(unit);
    if (p_unit->p_pool == NULL) return;

    p_pool = ABTI_Pool_get_ptr(pool);
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

int ABTI_Pool_print(ABT_Pool pool)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_Pool *p_pool;
    size_t i;

    if (pool == ABT_POOL_NULL) {
        printf("NULL POOL\n");
        goto fn_exit;
    }

    p_pool = ABTI_Pool_get_ptr(pool);
    printf("num_units: %lu ", p_pool->num_units);
    printf("{ ");
    ABTI_Unit *p_current = p_pool->p_head;
    for (i = 0; i < p_pool->num_units; i++) {
        if (i != 0) printf(" -> ");
        ABT_Unit h_unit = ABTI_Unit_get_handle(p_current);
        ABTI_Unit_print(h_unit);
    }
    printf(" }\n");

  fn_exit:
    return abt_errno;
}

