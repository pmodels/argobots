/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"
#include <sys/time.h>

static inline double convert_timespec_to_sec(const struct timespec *p_ts);

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
    ABTI_cond *p_newcond;
    int abt_errno = ABTU_malloc(sizeof(ABTI_cond), (void **)&p_newcond);
    ABTI_CHECK_ERROR(abt_errno);

    ABTI_cond_init(p_newcond);
    /* Return value */
    *newcond = ABTI_cond_get_handle(p_newcond);
    return ABT_SUCCESS;
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
    ABT_cond h_cond = *cond;
    ABTI_cond *p_cond = ABTI_cond_get_ptr(h_cond);
    ABTI_CHECK_NULL_COND_PTR(p_cond);
    ABTI_CHECK_TRUE(!ABTI_waitlist_is_empty(&p_cond->waitlist), ABT_ERR_COND);

    ABTI_cond_fini(p_cond);
    ABTU_free(p_cond);
    /* Return value */
    *cond = ABT_COND_NULL;
    return ABT_SUCCESS;
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
    ABTI_local *p_local = ABTI_local_get_local();
    ABTI_cond *p_cond = ABTI_cond_get_ptr(cond);
    ABTI_CHECK_NULL_COND_PTR(p_cond);
    ABTI_mutex *p_mutex = ABTI_mutex_get_ptr(mutex);
    ABTI_CHECK_NULL_MUTEX_PTR(p_mutex);

    int abt_errno = ABTI_cond_wait(&p_local, p_cond, p_mutex);
    ABTI_CHECK_ERROR(abt_errno);
    return ABT_SUCCESS;
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
    ABTI_local *p_local = ABTI_local_get_local();
    ABTI_cond *p_cond = ABTI_cond_get_ptr(cond);
    ABTI_CHECK_NULL_COND_PTR(p_cond);
    ABTI_mutex *p_mutex = ABTI_mutex_get_ptr(mutex);
    ABTI_CHECK_NULL_MUTEX_PTR(p_mutex);

    double tar_time = convert_timespec_to_sec(abstime);

    ABTI_thread thread;
    thread.type = ABTI_THREAD_TYPE_EXT;
    ABTD_atomic_relaxed_store_int(&thread.state, ABT_THREAD_STATE_BLOCKED);

    ABTI_spinlock_acquire(&p_cond->lock);

    if (p_cond->p_waiter_mutex == NULL) {
        p_cond->p_waiter_mutex = p_mutex;
    } else {
        if (p_cond->p_waiter_mutex != p_mutex) {
            ABTI_spinlock_release(&p_cond->lock);
            ABTI_HANDLE_ERROR(ABT_ERR_INV_MUTEX);
        }
    }

    /* Unlock the mutex that the calling ULT is holding */
    ABTI_mutex_unlock(p_local, p_mutex);
    ABT_bool is_timedout =
        ABTI_waitlist_wait_timedout_and_unlock(&p_local, &p_cond->waitlist,
                                               &p_cond->lock, ABT_FALSE,
                                               tar_time,
                                               ABT_SYNC_EVENT_TYPE_COND,
                                               (void *)p_cond);
    /* Lock the mutex again */
    ABTI_mutex_lock(&p_local, p_mutex);
    return is_timedout ? ABT_ERR_COND_TIMEDOUT : ABT_SUCCESS;
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
    ABTI_local *p_local = ABTI_local_get_local();
    ABTI_cond *p_cond = ABTI_cond_get_ptr(cond);
    ABTI_CHECK_NULL_COND_PTR(p_cond);

    ABTI_spinlock_acquire(&p_cond->lock);
    ABTI_waitlist_signal(p_local, &p_cond->waitlist);
    ABTI_spinlock_release(&p_cond->lock);

    return ABT_SUCCESS;
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
    ABTI_local *p_local = ABTI_local_get_local();
    ABTI_cond *p_cond = ABTI_cond_get_ptr(cond);
    ABTI_CHECK_NULL_COND_PTR(p_cond);

    ABTI_cond_broadcast(p_local, p_cond);
    return ABT_SUCCESS;
}

/*****************************************************************************/
/* Internal static functions                                                 */
/*****************************************************************************/

static inline double convert_timespec_to_sec(const struct timespec *p_ts)
{
    double secs;
    secs = ((double)p_ts->tv_sec) + 1.0e-9 * ((double)p_ts->tv_nsec);
    return secs;
}
