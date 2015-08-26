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
 * @param[out] newcond  handle to a new condition variable
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_cond_create(ABT_cond *newcond)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_cond *p_newcond;
    ABTI_thread_entry *p_entry;

    p_newcond = (ABTI_cond *)ABTU_malloc(sizeof(ABTI_cond));

    ABTI_mutex_init(&p_newcond->mutex);
    p_newcond->p_waiter_mutex = NULL;
    p_newcond->num_waiters  = 0;

    /* Allocate one entry for waiters and keep it */
    p_entry = (ABTI_thread_entry *)ABTU_malloc(sizeof(ABTI_thread_entry));
    p_entry->current = NULL;
    p_entry->next = NULL;
    p_newcond->waiters.head = p_entry;
    p_newcond->waiters.tail = p_entry;

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
    ABTI_CHECK_TRUE(p_cond->waiters.head != NULL, ABT_ERR_COND);

    /* The lock needs to be acquired to safely free the condition structure.
     * However, we do not have to unlock it because the entire structure is
     * freed here. */
    ABTI_mutex_spinlock(&p_cond->mutex);

    ABTU_free(p_cond->waiters.head);
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
    ABTI_cond *p_cond = ABTI_cond_get_ptr(cond);
    ABTI_CHECK_NULL_COND_PTR(p_cond);
    ABTI_mutex *p_mutex = ABTI_mutex_get_ptr(mutex);
    ABTI_CHECK_NULL_MUTEX_PTR(p_mutex);

    ABTI_thread *p_thread;
    ABT_unit_type type;
    volatile int ext_signal = 0;

    if (lp_ABTI_local != NULL) {
        p_thread = ABTI_local_get_thread();
        ABTI_CHECK_TRUE(p_thread != NULL, ABT_ERR_COND);
        type = ABT_UNIT_TYPE_THREAD;
    } else {
        /* external thread */
        p_thread = (ABTI_thread *)&ext_signal;
        type = ABT_UNIT_TYPE_EXT;
    }

    ABTI_mutex_spinlock(&p_cond->mutex);

    if (p_cond->p_waiter_mutex == NULL) {
        p_cond->p_waiter_mutex = p_mutex;
    } else {
        ABT_bool result = ABTI_mutex_equal(p_cond->p_waiter_mutex, p_mutex);
        if (result == ABT_FALSE) {
            ABTI_mutex_unlock(&p_cond->mutex);
            abt_errno = ABT_ERR_INV_MUTEX;
            goto fn_fail;
        }
    }

    ABTI_thread_entry *p_entry;
    if (p_cond->num_waiters == 0) {
        p_entry = p_cond->waiters.head;
        p_entry->current = p_thread;
    } else {
        p_entry = (ABTI_thread_entry *)ABTU_malloc(sizeof(ABTI_thread_entry));
        p_entry->current = p_thread;
        p_entry->next = NULL;

        p_cond->waiters.tail->next = p_entry;
        p_cond->waiters.tail = p_entry;
    }
    p_entry->type = type;

    p_cond->num_waiters++;

    if (type == ABT_UNIT_TYPE_THREAD) {
        /* Change the ULT's state to BLOCKED */
        ABTI_thread_set_blocked(p_thread);
    }

    ABTI_mutex_unlock(&p_cond->mutex);

    /* Unlock the mutex that the calling ULT is holding */
    /* FIXME: should check if mutex was locked by the calling ULT */
    ABTI_mutex_unlock(p_mutex);

    if (type == ABT_UNIT_TYPE_THREAD) {
        /* Suspend the current ULT */
        ABTI_thread_suspend(p_thread);
    } else {
        /* External thread is waiting here polling ext_signal. */
        /* FIXME: need a better implementation */
        while (!ext_signal);
    }

    /* Lock the mutex again */
    ABTI_mutex_lock(p_mutex);

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
    ABTI_cond *p_cond = ABTI_cond_get_ptr(cond);
    ABTI_CHECK_NULL_COND_PTR(p_cond);

    ABTI_mutex_lock(&p_cond->mutex);

    if (p_cond->num_waiters == 0) {
        ABTI_mutex_unlock(&p_cond->mutex);
        goto fn_exit;
    }

    /* Wake up the first waiting ULT */
    ABTI_thread_entry *head = p_cond->waiters.head;
    if (head->type == ABT_UNIT_TYPE_THREAD) {
        ABTI_thread_set_ready(head->current);
    } else {
        /* When the head is an external thread */
        volatile int *p_ext_signal = (volatile int *)head->current;
        *p_ext_signal = 1;
    }

    if (p_cond->num_waiters == 1) {
        head->current = NULL;
        p_cond->p_waiter_mutex = NULL;
    } else {
        p_cond->waiters.head = head->next;
        ABTU_free(head);
    }

    p_cond->num_waiters--;

    ABTI_mutex_unlock(&p_cond->mutex);

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
    ABTI_cond *p_cond = ABTI_cond_get_ptr(cond);
    ABTI_CHECK_NULL_COND_PTR(p_cond);

    ABTI_mutex_lock(&p_cond->mutex);

    if (p_cond->num_waiters == 0) {
        ABTI_mutex_unlock(&p_cond->mutex);
        goto fn_exit;
    }

    /* Wake up all waiting ULTs */
    /* We do not free the first entry */
    ABTI_thread_entry *head = p_cond->waiters.head;
    if (head->type == ABT_UNIT_TYPE_THREAD) {
        ABTI_thread_set_ready(head->current);
    } else {
        /* When the head is an external thread */
        volatile int *p_ext_signal = (volatile int *)head->current;
        *p_ext_signal = 1;
    }
    head->current = NULL;

    head = head->next;
    while (head != NULL) {
        if (head->type == ABT_UNIT_TYPE_THREAD) {
            ABTI_thread_set_ready(head->current);
        } else {
            /* When the head is an external thread */
            volatile int *p_ext_signal = (volatile int *)head->current;
            *p_ext_signal = 1;
        }
        ABTI_thread_entry *prev = head;
        head = head->next;
        ABTU_free(prev);
    }

    p_cond->waiters.head->next = NULL;
    p_cond->waiters.tail = p_cond->waiters.head;
    p_cond->num_waiters = 0;
    p_cond->p_waiter_mutex = NULL;

    ABTI_mutex_unlock(&p_cond->mutex);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

