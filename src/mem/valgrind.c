/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"

#ifdef HAVE_VALGRIND_SUPPORT

/*
 * These routines register and unregister stacks of threads so that Valgrind can
 * handle them.  This implementation uses a very naive linear list to keep track
 * of stacks and valgrind_id.  Its performance is bad compared to, for example,
 * hash tables, but performance is less important when Valgrind is used.
 */

typedef size_t ABTI_valgrind_id;

typedef struct ABTI_valgrind_id_list_t {
    const void *p_stack;
    ABTI_valgrind_id valgrind_id;
    struct ABTI_valgrind_id_list_t *p_next;
} ABTI_valgrind_id_list;

/* The list is protected by a global lock. */
uint8_t g_valgrind_id_list_lock = 0;
ABTI_valgrind_id_list *gp_valgrind_id_list_head = NULL;
ABTI_valgrind_id_list *gp_valgrind_id_list_tail = NULL;

static inline
void ABTI_valgrind_lock_acquire() {
    while (ABTD_atomic_test_and_set_uint8(&g_valgrind_id_list_lock)) {
        while (ABTD_atomic_load_uint8(&g_valgrind_id_list_lock) != 0);
    }
}

static inline
void ABTI_valgrind_lock_release() {
    ABTD_atomic_clear_uint8(&g_valgrind_id_list_lock);
}

#include <valgrind/valgrind.h>

void ABTI_valgrind_register_stack(const void *p_stack, size_t size) {
    if (p_stack == 0)
        return;

    const void *p_start = (char *)(p_stack);
    const void *p_end   = (char *)(p_stack) + size;

    ABTI_valgrind_lock_acquire();
    ABTI_valgrind_id valgrind_id = VALGRIND_STACK_REGISTER(p_start, p_end);
    ABTI_valgrind_id_list *p_valgrind_id_list =
                 (ABTI_valgrind_id_list *)malloc(sizeof(ABTI_valgrind_id_list));
    p_valgrind_id_list->p_stack = p_stack;
    p_valgrind_id_list->valgrind_id = valgrind_id;
    p_valgrind_id_list->p_next = 0;
    if (!gp_valgrind_id_list_head) {
        gp_valgrind_id_list_head = p_valgrind_id_list;
        gp_valgrind_id_list_tail = p_valgrind_id_list;
    } else {
        gp_valgrind_id_list_tail->p_next = p_valgrind_id_list;
        gp_valgrind_id_list_tail = p_valgrind_id_list;
    }
    LOG_DEBUG("valgrind : register stack %p (id = %d)\n",
              p_stack, (int) valgrind_id);
    ABTI_valgrind_lock_release();
}

void ABTI_valgrind_unregister_stack(const void *p_stack) {
    if (p_stack == 0)
        return;

    ABTI_valgrind_lock_acquire();
    if (gp_valgrind_id_list_head->p_stack == p_stack) {
        VALGRIND_STACK_DEREGISTER(gp_valgrind_id_list_head->valgrind_id);
        ABTI_valgrind_id_list *p_next = gp_valgrind_id_list_head->p_next;
        free(gp_valgrind_id_list_head);
        gp_valgrind_id_list_head = p_next;
        if (!p_next)
            gp_valgrind_id_list_tail = NULL;
    } else {
        /* Do linear search to find the corresponding valgrind_id. */
        ABTI_valgrind_id_list *p_prev    = gp_valgrind_id_list_head;
        ABTI_valgrind_id_list *p_current = gp_valgrind_id_list_head->p_next;
        ABT_bool deregister_flag = ABT_FALSE;
        while (p_current) {
            if (p_current->p_stack == p_stack) {
                LOG_DEBUG("valgrind : deregister stack %p (id = %d)\n",
                          p_stack, (int) p_current->valgrind_id);
                VALGRIND_STACK_DEREGISTER(p_current->valgrind_id);
                p_prev->p_next = p_current->p_next;
                if (!p_prev->p_next)
                    gp_valgrind_id_list_tail = p_prev;
                free(p_current);
                deregister_flag = ABT_TRUE;
                break;
            }
            p_prev    = p_current;
            p_current = p_current->p_next;
        }
        ABTI_ASSERT(deregister_flag);
    }
    ABTI_valgrind_lock_release();
}

#endif
