/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"
#include <sys/time.h>

/** @defgroup COND Condition Variable
 * This group is for Condition Variable.
 */

/**
 * @ingroup COND
 * @brief   Create a new condition variable.
 *
 * \c ABT_cond_create() creates a new condition variable and returns its handle
 * through \c newcond.
 * If an error occurs in this routine, a non-zero error code will be returned
 * and newcond will be set to \c ABT_COND_NULL.
 *
 * @param[out] newcond  handle to a new condition variable
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_cond_create(ABT_cond *newcond)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_cond *p_newcond;

    p_newcond = (ABTI_cond *)ABTU_malloc(sizeof(ABTI_cond));
    ABTI_cond_init(p_newcond);

    /* Return value */
    *newcond = ABTI_cond_get_handle(p_newcond);

    return abt_errno;
}

/**
 * @ingroup COND
 * @brief   Free the condition variable.
 *
 * \c ABT_cond_free() deallocates the memory used for the condition variable
 * object associated with the handle \c cond. If it is successfully processed,
 * \c cond is set to \c ABT_COND_NULL.
 *
 * @param[in,out] cond  handle to the condition variable
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_cond_free(ABT_cond *cond)
{
    int abt_errno = ABT_SUCCESS;
    ABT_cond h_cond = *cond;
    ABTI_cond *p_cond = ABTI_cond_get_ptr(h_cond);
    ABTI_CHECK_NULL_COND_PTR(p_cond);

    ABTI_CHECK_TRUE(p_cond->num_waiters == 0, ABT_ERR_COND);

    ABTI_cond_fini(p_cond);
    ABTU_free(p_cond);

    /* Return value */
    *cond = ABT_COND_NULL;

fn_exit:
    return abt_errno;

fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

/**
 * @ingroup COND
 * @brief   Wait on the condition.
 *
 * The ULT calling \c ABT_cond_wait() waits on the condition variable until
 * it is signaled.
 * The user should call this routine while the mutex specified as \c mutex is
 * locked. The mutex will be automatically released while waiting. After signal
 * is received and the waiting ULT is awakened, the mutex will be
 * automatically locked for use by the ULT. The user is then responsible for
 * unlocking mutex when the ULT is finished with it.
 *
 * @param[in] cond   handle to the condition variable
 * @param[in] mutex  handle to the mutex
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_cond_wait(ABT_cond cond, ABT_mutex mutex)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_xstream *p_local_xstream = ABTI_local_get_xstream();
    ABTI_cond *p_cond = ABTI_cond_get_ptr(cond);
    ABTI_CHECK_NULL_COND_PTR(p_cond);
    ABTI_mutex *p_mutex = ABTI_mutex_get_ptr(mutex);
    ABTI_CHECK_NULL_MUTEX_PTR(p_mutex);

    abt_errno = ABTI_cond_wait(&p_local_xstream, p_cond, p_mutex);
    if (abt_errno != ABT_SUCCESS)
        goto fn_fail;

fn_exit:
    return abt_errno;

fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

static inline double convert_timespec_to_sec(const struct timespec *p_ts)
{
    double secs;
    secs = ((double)p_ts->tv_sec) + 1.0e-9 * ((double)p_ts->tv_nsec);
    return secs;
}

static inline void remove_thread(ABTI_cond *p_cond, ABTI_thread *p_thread)
{
    if (p_thread->p_next == NULL)
        return;

    ABTI_spinlock_acquire(&p_cond->lock);

    if (p_thread->p_next == NULL) {
        ABTI_spinlock_release(&p_cond->lock);
        return;
    }

    /* If p_thread is still in the queue, we have to remove it. */
    p_cond->num_waiters--;
    if (p_cond->num_waiters == 0) {
        p_cond->p_waiter_mutex = NULL;
        p_cond->p_head = NULL;
        p_cond->p_tail = NULL;
    } else {
        p_thread->p_prev->p_next = p_thread->p_next;
        p_thread->p_next->p_prev = p_thread->p_prev;
        if (p_thread == p_cond->p_head) {
            p_cond->p_head = p_thread->p_next;
        } else if (p_thread == p_cond->p_tail) {
            p_cond->p_tail = p_thread->p_prev;
        }
    }

    ABTI_spinlock_release(&p_cond->lock);

    p_thread->p_prev = NULL;
    p_thread->p_next = NULL;
}

/**
 * @ingroup COND
 * @brief   Wait on the condition.
 *
 * The ULT calling \c ABT_cond_timedwait() waits on the condition variable
 * until it is signaled or the absolute time specified by \c abstime passes.
 * If system time equals or exceeds \c abstime before \c cond is signaled,
 * the error code \c ABT_ERR_COND_TIMEDOUT is returned.
 *
 * The user should call this routine while the mutex specified as \c mutex is
 * locked. The mutex will be automatically released while waiting. After signal
 * is received and the waiting ULT is awakened, the mutex will be
 * automatically locked for use by the ULT. The user is then responsible for
 * unlocking mutex when the ULT is finished with it.
 *
 * @param[in] cond     handle to the condition variable
 * @param[in] mutex    handle to the mutex
 * @param[in] abstime  absolute time for timeout
 * @return Error code
 * @retval ABT_SUCCESS            on success
 * @retval ABT_ERR_COND_TIMEDOUT  timeout
 */
