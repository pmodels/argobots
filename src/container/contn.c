/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"


void ABTI_contn_create(ABTI_contn **pp_contn)
{
    ABTI_contn *p_contn;

    p_contn = (ABTI_contn *)ABTU_malloc(sizeof(ABTI_contn));
    p_contn->num_elems = 0;
    p_contn->p_head = NULL;
    p_contn->p_tail = NULL;

    *pp_contn = p_contn;
}

int ABTI_contn_free(ABTI_contn **pp_contn)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_CHECK_TRUE(pp_contn && *pp_contn, ABT_ERR_OTHER);

    ABTI_contn *p_contn = *pp_contn;

    while (p_contn->num_elems > 0) {
        ABTI_elem *p_elem = ABTI_contn_pop(p_contn);

        switch (ABTI_elem_get_type(p_elem)) {
            case ABT_UNIT_TYPE_THREAD: {
                ABTI_thread *p_thread = ABTI_elem_get_thread(p_elem);
                ABTI_thread_free(p_thread);
                break;
            }
            case ABT_UNIT_TYPE_TASK: {
                ABTI_task *p_task = ABTI_elem_get_task(p_elem);
                ABTI_task_free(p_task);
                break;
            }
            case ABT_UNIT_TYPE_XSTREAM: {
                ABTI_xstream *p_xstream = ABTI_elem_get_xstream(p_elem);
                abt_errno = ABTI_xstream_free(p_xstream);
                ABTI_CHECK_ERROR_MSG(abt_errno, "ABTI_xstream_free");
                break;
            }
            default:
                HANDLE_ERROR("Unknown elem type");
                break;
        }

    }

    ABTU_free(p_contn);

    *pp_contn = NULL;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

size_t ABTI_contn_get_size(ABTI_contn *p_contn)
{
    return p_contn->num_elems;
}

void ABTI_contn_push(ABTI_contn *p_contn, ABTI_elem *p_elem)
{
    p_elem->p_contn = p_contn;

    if (p_contn->num_elems == 0) {
        p_elem->p_prev = p_elem;
        p_elem->p_next = p_elem;
        p_contn->p_head = p_elem;
        p_contn->p_tail = p_elem;
    } else {
        ABTI_elem *p_head = p_contn->p_head;
        ABTI_elem *p_tail = p_contn->p_tail;
        p_tail->p_next = p_elem;
        p_head->p_prev = p_elem;
        p_elem->p_prev = p_tail;
        p_elem->p_next = p_head;
        p_contn->p_tail = p_elem;
    }
    p_contn->num_elems++;
}

ABTI_elem *ABTI_contn_pop(ABTI_contn *p_contn)
{
    ABTI_elem *p_elem = NULL;

    if (p_contn->num_elems > 0) {
        p_elem = p_contn->p_head;
        if (p_contn->num_elems == 1) {
            p_contn->p_head = NULL;
            p_contn->p_tail = NULL;
        } else {
            p_elem->p_prev->p_next = p_elem->p_next;
            p_elem->p_next->p_prev = p_elem->p_prev;
            p_contn->p_head = p_elem->p_next;
        }
        p_contn->num_elems--;

        p_elem->p_contn = NULL;
        p_elem->p_prev = NULL;
        p_elem->p_next = NULL;

    }
    return p_elem;
}

void ABTI_contn_remove(ABTI_contn *p_contn, ABTI_elem *p_elem)
{
    if (p_elem->p_contn == NULL) return;

    if (p_contn->num_elems == 0) return;

    if (p_elem->p_contn != p_contn) {
        HANDLE_ERROR("Not my contn");
    }

    if (p_contn->num_elems == 1) {
        p_contn->p_head = NULL;
        p_contn->p_tail = NULL;
    } else {
        p_elem->p_prev->p_next = p_elem->p_next;
        p_elem->p_next->p_prev = p_elem->p_prev;
        if (p_elem == p_contn->p_head)
            p_contn->p_head = p_elem->p_next;
        else if (p_elem == p_contn->p_tail)
            p_contn->p_tail = p_elem->p_prev;
    }
    p_contn->num_elems--;

    p_elem->p_contn = NULL;
    p_elem->p_prev = NULL;
    p_elem->p_next = NULL;
}

void ABTI_contn_print(ABTI_contn *p_contn, FILE *p_os, int indent, ABT_bool detail)
{
    size_t i;
    char *prefix = ABTU_get_indent_str(indent);

    if (p_contn == NULL) {
        fprintf(p_os, "%s== NULL CONTN ==\n", prefix);
        goto fn_exit;
    }

    fprintf(p_os,
        "%s== CONTN (%p) ==\n"
        "%snum_elems: %zu\n"
        "%shead     : %p\n"
        "%stail     : %p\n",
        prefix, p_contn,
        prefix, p_contn->num_elems,
        prefix, p_contn->p_head,
        prefix, p_contn->p_tail
    );

    if (p_contn->num_elems > 0) {
        fprintf(p_os, "%sCONTN (%p) elements:\n", prefix, p_contn);
        ABTI_elem *p_current = p_contn->p_head;
        for (i = 0; i < p_contn->num_elems; i++) {
            if (i != 0) fprintf(p_os, "%s  -->\n", prefix);
            ABTI_elem_print(p_current, p_os, indent + ABTI_INDENT, detail);
        }
    }

  fn_exit:
    fflush(p_os);
    ABTU_free(prefix);
}

