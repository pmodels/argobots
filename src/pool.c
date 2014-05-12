/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"

int ABTI_Pool_create(ABTI_Pool **newpool)
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

    *newpool = p_pool;

  fn_exit:
    return abt_errno;

  fn_fail:
    goto fn_exit;
}

int ABTI_Pool_free(ABTI_Pool *p_pool)
{
    int abt_errno = ABT_SUCCESS;

    if (p_pool != NULL) {
        /* NOTE: pool should be empty. */
        assert(p_pool->num_units == 0);

        ABTU_Free(p_pool);
    }

    return abt_errno;
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
    }
    return ABTI_Unit_get_handle(p_unit);
}

void ABTI_Pool_remove(ABT_Pool pool, ABT_Unit unit)
{
    ABTI_Pool *p_pool;
    ABTI_Unit *p_unit;

    p_pool = ABTI_Pool_get_ptr(pool);
    if (p_pool->num_units == 0) return;

    p_unit = ABTI_Unit_get_ptr(unit);
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

