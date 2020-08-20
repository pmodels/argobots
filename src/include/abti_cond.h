/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#ifndef ABTI_COND_H_INCLUDED
#define ABTI_COND_H_INCLUDED

#include "abti_mutex.h"

/* Inlined functions for Condition Variable  */

static inline void ABTI_cond_init(ABTI_cond *p_cond)
{
    ABTI_spinlock_clear(&p_cond->lock);
    p_cond->p_waiter_mutex = NULL;
    p_cond->num_waiters = 0;
    p_cond->p_head = NULL;
    p_cond->p_tail = NULL;
}

static inline void ABTI_cond_fini(ABTI_cond *p_cond)
{
    /* The lock needs to be acquired to safely free the condition structure.
     * However, we do not have to unlock it because the entire structure is
     * freed here. */
    ABTI_spinlock_acquire(&p_cond->lock);
}

static inline ABTI_cond *ABTI_cond_get_ptr(ABT_cond cond)
{
#ifndef ABT_CONFIG_DISABLE_ERROR_CHECK
    ABTI_cond *p_cond;
    if (cond == ABT_COND_NULL) {
        p_cond = NULL;
    } else {
        p_cond = (ABTI_cond *)cond;
    }
    return p_cond;
#else
    return (ABTI_cond *)cond;
#endif
}

static inline ABT_cond ABTI_cond_get_handle(ABTI_cond *p_cond)
{
#ifndef ABT_CONFIG_DISABLE_ERROR_CHECK
    ABT_cond h_cond;
    if (p_cond == NULL) {
        h_cond = ABT_COND_NULL;
    } else {
        h_cond = (ABT_cond)p_cond;
    }
    return h_cond;
#else
    return (ABT_cond)p_cond;
#endif
}

static inline int ABTI_cond_wait(ABTI_xstream **pp_local_xstream,
                                 ABTI_cond *p_cond, ABTI_mutex *p_mutex)
{
    int abt_errno = ABT_SUCCESS;

    ABTI_xstream *p_local_xstream = *pp_local_xstream;
    ABTI_ythread *p_thread;
    ABTI_thread *p_unit;

    if (p_local_xstream != NULL) {
        ABTI_thread *p_self = p_local_xstream->p_unit;
        ABTI_CHECK_TRUE(ABTI_thread_type_is_thread(p_self->type), ABT_ERR_COND);
        p_thread = ABTI_unit_get_thread(p_self);
        p_unit = &p_thread->thread;
    } else {
        /* external thread */
        p_thread = NULL;
        p_unit = (ABTI_thread *)ABTU_calloc(1, sizeof(ABTI_thread));
        p_unit->type = ABTI_THREAD_TYPE_EXT;
        /* use state for synchronization */
        ABTD_atomic_relaxed_store_int(&p_unit->state,
                                      ABTI_THREAD_STATE_BLOCKED);
    }

    ABTI_spinlock_acquire(&p_cond->lock);

    if (p_cond->p_waiter_mutex == NULL) {
        p_cond->p_waiter_mutex = p_mutex;
    } else {
        ABT_bool result = ABTI_mutex_equal(p_cond->p_waiter_mutex, p_mutex);
        if (result == ABT_FALSE) {
            ABTI_spinlock_release(&p_cond->lock);
            if (p_thread)
                ABTU_free(p_unit);
            abt_errno = ABT_ERR_INV_MUTEX;
            goto fn_fail;
        }
    }

    if (p_cond->num_waiters == 0) {
        p_unit->p_prev = p_unit;
        p_unit->p_next = p_unit;
        p_cond->p_head = p_unit;
        p_cond->p_tail = p_unit;
    } else {
        p_cond->p_tail->p_next = p_unit;
        p_cond->p_head->p_prev = p_unit;
        p_unit->p_prev = p_cond->p_tail;
        p_unit->p_next = p_cond->p_head;
        p_cond->p_tail = p_unit;
    }

    p_cond->num_waiters++;

    if (p_thread) {
        /* Change the ULT's state to BLOCKED */
        ABTI_thread_set_blocked(p_thread);

        ABTI_spinlock_release(&p_cond->lock);

        /* Unlock the mutex that the calling ULT is holding */
        /* FIXME: should check if mutex was locked by the calling ULT */
        ABTI_mutex_unlock(p_local_xstream, p_mutex);

        /* Suspend the current ULT */
        ABTI_thread_suspend(pp_local_xstream, p_thread,
                            ABT_SYNC_EVENT_TYPE_COND, (void *)p_cond);

    } else {
        ABTI_spinlock_release(&p_cond->lock);
        ABTI_mutex_unlock(p_local_xstream, p_mutex);

        /* External thread is waiting here. */
        while (ABTD_atomic_acquire_load_int(&p_unit->state) !=
               ABTI_THREAD_STATE_READY)
            ;
        ABTU_free(p_unit);
    }

    /* Lock the mutex again */
    ABTI_mutex_lock(pp_local_xstream, p_mutex);

fn_exit:
    return abt_errno;

fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

static inline void ABTI_cond_broadcast(ABTI_xstream *p_local_xstream,
                                       ABTI_cond *p_cond)
{
    ABTI_spinlock_acquire(&p_cond->lock);

    if (p_cond->num_waiters == 0) {
        ABTI_spinlock_release(&p_cond->lock);
        return;
    }

    /* Wake up all waiting ULTs */
    ABTI_thread *p_head = p_cond->p_head;
    ABTI_thread *p_unit = p_head;
    while (1) {
        ABTI_thread *p_next = p_unit->p_next;

        p_unit->p_prev = NULL;
        p_unit->p_next = NULL;

        if (ABTI_thread_type_is_thread(p_unit->type)) {
            ABTI_ythread *p_thread = ABTI_unit_get_thread(p_unit);
            ABTI_thread_set_ready(p_local_xstream, p_thread);
        } else {
            /* When the head is an external thread */
            ABTD_atomic_release_store_int(&p_unit->state,
                                          ABTI_THREAD_STATE_READY);
        }

        /* Next ULT */
        if (p_next != p_head) {
            p_unit = p_next;
        } else {
            break;
        }
    }

    p_cond->p_waiter_mutex = NULL;
    p_cond->num_waiters = 0;
    p_cond->p_head = NULL;
    p_cond->p_tail = NULL;

    ABTI_spinlock_release(&p_cond->lock);
}

#endif /* ABTI_COND_H_INCLUDED */
