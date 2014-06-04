/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"


ABTI_Unit *ABTI_Unit_get_ptr(ABT_Unit unit)
{
    ABTI_Unit *p_unit;
    if (unit == ABT_UNIT_NULL) {
        p_unit = NULL;
    } else {
        p_unit = (ABTI_Unit *)unit;
    }
    return p_unit;
}

ABT_Unit ABTI_Unit_get_handle(ABTI_Unit *p_unit)
{
    ABT_Unit h_unit;
    if (p_unit == NULL) {
        h_unit = ABT_UNIT_NULL;
    } else {
        h_unit = (ABT_Unit)p_unit;
    }
    return h_unit;
}

ABT_Unit_type ABTI_Unit_get_type(ABT_Unit unit)
{
    ABTI_Unit *p_unit = ABTI_Unit_get_ptr(unit);
    return p_unit->type;
}

ABT_Stream ABTI_Unit_get_stream(ABT_Unit unit)
{
    ABT_Stream h_stream;
    ABTI_Unit *p_unit = ABTI_Unit_get_ptr(unit);
    if (p_unit->type == ABT_UNIT_TYPE_OTHER) {
        ABTI_Stream *p_stream = (ABTI_Stream *)p_unit->p_unit;
        h_stream = ABTI_Stream_get_handle(p_stream);
    } else {
        h_stream = ABT_STREAM_NULL;
    }
    return h_stream;
}

ABT_Thread ABTI_Unit_get_thread(ABT_Unit unit)
{
    ABT_Thread h_thread;
    ABTI_Unit *p_unit = ABTI_Unit_get_ptr(unit);
    if (p_unit->type == ABT_UNIT_TYPE_THREAD) {
        ABTI_Thread *p_thread = (ABTI_Thread *)p_unit->p_unit;
        h_thread = ABTI_Thread_get_handle(p_thread);
    } else {
        h_thread = ABT_THREAD_NULL;
    }
    return h_thread;
}

ABT_Task ABTI_Unit_get_task(ABT_Unit unit)
{
    ABT_Task h_task;
    ABTI_Unit *p_unit = ABTI_Unit_get_ptr(unit);
    if (p_unit->type == ABT_UNIT_TYPE_TASK) {
        ABTI_Task *p_task = (ABTI_Task *)p_unit->p_unit;
        h_task = ABTI_Task_get_handle(p_task);
    } else {
        h_task = ABT_TASK_NULL;
    }
    return h_task;
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

void ABTI_Unit_free(ABT_Unit *unit)
{
    ABTI_Unit *p_unit = ABTI_Unit_get_ptr(*unit);
    ABTU_Free(p_unit);
    *unit = ABT_UNIT_NULL;
}

int ABTI_Unit_print(ABT_Unit unit)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_Unit *p_unit = ABTI_Unit_get_ptr(unit);

    printf("<");
    printf("pool:%p ", p_unit->p_pool);
    printf("type:");
    switch (p_unit->type) {
        case ABT_UNIT_TYPE_THREAD:
            printf("thread");
            ABTI_Thread *p_thread = (ABTI_Thread *)p_unit->p_unit;
            ABTI_Thread_print(p_thread);
            break;

        case ABT_UNIT_TYPE_TASK:
            printf("task");
            ABTI_Task *p_task = (ABTI_Task *)p_unit->p_unit;
            ABTI_Task_print(p_task);
            break;

        case ABT_UNIT_TYPE_OTHER:
            printf("other");
            break;

        default:
            printf("unknown");
    }
    printf(">");

    return abt_errno;
}

