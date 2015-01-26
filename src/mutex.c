/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"


/** @defgroup MUTEX Mutex
 * This group is for Mutex.
 */

/**
 * @ingroup MUTEX
 * @brief   Create a new mutex.
 *
 * @param[out] newmutex  handle to a new mutex
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_mutex_create(ABT_mutex *newmutex)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_mutex *p_newmutex;

    p_newmutex = (ABTI_mutex *)ABTU_malloc(sizeof(ABTI_mutex));
    if (!p_newmutex) {
        HANDLE_ERROR("ABTU_malloc");
        *newmutex = ABT_MUTEX_NULL;
        abt_errno = ABT_ERR_MEM;
        goto fn_fail;
    }

    p_newmutex->val = 0;

    /* Return value */
    *newmutex = ABTI_mutex_get_handle(p_newmutex);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_mutex_create", abt_errno);
    goto fn_exit;
}

/**
 * @ingroup MUTEX
 * @brief   Release the mutex object associated with mutex handle.
 *
 * If this routine successfully returns, mutex is set as ABT_MUTEX_NULL.
 *
 * @param[in,out] mutex  handle to the mutex
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_mutex_free(ABT_mutex *mutex)
{
    int abt_errno = ABT_SUCCESS;
    ABT_mutex h_mutex = *mutex;
    ABTI_mutex *p_mutex = ABTI_mutex_get_ptr(h_mutex);
    ABTI_CHECK_NULL_MUTEX_PTR(p_mutex);

    ABTU_free(p_mutex);

    /* Return value */
    *mutex = ABT_MUTEX_NULL;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_mutex_free", abt_errno);
    goto fn_exit;
}

/**
 * @ingroup MUTEX
 * @brief   Lock the mutex.
 *
 * If the mutex is already locked, the calling thread will block until it
 * becomes available.
 *
 * @param[in] mutex  handle to the mutex
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_mutex_lock(ABT_mutex mutex)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_mutex *p_mutex = ABTI_mutex_get_ptr(mutex);
    ABTI_CHECK_NULL_MUTEX_PTR(p_mutex);

    while (ABTD_atomic_cas_uint32(&p_mutex->val, 0, 1) != 0) {
        ABT_thread_yield();
    }

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_mutex_lock", abt_errno);
    goto fn_exit;
}

/**
 * @ingroup MUTEX
 * @brief   Lock the mutex.
 *
 * If the mutex is already locked, the calling thread will block until it
 * becomes available.
 *
 * @param[in] mutex  handle pointer to the mutex
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_mutex_lock_ptr(ABT_mutex *mutex)
{
    return ABT_mutex_lock(*mutex);
}

/**
 * @ingroup MUTEX
 * @brief   Attempt to lock a mutex without blocking.
 *
 * If the mutex is already locked, ABT_ERR_MUTEX_LOCKED will be returned
 * immediately without blocking the calling thread.
 *
 * @param[in] mutex  handle to the mutex
 * @return Error code
 * @retval ABT_SUCCESS on success
 * @retval ABT_ERR_MUTEX_LOCKED when mutex has already been locked
 */
int ABT_mutex_trylock(ABT_mutex mutex)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_mutex *p_mutex = ABTI_mutex_get_ptr(mutex);
    ABTI_CHECK_NULL_MUTEX_PTR(p_mutex);

    if (ABTD_atomic_cas_uint32(&p_mutex->val, 0, 1) != 0) {
        abt_errno = ABT_ERR_MUTEX_LOCKED;
    }

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_mutex_trylock", abt_errno);
    goto fn_exit;
}

/**
 * @ingroup MUTEX
 * @brief   Unlock the mutex.
 *
 * If the calling thread locked the mutex, this function unlocks the mutex.
 * However, if the calling thread did not lock the mutex, this function may
 * result in undefined behavior.
 *
 * @param[in] mutex  handle to the mutex
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_mutex_unlock(ABT_mutex mutex)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_mutex *p_mutex = ABTI_mutex_get_ptr(mutex);
    ABTI_CHECK_NULL_MUTEX_PTR(p_mutex);

    p_mutex->val = 0;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_mutex_unlock", abt_errno);
    goto fn_exit;
}

/**
 * @ingroup MUTEX
 * @brief   Unlock the mutex.
 *
 * If the calling thread locked the mutex, this function unlocks the mutex.
 * However, if the calling thread did not lock the mutex, this function may
 * result in undefined behavior.
 *
 * @param[in] mutex  handle pointer to the mutex
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_mutex_unlock_ptr(ABT_mutex *mutex)
{
    return ABT_mutex_unlock(*mutex);
}

/**
 * @ingroup MUTEX
 * @brief   Compare two mutex handles for equality.
 *
 * \c ABT_mutex_equal() compares two mutex handles for equality. If two handles
 * are associated with the same mutex object, \c result will be set to
 * \c ABT_TRUE. Otherwise, \c result will be set to \c ABT_FALSE.
 *
 * @param[in]  mutex1  handle to the mutex 1
 * @param[in]  mutex2  handle to the mutex 2
 * @param[out] result  comparison result (<tt>ABT_TRUE</tt>: same,
 *                     <tt>ABT_FALSE</tt>: not same)
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_mutex_equal(ABT_mutex mutex1, ABT_mutex mutex2, ABT_bool *result)
{
    ABTI_mutex *p_mutex1 = ABTI_mutex_get_ptr(mutex1);
    ABTI_mutex *p_mutex2 = ABTI_mutex_get_ptr(mutex2);
    *result = (p_mutex1 == p_mutex2) ? ABT_TRUE : ABT_FALSE;
    return ABT_SUCCESS;
}

int ABT_mutex_waitlock(ABT_mutex mutex)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_mutex *p_mutex = ABTI_mutex_get_ptr(mutex);
    while (ABTD_atomic_cas_uint32(&p_mutex->val, 0, 1) != 0) {
    }
    return abt_errno;
}


/*****************************************************************************/
/* Private APIs                                                              */
/*****************************************************************************/

ABTI_mutex *ABTI_mutex_get_ptr(ABT_mutex mutex)
{
    ABTI_mutex *p_mutex;
    if (mutex == ABT_MUTEX_NULL) {
        p_mutex = NULL;
    } else {
        p_mutex = (ABTI_mutex *)mutex;
    }
    return p_mutex;
}

ABT_mutex ABTI_mutex_get_handle(ABTI_mutex *p_mutex)
{
    ABT_mutex h_mutex;
    if (p_mutex == NULL) {
        h_mutex = ABT_MUTEX_NULL;
    } else {
        h_mutex = (ABT_mutex)p_mutex;
    }
    return h_mutex;
}

