/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"

ABT_Unit_type ABTI_Unit_get_type(ABT_Unit unit)
{
    ABTI_Unit *p_unit = ABTI_Unit_get_ptr(unit);
    return p_unit->type;
}

ABT_Stream ABTI_Unit_get_stream(ABT_Unit unit)
{
    ABTI_Unit *p_unit = ABTI_Unit_get_ptr(unit);
    if (p_unit->type == ABT_UNIT_TYPE_OTHER) {
        ABTI_Stream *p_stream = (ABTI_Stream *)p_unit->p_unit;
        return ABTI_Stream_get_handle(p_stream);
    } else {
        return ABT_STREAM_NULL;
    }
}

ABT_Thread ABTI_Unit_get_thread(ABT_Unit unit)
{
    ABTI_Unit *p_unit = ABTI_Unit_get_ptr(unit);
    if (p_unit->type == ABT_UNIT_TYPE_THREAD) {
        ABTI_Thread *p_thread = (ABTI_Thread *)p_unit->p_unit;
        return ABTI_Thread_get_handle(p_thread);
    } else {
        return ABT_THREAD_NULL;
    }
}

ABT_Task ABTI_Unit_get_task(ABT_Unit unit)
{
    ABTI_Unit *p_unit = ABTI_Unit_get_ptr(unit);
    if (p_unit->type == ABT_UNIT_TYPE_TASK) {
        ABTI_Task *p_task = (ABTI_Task *)p_unit->p_unit;
        return ABTI_Task_get_handle(p_task);
    } else {
        return ABT_TASK_NULL;
    }
}

ABT_Unit ABTI_Unit_create_from_stream(ABT_Stream stream)
{
    ABTI_Unit *p_unit;

    p_unit = (ABTI_Unit *)ABTU_Malloc(sizeof(ABTI_Unit));
    if (!p_unit) {
        HANDLE_ERROR("ABTU_Malloc");
        return ABT_UNIT_NULL;
    }

    p_unit->p_pool = NULL;
    p_unit->type   = ABT_UNIT_TYPE_OTHER;
    p_unit->p_unit = (void *)ABTI_Stream_get_ptr(stream);
    p_unit->p_prev = NULL;
    p_unit->p_next = NULL;

    return ABTI_Unit_get_handle(p_unit);
}

ABT_Unit ABTI_Unit_create_from_thread(ABT_Thread thread)
{
    ABTI_Unit *p_unit;

    p_unit = (ABTI_Unit *)ABTU_Malloc(sizeof(ABTI_Unit));
    if (!p_unit) {
        HANDLE_ERROR("ABTU_Malloc");
        return ABT_UNIT_NULL;
    }

    p_unit->p_pool = NULL;
    p_unit->type   = ABT_UNIT_TYPE_THREAD;
    p_unit->p_unit = (void *)ABTI_Thread_get_ptr(thread);
    p_unit->p_prev = NULL;
    p_unit->p_next = NULL;

    return ABTI_Unit_get_handle(p_unit);
}

ABT_Unit ABTI_Unit_create_from_task(ABT_Task task)
{
    ABTI_Unit *p_unit;

    p_unit = (ABTI_Unit *)ABTU_Malloc(sizeof(ABTI_Unit));
    if (!p_unit) {
        HANDLE_ERROR("ABTU_Malloc");
        return ABT_UNIT_NULL;
    }

    p_unit->p_pool = NULL;
    p_unit->type   = ABT_UNIT_TYPE_TASK;
    p_unit->p_unit = (void *)ABTI_Task_get_ptr(task);
    p_unit->p_prev = NULL;
    p_unit->p_next = NULL;

    return ABTI_Unit_get_handle(p_unit);
}

void ABTI_Unit_free(ABT_Unit unit)
{
    ABTI_Unit *p_unit = ABTI_Unit_get_ptr(unit);
    ABTU_Free(p_unit);
}

