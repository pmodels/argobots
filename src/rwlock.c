/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"

/** @defgroup RWLOCK Readers Writer Lock
 * A Readers writer lock allows concurrent access for readers and exclusionary
 * access for writers.
 */

/**
 * @ingroup RWLOCK
 * @brief Create a new rwlock
 * \c ABT_rwlock_create creates a new rwlock object and returns its handle
 * through \c newrwlock. If an error occurs in this routine, a non-zero error
 * code will be returned and \c newrwlock will be set to \c ABT_RWLOCK_NULL.
 *
 * Only ULTs can use the rwlock, and tasklets must not use it.
 *
 * @param[out] newrwlock  handle to a new rwlock
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_rwlock_create(ABT_rwlock *newrwlock)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_rwlock *p_newrwlock;

    p_newrwlock = (ABTI_rwlock *)ABTU_malloc(sizeof(ABTI_rwlock));
    if (p_newrwlock == NULL) {
        abt_errno = ABT_ERR_MEM;
    }
    else {
        ABTI_rwlock_init(p_newrwlock);
    }

    /* Return value */
    *newrwlock = ABTI_rwlock_get_handle(p_newrwlock);

    return abt_errno;
}

/**
 * @ingroup RWLOCK
 * @brief   Free the rwlock object.
 *
 * \c ABT_rwlock_free deallocates the memory used for the rwlock object
 * associated with the handle \c rwlock. If it is successfully processed,
 * \c rwlock is set to \c ABT_RWLOCK_NULL.
 *
 * Using the rwlock handle after calling \c ABT_rwlock_free may cause
 * undefined behavior.
 *
 * @param[in,out] rwlock  handle to the rwlock
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_rwlock_free(ABT_rwlock *rwlock)
{
    int abt_errno = ABT_SUCCESS;
    ABT_rwlock h_rwlock = *rwlock;
    ABTI_rwlock *p_rwlock = ABTI_rwlock_get_ptr(h_rwlock);
    ABTI_CHECK_NULL_RWLOCK_PTR(p_rwlock);

    ABTI_rwlock_fini(p_rwlock);
    ABTU_free(p_rwlock);

    /* Return value */
    *rwlock = ABT_RWLOCK_NULL;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

/**
 * @ingroup RWLOCK
 * @brief   Lock the rwlock as a reader.
 *
 * \c ABT_rwlock_rdlock locks the rwlock \c rwlock. If this routine successfully
 * returns, the caller ULT acquires the rwlock. If the rwlock has been locked
 * by a writer, the caller ULT will be blocked until the rwlock becomes
 * available. rwlocks may be acquired by any number of readers concurrently.
 * When the caller ULT is blocked, the context is switched to the scheduler
 * of the associated ES to make progress of other work units.
 *
 * The rwlock can be used only by ULTs. Tasklets must not call any blocking
 * routines like \c ABT_rwlock_rdlock.
 *
 * @param[in] rwlock  handle to the rwlock
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_rwlock_rdlock(ABT_rwlock rwlock)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_rwlock *p_rwlock = ABTI_rwlock_get_ptr(rwlock);
    ABTI_CHECK_NULL_RWLOCK_PTR(p_rwlock);

    ABTI_rwlock_rdlock(p_rwlock);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

/**
 * @ingroup RWLOCK
 * @brief   Lock the rwlock as a writer.
 *
 * \c ABT_rwlock_wrlock locks the rwlock \c rwlock. If this routine successfully
 * returns, the caller ULT acquires the rwlock. If the rwlock has been locked
 * by a reader or a writer, the caller ULT will be blocked until the rwlock
 * becomes available. When the caller ULT is blocked, the context is switched
 * to the scheduler of the associated ES to make progress of other work units.
 *
 * The rwlock can be used only by ULTs. Tasklets must not call any blocking
 * routines like \c ABT_rwlock_wrlock.
 *
 * @param[in] rwlock  handle to the rwlock
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_rwlock_wrlock(ABT_rwlock rwlock)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_rwlock *p_rwlock = ABTI_rwlock_get_ptr(rwlock);
    ABTI_CHECK_NULL_RWLOCK_PTR(p_rwlock);

    ABTI_rwlock_wrlock(p_rwlock);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

/**
 * @ingroup RWLOCK
 * @brief Unlock the rwlock
 *
 * \c ABT_rwlock_unlock unlocks the rwlock \c rwlock.
 * If the caller ULT locked the rwlock, this routine unlocks the rwlock. However,
 * if the caller ULT did not lock the rwlock, this routine may result in
 * undefined behavior.
 *
 * @param[in] rwlock  handle to the rwlock
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_rwlock_unlock(ABT_rwlock rwlock)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_rwlock *p_rwlock = ABTI_rwlock_get_ptr(rwlock);
    ABTI_CHECK_NULL_RWLOCK_PTR(p_rwlock);

    ABTI_rwlock_unlock(p_rwlock);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}
