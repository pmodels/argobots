/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"


ABT_unit_type ABTI_elem_get_type(ABTI_elem *p_elem)
{
    return p_elem->type;
}

ABTI_xstream *ABTI_elem_get_xstream(ABTI_elem *p_elem)
{
    ABTI_xstream *p_xstream;
    if (p_elem->type == ABT_UNIT_TYPE_XSTREAM) {
        p_xstream = (ABTI_xstream *)p_elem->p_obj;
    } else {
        p_xstream = NULL;
    }
    return p_xstream;
}

ABTI_thread *ABTI_elem_get_thread(ABTI_elem *p_elem)
{
    ABTI_thread *p_thread = NULL;
    if (p_elem->type == ABT_UNIT_TYPE_THREAD) {
        p_thread = (ABTI_thread *)p_elem->p_obj;
    }
    return p_thread;
}

ABTI_task *ABTI_elem_get_task(ABTI_elem *p_elem)
{
    ABTI_task *p_task = NULL;
    if (p_elem->type == ABT_UNIT_TYPE_TASK) {
        p_task = (ABTI_task *)p_elem->p_obj;
    }
    return p_task;
}

ABTI_elem *ABTI_elem_get_next(ABTI_elem *p_elem)
{
    return p_elem->p_next;
}

void ABTI_elem_create_from_xstream(ABTI_xstream *p_xstream)
{
    ABTI_elem *p_elem;

    p_elem = &p_xstream->elem;
    p_elem->p_contn = NULL;
    p_elem->type    = ABT_UNIT_TYPE_XSTREAM;
    p_elem->p_obj   = (void *)p_xstream;
    p_elem->p_prev  = NULL;
    p_elem->p_next  = NULL;
}

ABTI_elem *ABTI_elem_create_from_thread(ABTI_thread *p_thread)
{
    ABTI_elem *p_elem;

    p_elem = &p_thread->elem_def;
    p_elem->p_contn = NULL;
    p_elem->type    = ABT_UNIT_TYPE_THREAD;
    p_elem->p_obj   = (void *)p_thread;
    p_elem->p_prev  = NULL;
    p_elem->p_next  = NULL;

    return p_elem;
}

ABTI_elem *ABTI_elem_create_from_task(ABTI_task *p_task)
{
    ABTI_elem *p_elem;

    p_elem = &p_task->elem_def;
    p_elem->p_contn = NULL;
    p_elem->type    = ABT_UNIT_TYPE_TASK;
    p_elem->p_obj   = (void *)p_task;
    p_elem->p_prev  = NULL;
    p_elem->p_next  = NULL;

    return p_elem;
}

void ABTI_elem_free(ABTI_elem **pp_elem)
{
    *pp_elem = NULL;
}

int ABTI_elem_print(ABTI_elem *p_elem)
{
    int abt_errno = ABT_SUCCESS;

    printf("<");
    printf("contn:%p ", p_elem->p_contn);
    printf("type:");
    switch (p_elem->type) {
        case ABT_UNIT_TYPE_THREAD:
            printf("thread");
            ABTI_thread *p_thread = (ABTI_thread *)p_elem->p_obj;
            ABTI_thread_print(p_thread);
            break;

        case ABT_UNIT_TYPE_TASK:
            printf("task");
            ABTI_task *p_task = (ABTI_task *)p_elem->p_obj;
            ABTI_task_print(p_task);
            break;

        case ABT_UNIT_TYPE_XSTREAM:
            printf("xstream");
            break;

        default:
            printf("unknown");
    }
    printf(">");

    return abt_errno;
}

