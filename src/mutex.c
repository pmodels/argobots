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
int ABT_Mutex_create(ABT_Mutex *newmutex)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_Mutex *p_newmutex;

    p_newmutex = (ABTI_Mutex *)ABTU_Malloc(sizeof(ABTI_Mutex));
    if (!p_newmutex) {
        HANDLE_ERROR("ABTU_Malloc");
        *newmutex = ABT_MUTEX_NULL;
        abt_errno = ABT_ERR_MEM;
        goto fn_fail;
    }

    p_newmutex->val = 0;

    /* Return value */
    *newmutex = ABTI_Mutex_get_handle(p_newmutex);

  fn_exit:
    return abt_errno;

  fn_fail:
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
int ABT_Mutex_free(ABT_Mutex *mutex)
{
    int abt_errno = ABT_SUCCESS;
    ABT_Mutex h_mutex = *mutex;
    ABTI_Mutex *p_mutex = ABTI_Mutex_get_ptr(h_mutex);
    ABTU_Free(p_mutex);
    *mutex = ABT_MUTEX_NULL;
    return abt_errno;
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
int ABT_Mutex_lock(ABT_Mutex mutex)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_Mutex *p_mutex = ABTI_Mutex_get_ptr(mutex);
    while (ABTD_Atomic_cas_uint32(&p_mutex->val, 0, 1) != 0) {
        ABT_Thread_yield();
    }
    return abt_errno;
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
int ABT_Mutex_trylock(ABT_Mutex mutex)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_Mutex *p_mutex = ABTI_Mutex_get_ptr(mutex);
    if (ABTD_Atomic_cas_uint32(&p_mutex->val, 0, 1) != 0) {
        abt_errno = ABT_ERR_MUTEX_LOCKED;
    }
    return abt_errno;
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
int ABT_Mutex_unlock(ABT_Mutex mutex)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_Mutex *p_mutex = ABTI_Mutex_get_ptr(mutex);
    p_mutex->val = 0;
    return abt_errno;
}


/* Private APIs */
ABTI_Mutex *ABTI_Mutex_get_ptr(ABT_Mutex mutex)
{
    ABTI_Mutex *p_mutex;
    if (mutex == ABT_MUTEX_NULL) {
        p_mutex = NULL;
    } else {
        p_mutex = (ABTI_Mutex *)mutex;
    }
    return p_mutex;
}

ABT_Mutex ABTI_Mutex_get_handle(ABTI_Mutex *p_mutex)
{
    ABT_Mutex h_mutex;
    if (p_mutex == NULL) {
        h_mutex = ABT_MUTEX_NULL;
    } else {
        h_mutex = (ABT_Mutex)p_mutex;
    }
    return h_mutex;
}

int ABTI_Mutex_waitlock(ABT_Mutex mutex)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_Mutex *p_mutex = ABTI_Mutex_get_ptr(mutex);
    while (ABTD_Atomic_cas_uint32(&p_mutex->val, 0, 1) != 0) {
    }
    return abt_errno;
}

