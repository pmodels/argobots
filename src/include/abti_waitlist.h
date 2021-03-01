/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#ifndef ABTI_WAITLIST_H_INCLUDED
#define ABTI_WAITLIST_H_INCLUDED

#include "abt_config.h"

static inline void ABTI_waitlist_init(ABTI_waitlist *p_waitlist)
{
    p_waitlist->p_head = NULL;
    p_waitlist->p_tail = NULL;
}

static inline void
ABTI_waitlist_wait_and_unlock(ABTI_local **pp_local, ABTI_waitlist *p_waitlist,
                              ABTD_spinlock *p_lock,
                              ABT_sync_event_type sync_event_type, void *p_sync)
{
    ABTI_ASSERT(ABTD_spinlock_is_locked(p_lock) == ABT_TRUE);
    ABTI_ythread *p_ythread = NULL;
    ABTI_xstream *p_local_xstream = ABTI_local_get_xstream_or_null(*pp_local);
    if (!ABTI_IS_EXT_THREAD_ENABLED || p_local_xstream) {
        p_ythread = ABTI_thread_get_ythread_or_null(p_local_xstream->p_thread);
    }
    if (!p_ythread) {
        /* External thread or non-yieldable thread. */
        ABTI_thread thread;
        thread.type = ABTI_THREAD_TYPE_EXT;
        /* use state for synchronization */
        ABTD_atomic_relaxed_store_int(&thread.state, ABT_THREAD_STATE_BLOCKED);
        /* Add thread to the list. */
        thread.p_next = NULL;
        if (p_waitlist->p_head == NULL) {
            p_waitlist->p_head = &thread;
        } else {
            p_waitlist->p_tail->p_next = &thread;
        }
        p_waitlist->p_tail = &thread;

        /* Non-yieldable thread is waiting here. */
        ABTD_spinlock_release(p_lock);
        while (ABTD_atomic_acquire_load_int(&thread.state) !=
               ABT_THREAD_STATE_READY)
            ;
    } else {
        /* Add p_thread to the list. */
        p_ythread->thread.p_next = NULL;
        if (p_waitlist->p_head == NULL) {
            p_waitlist->p_head = &p_ythread->thread;
        } else {
            p_waitlist->p_tail->p_next = &p_ythread->thread;
        }
        p_waitlist->p_tail = &p_ythread->thread;

        /* Suspend the current ULT */
        ABTI_ythread_set_blocked(p_ythread);
        ABTD_spinlock_release(p_lock);
        ABTI_ythread_suspend(&p_local_xstream, p_ythread,
                             ABT_SYNC_EVENT_TYPE_EVENTUAL, p_sync);
        /* Resumed. */
        *pp_local = ABTI_xstream_get_local(p_local_xstream);
    }
}

