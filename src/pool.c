/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"

int ABTI_Pool_create(ABTD_Pool **newpool)
{
    int abt_errno = ABT_SUCCESS;
    ABTD_Pool *pool;

    pool = (ABTD_Pool *)ABTU_Malloc(sizeof(ABTD_Pool));
    if (!pool) {
        HANDLE_ERROR("ABTU_Malloc");
        *newpool = NULL;
        abt_errno = ABT_ERR_MEM;
        goto fn_fail;
    }

    pool->num_units = 0;
    pool->head = NULL;
    pool->tail = NULL;

    *newpool = pool;

  fn_exit:
    return abt_errno;

  fn_fail:
    goto fn_exit;
}

int ABTI_Pool_free(ABTD_Pool *pool)
{
    int abt_errno = ABT_SUCCESS;

    if (pool != NULL) {
        /* NOTE: pool should be empty. */
        assert(pool->num_units == 0);

        free(pool);
    }

    return abt_errno;
}

size_t ABTI_Pool_get_size(ABT_Pool pool)
{
    ABTD_Pool *pool_ptr = ABTI_Pool_get_ptr(pool);
    return pool_ptr->num_units;
}

void ABTI_Pool_push(ABT_Pool pool, ABT_Unit unit)
{
    ABTD_Pool *pool_ptr = ABTI_Pool_get_ptr(pool);
    ABTD_Unit *unit_ptr = ABTI_Unit_get_ptr(unit);

    unit_ptr->pool = pool_ptr;

    if (pool_ptr->num_units == 0) {
        unit_ptr->prev = unit_ptr;
        unit_ptr->next = unit_ptr;
        pool_ptr->head = unit_ptr;
        pool_ptr->tail = unit_ptr;
    } else {
        ABTD_Unit *head = pool_ptr->head;
        ABTD_Unit *tail = pool_ptr->tail;
        tail->next = unit_ptr;
        head->prev = unit_ptr;
        unit_ptr->prev = tail;
        unit_ptr->next = head;
        pool_ptr->tail = unit_ptr;
    }
    pool_ptr->num_units++;
}

ABT_Unit ABTI_Pool_pop(ABT_Pool pool)
{
    ABTD_Pool *pool_ptr = ABTI_Pool_get_ptr(pool);
    ABTD_Unit *unit_ptr = NULL;

    if (pool_ptr->num_units > 0) {
        unit_ptr = pool_ptr->head;
        if (pool_ptr->num_units == 1) {
            pool_ptr->head = NULL;
            pool_ptr->tail = NULL;
        } else {
            unit_ptr->prev->next = unit_ptr->next;
            unit_ptr->next->prev = unit_ptr->prev;
            pool_ptr->head = unit_ptr->next;
        }
        pool_ptr->num_units--;

        unit_ptr->pool = NULL;
        unit_ptr->prev = NULL;
        unit_ptr->next = NULL;
    }
    return ABTI_Unit_get_handle(unit_ptr);
}

void ABTI_Pool_remove(ABT_Pool pool, ABT_Unit unit)
{
    ABTD_Pool *pool_ptr;
    ABTD_Unit *unit_ptr;

    pool_ptr = ABTI_Pool_get_ptr(pool);
    if (pool_ptr->num_units == 0) return;

    unit_ptr = ABTI_Unit_get_ptr(unit);
    if (unit_ptr->pool != pool_ptr) {
        HANDLE_ERROR("Not my pool");
    }

    if (pool_ptr->num_units == 1) {
        pool_ptr->head = NULL;
        pool_ptr->tail = NULL;
    } else {
        unit_ptr->prev->next = unit_ptr->next;
        unit_ptr->next->prev = unit_ptr->prev;
        if (unit_ptr == pool_ptr->head)
            pool_ptr->head = unit_ptr->next;
        else if (unit_ptr == pool_ptr->tail)
            pool_ptr->tail = unit_ptr->prev;
    }
    pool_ptr->num_units--;

    unit_ptr->pool = NULL;
    unit_ptr->prev = NULL;
    unit_ptr->next = NULL;
}

