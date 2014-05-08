/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"

ABT_Unit_type ABTI_Unit_get_type(ABT_Unit unit)
{
    ABTD_Unit *unit_ptr = ABTI_Unit_get_ptr(unit);
    return unit_ptr->type;
}

ABT_Thread ABTI_Unit_get_thread(ABT_Unit unit)
{
    ABTD_Unit *unit_ptr = ABTI_Unit_get_ptr(unit);
    if (unit_ptr->type == ABT_UNIT_TYPE_THREAD) {
        ABTD_Thread *thread_ptr = (ABTD_Thread *)unit_ptr->unit;
        return ABTI_Thread_get_handle(thread_ptr);
    } else {
        return ABT_THREAD_NULL;
    }
}

ABT_Task ABTI_Unit_get_task(ABT_Unit unit)
{
    /* TODO */
    ABTD_Unit *unit_ptr = ABTI_Unit_get_ptr(unit);
    return (ABT_Task)(unit_ptr->unit);
}

ABT_Unit ABTI_Unit_create_from_thread(ABT_Thread thread)
{
    ABTD_Unit *unit_ptr;

    unit_ptr = (ABTD_Unit *)ABTU_Malloc(sizeof(ABTD_Unit));
    if (!unit_ptr) {
        HANDLE_ERROR("ABTU_Malloc");
        return ABT_UNIT_NULL;
    }

    unit_ptr->pool = NULL;
    unit_ptr->type = ABT_UNIT_TYPE_THREAD;
    unit_ptr->unit = (void *)ABTI_Thread_get_ptr(thread);
    unit_ptr->prev = NULL;
    unit_ptr->next = NULL;

    return ABTI_Unit_get_handle(unit_ptr);
}

ABT_Unit ABTI_Unit_create_from_task(ABT_Task task)
{
    /* TODO */
    return ABT_UNIT_NULL;
}

void ABTI_Unit_free(ABT_Unit unit)
{
    ABTD_Unit *unit_ptr = ABTI_Unit_get_ptr(unit);
    free(unit_ptr);
}

