/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"


int ABTI_contn_create(ABTI_contn **pp_contn)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_contn *p_contn;

    p_contn = (ABTI_contn *)ABTU_malloc(sizeof(ABTI_contn));
    p_contn->num_elems = 0;
    p_contn->p_head = NULL;
    p_contn->p_tail = NULL;

    *pp_contn = p_contn;

  fn_exit:
    return abt_errno;

  fn_fail:
    *pp_contn = NULL;
    HANDLE_ERROR_WITH_CODE("ABTI_contn_create", abt_errno);
    goto fn_exit;
}

int ABTI_contn_free(ABTI_contn **pp_contn)
{
    assert(pp_contn && *pp_contn);

    int abt_errno = ABT_SUCCESS;
    ABTI_contn *p_contn = *pp_contn;

    while (p_contn->num_elems > 0) {
        ABTI_elem *p_elem = ABTI_contn_pop(p_contn);

        switch (ABTI_elem_get_type(p_elem)) {
            case ABT_UNIT_TYPE_THREAD: {
                ABTI_thread *p_thread = ABTI_elem_get_thread(p_elem);
                abt_errno = ABTI_thread_free(p_thread);
                ABTI_CHECK_ERROR_MSG(abt_errno, "ABTI_thread_free");
                break;
            }
            case ABT_UNIT_TYPE_TASK: {
                ABTI_task *p_task = ABTI_elem_get_task(p_elem);
                abt_errno = ABTI_task_free(p_task);
                ABTI_CHECK_ERROR_MSG(abt_errno, "ABTI_task_free");
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
    HANDLE_ERROR_WITH_CODE("ABTI_contn_free", abt_errno);
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

int ABTI_contn_print(ABTI_contn *p_contn)
{
    int abt_errno = ABT_SUCCESS;
    size_t i;

    if (p_contn == NULL) {
        printf("NULL CONTN\n");
        goto fn_exit;
    }

    printf("num_elems: %zu ", p_contn->num_elems);
    printf("{ ");
    ABTI_elem *p_current = p_contn->p_head;
    for (i = 0; i < p_contn->num_elems; i++) {
        if (i != 0) printf(" -> ");
        ABTI_elem_print(p_current);
    }
    printf(" }\n");

  fn_exit:
    return abt_errno;
}

