/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"


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
 * @param[out] cond  handle to a new condition variable
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_cond_create(ABT_cond *newcond)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_cond *p_newcond;
    ABTI_thread_entry *entry;

    p_newcond = (ABTI_cond *)ABTU_malloc(sizeof(ABTI_cond));
    if (!p_newcond) {
        HANDLE_ERROR("ABTU_malloc");
        *newcond = ABT_COND_NULL;
        abt_errno = ABT_ERR_MEM;
        goto fn_fail;
    }

    abt_errno = ABT_mutex_create(&p_newcond->mutex);
    ABTI_CHECK_ERROR(abt_errno);

    p_newcond->waiter_mutex = ABT_MUTEX_NULL;
    p_newcond->num_waiters  = 0;

    /* Allocate one entry for waiters and keep it */
    entry = (ABTI_thread_entry *)ABTU_malloc(sizeof(ABTI_thread_entry));
    assert(entry != NULL);
    entry->current = ABT_THREAD_NULL;
    entry->next = NULL;
    p_newcond->waiters.head = entry;
    p_newcond->waiters.tail = entry;

    /* Return value */
    *newcond = ABTI_cond_get_handle(p_newcond);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_cond_create", abt_errno);
    goto fn_exit;
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

    assert(p_cond->num_waiters == 0);
    assert(p_cond->waiters.head != NULL);

    abt_errno = ABT_mutex_free(&p_cond->mutex);
    ABTI_CHECK_ERROR(abt_errno);

    ABTU_free(p_cond->waiters.head);
    ABTU_free(p_cond);

    /* Return value */
    *cond = ABT_COND_NULL;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_cond_free", abt_errno);
    goto fn_exit;
}

/**
 * @ingroup COND
 * @brief   Wait on the condition.
 *
 * The thread calling \c ABT_cond_wait() waits on the condition variable until
 * it is signaled.
 * The user should call this routine while the mutex specified as \c mutex is
 * locked. The mutex will be automatically released while waiting. After signal
 * is received and the waiting thread is awakened, the mutex will be
 * automatically locked for use by the thread. The user is then responsible for
 * unlocking mutex when the thread is finished with it.
 *
 * @param[in] cond   handle to the condition variable
 * @param[in] mutex  handle to the mutex
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_cond_wait(ABT_cond cond, ABT_mutex mutex)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_cond *p_cond = ABTI_cond_get_ptr(cond);
    ABTI_CHECK_NULL_COND_PTR(p_cond);

    /* FIXME: should check if mutex was locked by the calling thread */

    ABT_mutex_lock(p_cond->mutex);

    if (p_cond->waiter_mutex == ABT_MUTEX_NULL) {
        p_cond->waiter_mutex = mutex;
    } else {
        int result;
        ABTI_mutex_equal(p_cond->waiter_mutex, mutex, &result);
        if (!result) {
            abt_errno = ABT_ERR_INV_MUTEX;
            goto fn_fail;
        }
    }

    if (p_cond->num_waiters == 0) {
        ABTI_thread_entry *entry = p_cond->waiters.head;
        entry->current = ABTI_thread_get_handle(ABTI_local_get_thread());
    } else {
        ABTI_thread_entry *entry;
        entry = (ABTI_thread_entry *)ABTU_malloc(sizeof(ABTI_thread_entry));
        assert(entry != NULL);

        entry->current = ABTI_thread_get_handle(ABTI_local_get_thread());
        entry->next = NULL;

        p_cond->waiters.tail->next = entry;
        p_cond->waiters.tail = entry;
    }

    p_cond->num_waiters++;

    ABT_mutex_unlock(p_cond->mutex);

    /* Unlock the mutex that the calling thread is holding */
    ABT_mutex_unlock(mutex);

    /* Suspend the current thread */
    ABTI_thread_suspend();

    /* Lock the mutex again */
    ABT_mutex_lock(mutex);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_cond_wait", abt_errno);
    goto fn_exit;
}

/**
 * @ingroup COND
 * @brief   Signal a condition.
 *
 * \c ABT_cond_signal() signals another thread that is waiting on the condition
 * variable. Only one thread is waken up by the signal and the scheduler
 * determines the thread.
 * This routine shall have no effect if no threads are currently blocked on the
 * condition variable.
 *
 * @param[in] cond   handle to the condition variable
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_cond_signal(ABT_cond cond)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_cond *p_cond = ABTI_cond_get_ptr(cond);
    ABTI_CHECK_NULL_COND_PTR(p_cond);

    ABT_mutex_lock(p_cond->mutex);

    if (p_cond->num_waiters == 0) {
        ABT_mutex_unlock(p_cond->mutex);
        goto fn_exit;
    }

    /* Wake up the first waiting thread */
    ABTI_thread_entry *head = p_cond->waiters.head;
    ABTI_thread_set_ready(head->current);

    if (p_cond->num_waiters == 1) {
        head->current = ABT_THREAD_NULL;
        p_cond->waiter_mutex = ABT_MUTEX_NULL;
    } else {
        p_cond->waiters.head = head->next;
        ABTU_free(head);
    }

    p_cond->num_waiters--;

    ABT_mutex_unlock(p_cond->mutex);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_cond_signal", abt_errno);
    goto fn_exit;
}

/**
 * @ingroup COND
 * @brief   Broadcast a condition.
 *
 * \c ABT_cond_broadcast() signals all threads that are waiting on the
 * condition variable.
 * This routine shall have no effect if no threads are currently blocked on the
 * condition variable.
 *
 * @param[in] cond   handle to the condition variable
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_cond_broadcast(ABT_cond cond)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_cond *p_cond = ABTI_cond_get_ptr(cond);
    ABTI_CHECK_NULL_COND_PTR(p_cond);

    ABT_mutex_lock(p_cond->mutex);

    if (p_cond->num_waiters == 0) {
        ABT_mutex_unlock(p_cond->mutex);
        goto fn_exit;
    }

    /* Wake up all waiting threads */
    /* We do not free the first entry */
    ABTI_thread_entry *head = p_cond->waiters.head;
    ABTI_thread_set_ready(head->current);
    head->current = ABT_THREAD_NULL;

    head = head->next;
    while (head != NULL) {
        ABTI_thread_set_ready(head->current);
        ABTI_thread_entry *prev = head;
        head = head->next;
        ABTU_free(prev);
    }

    p_cond->waiters.head->next = NULL;
    p_cond->waiters.tail = p_cond->waiters.head;
    p_cond->num_waiters = 0;
    p_cond->waiter_mutex = ABT_MUTEX_NULL;

    ABT_mutex_unlock(p_cond->mutex);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_cond_broadcast", abt_errno);
    goto fn_exit;
}


/*****************************************************************************/
/* Private APIs                                                              */
/*****************************************************************************/

ABTI_cond *ABTI_cond_get_ptr(ABT_cond cond)
{
    ABTI_cond *p_cond;
    if (cond == ABT_COND_NULL) {
        p_cond = NULL;
    } else {
        p_cond = (ABTI_cond *)cond;
    }
    return p_cond;
}

ABT_cond ABTI_cond_get_handle(ABTI_cond *p_cond)
{
    ABT_cond h_cond;
    if (p_cond == NULL) {
        h_cond = ABT_COND_NULL;
    } else {
        h_cond = (ABT_cond)p_cond;
    }
    return h_cond;
}

