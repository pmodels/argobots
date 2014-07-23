/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"


ABTI_unit *ABTI_unit_get_ptr(ABT_unit unit)
{
    ABTI_unit *p_unit;
    if (unit == ABT_UNIT_NULL) {
        p_unit = NULL;
    } else {
        p_unit = (ABTI_unit *)unit;
    }
    return p_unit;
}

ABT_unit ABTI_unit_get_handle(ABTI_unit *p_unit)
{
    ABT_unit h_unit;
    if (p_unit == NULL) {
        h_unit = ABT_UNIT_NULL;
    } else {
        h_unit = (ABT_unit)p_unit;
    }
    return h_unit;
}

ABT_unit_type ABTI_unit_get_type(ABT_unit unit)
{
    ABTI_unit *p_unit = ABTI_unit_get_ptr(unit);
    return p_unit->type;
}

ABT_xstream ABTI_unit_get_xstream(ABT_unit unit)
{
    ABT_xstream h_xstream;
    ABTI_unit *p_unit = ABTI_unit_get_ptr(unit);
    if (p_unit->type == ABT_UNIT_TYPE_OTHER) {
        ABTI_xstream *p_xstream = (ABTI_xstream *)p_unit->p_unit;
        h_xstream = ABTI_xstream_get_handle(p_xstream);
    } else {
        h_xstream = ABT_XSTREAM_NULL;
    }
    return h_xstream;
}

ABT_thread ABTI_unit_get_thread(ABT_unit unit)
{
    ABT_thread h_thread;
    ABTI_unit *p_unit = ABTI_unit_get_ptr(unit);
    if (p_unit->type == ABT_UNIT_TYPE_THREAD) {
        ABTI_thread *p_thread = (ABTI_thread *)p_unit->p_unit;
        h_thread = ABTI_thread_get_handle(p_thread);
    } else {
        h_thread = ABT_THREAD_NULL;
    }
    return h_thread;
}

ABT_task ABTI_unit_get_task(ABT_unit unit)
{
    ABT_task h_task;
    ABTI_unit *p_unit = ABTI_unit_get_ptr(unit);
    if (p_unit != NULL && p_unit->type == ABT_UNIT_TYPE_TASK) {
        ABTI_task *p_task = (ABTI_task *)p_unit->p_unit;
        h_task = ABTI_task_get_handle(p_task);
    } else {
        h_task = ABT_TASK_NULL;
    }
    return h_task;
}

ABT_unit ABTI_unit_get_next(ABT_unit unit)
{
    ABTI_unit *p_unit = ABTI_unit_get_ptr(unit);
    ABTI_unit *p_next = p_unit->p_next;
    return ABTI_unit_get_handle(p_next);
}

ABT_unit ABTI_unit_create_from_xstream(ABT_xstream xstream)
{
    ABTI_unit *p_unit;

    p_unit = (ABTI_unit *)ABTU_malloc(sizeof(ABTI_unit));
    if (!p_unit) {
        HANDLE_ERROR("ABTU_malloc");
        return ABT_UNIT_NULL;
    }

    p_unit->p_pool = NULL;
    p_unit->type   = ABT_UNIT_TYPE_OTHER;
    p_unit->p_unit = (void *)ABTI_xstream_get_ptr(xstream);
    p_unit->p_prev = NULL;
    p_unit->p_next = NULL;

    return ABTI_unit_get_handle(p_unit);
}

ABT_unit ABTI_unit_create_from_thread(ABT_thread thread)
{
    ABTI_unit *p_unit;

    p_unit = (ABTI_unit *)ABTU_malloc(sizeof(ABTI_unit));
    if (!p_unit) {
        HANDLE_ERROR("ABTU_malloc");
        return ABT_UNIT_NULL;
    }

    p_unit->p_pool = NULL;
    p_unit->type   = ABT_UNIT_TYPE_THREAD;
    p_unit->p_unit = (void *)ABTI_thread_get_ptr(thread);
    p_unit->p_prev = NULL;
    p_unit->p_next = NULL;

    return ABTI_unit_get_handle(p_unit);
}

ABT_unit ABTI_unit_create_from_task(ABT_task task)
{
    ABTI_unit *p_unit;

    p_unit = (ABTI_unit *)ABTU_malloc(sizeof(ABTI_unit));
    if (!p_unit) {
        HANDLE_ERROR("ABTU_malloc");
        return ABT_UNIT_NULL;
    }

    p_unit->p_pool = NULL;
    p_unit->type   = ABT_UNIT_TYPE_TASK;
    p_unit->p_unit = (void *)ABTI_task_get_ptr(task);
    p_unit->p_prev = NULL;
    p_unit->p_next = NULL;

    return ABTI_unit_get_handle(p_unit);
}

void ABTI_unit_free(ABT_unit *unit)
{
    ABTI_unit *p_unit = ABTI_unit_get_ptr(*unit);
    ABTU_free(p_unit);
    *unit = ABT_UNIT_NULL;
}

int ABTI_unit_print(ABT_unit unit)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_unit *p_unit = ABTI_unit_get_ptr(unit);

    printf("<");
    printf("pool:%p ", p_unit->p_pool);
    printf("type:");
    switch (p_unit->type) {
        case ABT_UNIT_TYPE_THREAD:
            printf("thread");
            ABTI_thread *p_thread = (ABTI_thread *)p_unit->p_unit;
            ABTI_thread_print(p_thread);
            break;

        case ABT_UNIT_TYPE_TASK:
            printf("task");
            ABTI_task *p_task = (ABTI_task *)p_unit->p_unit;
            ABTI_task_print(p_task);
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

