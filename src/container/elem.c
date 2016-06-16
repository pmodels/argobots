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

ABTI_elem *ABTI_elem_create_from_xstream(ABTI_xstream *p_xstream)
{
    ABTI_elem *p_elem;

    p_elem = (ABTI_elem *)ABTU_malloc(sizeof(ABTI_elem));
    p_elem->p_contn = NULL;
    p_elem->type    = ABT_UNIT_TYPE_XSTREAM;
    p_elem->p_obj   = (void *)p_xstream;
    p_elem->p_prev  = NULL;
    p_elem->p_next  = NULL;

    return p_elem;
}

ABTI_elem *ABTI_elem_create_from_thread(ABTI_thread *p_thread)
{
    ABTI_elem *p_elem;

    p_elem = (ABTI_elem *)ABTU_malloc(sizeof(ABTI_elem));
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

    p_elem = (ABTI_elem *)ABTU_malloc(sizeof(ABTI_elem));
    p_elem->p_contn = NULL;
    p_elem->type    = ABT_UNIT_TYPE_TASK;
    p_elem->p_obj   = (void *)p_task;
    p_elem->p_prev  = NULL;
    p_elem->p_next  = NULL;

    return p_elem;
}

void ABTI_elem_free(ABTI_elem **pp_elem)
{
    ABTU_free(*pp_elem);
    *pp_elem = NULL;
}

void ABTI_elem_print(ABTI_elem *p_elem, FILE *p_os, int indent, ABT_bool detail)
{
    char *prefix = ABTU_get_indent_str(indent);

    if (p_elem == NULL) {
        fprintf(p_os, "%s== NULL ELEM ==\n", prefix);
        goto fn_exit;
    }

    char *type;
    switch (p_elem->type) {
        case ABT_UNIT_TYPE_THREAD : type = "ULT"; break;
        case ABT_UNIT_TYPE_TASK   : type = "TASKLET"; break;
        case ABT_UNIT_TYPE_XSTREAM: type = "ES"; break;
        default:                    type = "UNKNOWN"; break;
    }

    fprintf(p_os,
        "%s== ELEM (%p) ==\n"
        "%scontn: %p\n"
        "%stype : %s\n"
        "%sobj  : %p\n"
        "%sprev : %p\n"
        "%snext : %p\n",
        prefix, p_elem,
        prefix, p_elem->p_contn,
        prefix, type,
        prefix, p_elem->p_obj,
        prefix, p_elem->p_prev,
        prefix, p_elem->p_next
    );

    if (detail == ABT_TRUE) {
        switch (p_elem->type) {
            case ABT_UNIT_TYPE_THREAD: {
                ABTI_thread *p_thread = (ABTI_thread *)p_elem->p_obj;
                ABTI_thread_print(p_thread, p_os, indent + ABTI_INDENT);
                break;
            }

            case ABT_UNIT_TYPE_TASK: {
                ABTI_task *p_task = (ABTI_task *)p_elem->p_obj;
                ABTI_task_print(p_task, p_os, indent + ABTI_INDENT);
                break;
            }

            case ABT_UNIT_TYPE_XSTREAM: {
                ABTI_xstream *p_xstream = (ABTI_xstream *)p_elem->p_obj;
                ABTI_xstream_print(p_xstream, p_os, indent + ABTI_INDENT,
                                   ABT_TRUE);
                break;
            }

            default:
                break;
        }
    }

  fn_exit:
    fflush(p_os);
    ABTU_free(prefix);
}