int ABT_cond_timedwait(ABT_cond cond, ABT_mutex mutex,
                       const struct timespec *abstime)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_xstream *p_local_xstream = ABTI_local_get_xstream();
    ABTI_cond *p_cond = ABTI_cond_get_ptr(cond);
    ABTI_CHECK_NULL_COND_PTR(p_cond);
    ABTI_mutex *p_mutex = ABTI_mutex_get_ptr(mutex);
    ABTI_CHECK_NULL_MUTEX_PTR(p_mutex);

    double tar_time = convert_timespec_to_sec(abstime);

    ABTI_thread thread;
    thread.type = ABTI_THREAD_TYPE_EXT;
    ABTD_atomic_relaxed_store_int(&thread.state, ABTI_THREAD_STATE_BLOCKED);

    ABTI_spinlock_acquire(&p_cond->lock);

    if (p_cond->p_waiter_mutex == NULL) {
        p_cond->p_waiter_mutex = p_mutex;
    } else {
        ABT_bool result = ABTI_mutex_equal(p_cond->p_waiter_mutex, p_mutex);
        if (result == ABT_FALSE) {
            ABTI_spinlock_release(&p_cond->lock);
            abt_errno = ABT_ERR_INV_MUTEX;
            goto fn_fail;
        }
    }

    if (p_cond->num_waiters == 0) {
        thread.p_prev = &thread;
        thread.p_next = &thread;
        p_cond->p_head = &thread;
        p_cond->p_tail = &thread;
    } else {
        p_cond->p_tail->p_next = &thread;
        p_cond->p_head->p_prev = &thread;
        thread.p_prev = p_cond->p_tail;
        thread.p_next = p_cond->p_head;
        p_cond->p_tail = &thread;
    }

    p_cond->num_waiters++;

    ABTI_spinlock_release(&p_cond->lock);

    /* Unlock the mutex that the calling ULT is holding */
    ABTI_mutex_unlock(p_local_xstream, p_mutex);

    while (ABTD_atomic_acquire_load_int(&thread.state) !=
           ABTI_THREAD_STATE_READY) {
        double cur_time = ABTI_get_wtime();
        if (cur_time >= tar_time) {
            remove_thread(p_cond, &thread);
            abt_errno = ABT_ERR_COND_TIMEDOUT;
            break;
        }
#ifndef ABT_CONFIG_DISABLE_EXT_THREAD
        if (!(ABTI_self_get_type(p_local_xstream) &
              ABTI_THREAD_TYPE_YIELDABLE)) {
            ABTD_atomic_pause();
            continue;
        }
#endif
        ABTI_ythread_yield(&p_local_xstream,
                           ABTI_thread_get_ythread(p_local_xstream->p_thread),
                           ABT_SYNC_EVENT_TYPE_COND, (void *)p_cond);
    }

    /* Lock the mutex again */
    ABTI_mutex_lock(&p_local_xstream, p_mutex);

fn_exit:
    return abt_errno;

fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

/**
 * @ingroup COND
 * @brief   Signal a condition.
 *
 * \c ABT_cond_signal() signals another ULT that is waiting on the condition
 * variable. Only one ULT is waken up by the signal and the scheduler
 * determines the ULT.
 * This routine shall have no effect if no ULTs are currently blocked on the
 * condition variable.
 *
 * @param[in] cond   handle to the condition variable
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_cond_signal(ABT_cond cond)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_xstream *p_local_xstream = ABTI_local_get_xstream();
    ABTI_cond *p_cond = ABTI_cond_get_ptr(cond);
    ABTI_CHECK_NULL_COND_PTR(p_cond);

    ABTI_spinlock_acquire(&p_cond->lock);

    if (p_cond->num_waiters == 0) {
        ABTI_spinlock_release(&p_cond->lock);
        goto fn_exit;
    }

    /* Wake up the first waiting ULT */
    ABTI_thread *p_thread = p_cond->p_head;

    p_cond->num_waiters--;
    if (p_cond->num_waiters == 0) {
        p_cond->p_waiter_mutex = NULL;
        p_cond->p_head = NULL;
        p_cond->p_tail = NULL;
    } else {
        p_thread->p_prev->p_next = p_thread->p_next;
        p_thread->p_next->p_prev = p_thread->p_prev;
        p_cond->p_head = p_thread->p_next;
    }
    p_thread->p_prev = NULL;
    p_thread->p_next = NULL;

    if (p_thread->type & ABTI_THREAD_TYPE_YIELDABLE) {
        ABTI_ythread *p_ythread = ABTI_thread_get_ythread(p_thread);
        ABTI_ythread_set_ready(p_local_xstream, p_ythread);
    } else {
        /* When the head is an external thread */
        ABTD_atomic_release_store_int(&p_thread->state,
                                      ABTI_THREAD_STATE_READY);
    }

    ABTI_spinlock_release(&p_cond->lock);

fn_exit:
    return abt_errno;

fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

/**
 * @ingroup COND
 * @brief   Broadcast a condition.
 *
 * \c ABT_cond_broadcast() signals all ULTs that are waiting on the
 * condition variable.
 * This routine shall have no effect if no ULTs are currently blocked on the
 * condition variable.
 *
 * @param[in] cond   handle to the condition variable
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_cond_broadcast(ABT_cond cond)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_xstream *p_local_xstream = ABTI_local_get_xstream();
    ABTI_cond *p_cond = ABTI_cond_get_ptr(cond);
    ABTI_CHECK_NULL_COND_PTR(p_cond);

    ABTI_cond_broadcast(p_local_xstream, p_cond);

fn_exit:
    return abt_errno;

fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}