/* Return ABT_TRUE if timed out. */
static inline ABT_bool ABTI_waitlist_wait_timedout_and_unlock(
    ABTI_local **pp_local, ABTI_waitlist *p_waitlist, ABTD_spinlock *p_lock,
    double target_time, ABT_sync_event_type sync_event_type, void *p_sync)
{
    ABTI_ASSERT(ABTD_spinlock_is_locked(p_lock) == ABT_TRUE);
    ABTI_ythread *p_ythread = NULL;
    ABTI_xstream *p_local_xstream = ABTI_local_get_xstream_or_null(*pp_local);
    if (!ABTI_IS_EXT_THREAD_ENABLED || p_local_xstream)
        p_ythread = ABTI_thread_get_ythread_or_null(p_local_xstream->p_thread);

    /* Always use a dummy thread. */
    ABTI_thread thread;
    thread.type = ABTI_THREAD_TYPE_EXT;
    /* use state for synchronization */
    ABTD_atomic_relaxed_store_int(&thread.state, ABT_THREAD_STATE_BLOCKED);

    /* Add p_thread to the list.  This implementation is tricky since this
     * updates p_prev as well for removal on timeout while the other functions
     * (e.g., wait, broadcast, signal) do not update it. */
    thread.p_next = NULL;
    if (p_waitlist->p_head == NULL) {
        p_waitlist->p_head = &thread;
        thread.p_prev = NULL;
    } else {
        p_waitlist->p_tail->p_next = &thread;
        thread.p_prev = p_waitlist->p_tail;
    }
    p_waitlist->p_tail = &thread;

    /* Waiting here. */
    ABTD_spinlock_release(p_lock);
    while (ABTD_atomic_acquire_load_int(&thread.state) !=
           ABT_THREAD_STATE_READY) {
        double cur_time = ABTI_get_wtime();
        if (cur_time >= target_time) {
            /* Timeout.  Remove this thread if not signaled even after taking
             * a lock. */
            ABTD_spinlock_acquire(p_lock);
            ABT_bool is_timedout =
                (ABTD_atomic_acquire_load_int(&thread.state) !=
                 ABT_THREAD_STATE_READY)
                    ? ABT_TRUE
                    : ABT_FALSE;
            if (is_timedout) {
                /* This thread is still in the list. */
                if (p_waitlist->p_head == &thread) {
                    /* thread is a head. */
                    /* Note that thread->p_prev cannot be used to check whether
                     * thread is a head or not because signal and broadcast do
                     * not modify thread->p_prev. */
                    p_waitlist->p_head = thread.p_next;
                    if (!thread.p_next) {
                        /* This thread is p_tail */
                        ABTI_ASSERT(p_waitlist->p_tail == &thread);
                        p_waitlist->p_tail = NULL;
                    }
                } else {
                    /* thread is not a head and thus p_prev exists. */
                    ABTI_ASSERT(thread.p_prev);
                    thread.p_prev->p_next = thread.p_next;
                    if (thread.p_next && thread.type == ABTI_THREAD_TYPE_EXT) {
                        /* Only an external thread (created by this function)
                         * checks p_prev.  Note that an external thread is
                         * dummy, so updating p_prev is allowed. */
                        thread.p_next->p_prev = thread.p_prev;
                    } else {
                        /* This thread is p_tail */
                        ABTI_ASSERT(p_waitlist->p_tail == &thread);
                        p_waitlist->p_tail = thread.p_prev;
                    }
                }
                /* We do not need to modify thread->p_prev and p_next since this
                 * dummy thread is no longer used. */
            }
            ABTD_spinlock_release(p_lock);
            return is_timedout;
        }
        if (p_ythread) {
            ABTI_ythread_yield(&p_local_xstream, p_ythread, sync_event_type,
                               p_sync);
            *pp_local = ABTI_xstream_get_local(p_local_xstream);
        } else {
            ABTD_atomic_pause();
        }
    }
    /* Singled */
    return ABT_FALSE;
}

static inline void ABTI_waitlist_signal(ABTI_local *p_local,
                                        ABTI_waitlist *p_waitlist)
{
    ABTI_thread *p_thread = p_waitlist->p_head;
    if (p_thread) {
        ABTI_thread *p_next = p_thread->p_next;
        p_thread->p_next = NULL;

        ABTI_ythread *p_ythread = ABTI_thread_get_ythread_or_null(p_thread);
        if (p_ythread) {
            ABTI_ythread_set_ready(p_local, p_ythread);
        } else {
            /* When p_thread is an external thread or a tasklet */
            ABTD_atomic_release_store_int(&p_thread->state,
                                          ABT_THREAD_STATE_READY);
        }
        /* After updating p_thread->state, p_thread can be updated and
         * freed. */
        p_waitlist->p_head = p_next;
        if (!p_next)
            p_waitlist->p_tail = NULL;
    }
}

static inline void ABTI_waitlist_broadcast(ABTI_local *p_local,
                                           ABTI_waitlist *p_waitlist)
{
    ABTI_thread *p_thread = p_waitlist->p_head;
    if (p_thread) {
        do {
            ABTI_thread *p_next = p_thread->p_next;
            p_thread->p_next = NULL;

            ABTI_ythread *p_ythread = ABTI_thread_get_ythread_or_null(p_thread);
            if (p_ythread) {
                ABTI_ythread_set_ready(p_local, p_ythread);
            } else {
                /* When p_thread is an external thread or a tasklet */
                ABTD_atomic_release_store_int(&p_thread->state,
                                              ABT_THREAD_STATE_READY);
            }
            /* After updating p_thread->state, p_thread can be updated and
             * freed. */
            p_thread = p_next;
        } while (p_thread);
        p_waitlist->p_head = NULL;
        p_waitlist->p_tail = NULL;
    }
}

static inline ABT_bool ABTI_waitlist_is_empty(ABTI_waitlist *p_waitlist)
{
    return p_waitlist->p_head ? ABT_TRUE : ABT_FALSE;
}

#endif /* ABTI_WAITLIST_H_INCLUDED */
