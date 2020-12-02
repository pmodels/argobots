/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"

static inline void mutex_lock_low(ABTI_local **pp_local, ABTI_mutex *p_mutex);
static inline void mutex_unlock_se(ABTI_local **pp_local, ABTI_mutex *p_mutex);

/** @defgroup MUTEX Mutex
 * Mutex is a synchronization method to support mutual exclusion between ULTs.
 * When more than one ULT competes for locking the same mutex, only one ULT is
 * guaranteed to lock the mutex.  Other ULTs are blocked and wait until the ULT
 * which locked the mutex unlocks it.  When the mutex is unlocked, another ULT
 * is able to lock the mutex again.
 *
 * The mutex is basically intended to be used by ULTs but it can also be used
 * by tasklets or external threads.  In that case, the mutex will behave like
 * a spinlock.
 */

/**
 * @ingroup MUTEX
 * @brief   Create a new mutex.
 *
 * \c ABT_mutex_create() creates a new mutex object with default attributes and
 * returns its handle through \c newmutex.  To set different attributes, please
 * use \c ABT_mutex_create_with_attr().  If an error occurs in this routine,
 * a non-zero error code will be returned and \c newmutex will be set to
 * \c ABT_MUTEX_NULL.
 *
 * @param[out] newmutex  handle to a new mutex
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_mutex_create(ABT_mutex *newmutex)
{
    int abt_errno;
    ABTI_mutex *p_newmutex;

    abt_errno = ABTU_calloc(1, sizeof(ABTI_mutex), (void **)&p_newmutex);
    ABTI_CHECK_ERROR(abt_errno);
    abt_errno = ABTI_mutex_init(p_newmutex);
    if (ABTI_IS_ERROR_CHECK_ENABLED && abt_errno != ABT_SUCCESS) {
        ABTU_free(p_newmutex);
        ABTI_HANDLE_ERROR(abt_errno);
    }

    /* Return value */
    *newmutex = ABTI_mutex_get_handle(p_newmutex);
    return ABT_SUCCESS;
}

/**
 * @ingroup MUTEX
 * @brief   Create a new mutex with attributes.
 *
 * \c ABT_mutex_create_with_attr() creates a new mutex object having attributes
 * passed by \c attr and returns its handle through \c newmutex.  Note that
 * \c ABT_mutex_create() can be used to create a mutex with default attributes.
 *
 * If an error occurs in this routine, a non-zero error code will be returned
 * and \c newmutex will be set to \c ABT_MUTEX_NULL.
 *
 * @param[in]  attr      handle to the mutex attribute object
 * @param[out] newmutex  handle to a new mutex
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_mutex_create_with_attr(ABT_mutex_attr attr, ABT_mutex *newmutex)
{
    int abt_errno;
    ABTI_mutex_attr *p_attr = ABTI_mutex_attr_get_ptr(attr);
    ABTI_CHECK_NULL_MUTEX_ATTR_PTR(p_attr);
    ABTI_mutex *p_newmutex;

    abt_errno = ABTU_malloc(sizeof(ABTI_mutex), (void **)&p_newmutex);
    ABTI_CHECK_ERROR(abt_errno);
    abt_errno = ABTI_mutex_init(p_newmutex);
    if (ABTI_IS_ERROR_CHECK_ENABLED && abt_errno != ABT_SUCCESS) {
        ABTU_free(p_newmutex);
        ABTI_HANDLE_ERROR(abt_errno);
    }
    memcpy(&p_newmutex->attr, p_attr, sizeof(ABTI_mutex_attr));

    /* Return value */
    *newmutex = ABTI_mutex_get_handle(p_newmutex);
    return ABT_SUCCESS;
}

/**
 * @ingroup MUTEX
 * @brief   Free the mutex object.
 *
 * \c ABT_mutex_free() deallocates the memory used for the mutex object
 * associated with the handle \c mutex.  If it is successfully processed,
 * \c mutex is set to \c ABT_MUTEX_NULL.
 *
 * Using the mutex handle after calling \c ABT_mutex_free() may cause
 * undefined behavior.
 *
 * @param[in,out] mutex  handle to the mutex
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_mutex_free(ABT_mutex *mutex)
{
    ABT_mutex h_mutex = *mutex;
    ABTI_mutex *p_mutex = ABTI_mutex_get_ptr(h_mutex);
    ABTI_CHECK_NULL_MUTEX_PTR(p_mutex);

    ABTU_free(p_mutex);

    /* Return value */
    *mutex = ABT_MUTEX_NULL;
    return ABT_SUCCESS;
}

/**
 * @ingroup MUTEX
 * @brief   Lock the mutex.
 *
 * \c ABT_mutex_lock() locks the mutex \c mutex.  If this routine successfully
 * returns, the caller work unit acquires the mutex.  If the mutex has already
 * been locked, the caller will be blocked until the mutex becomes available.
 * When the caller is a ULT and is blocked, the context is switched to the
 * scheduler of the associated ES to make progress of other work units.
 *
 * The mutex can be used by any work units, but tasklets are discouraged to use
 * the mutex because any blocking calls like \c ABT_mutex_lock() may block the
 * associated ES and prevent other work units from being scheduled on the ES.
 *
 * @param[in] mutex  handle to the mutex
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_mutex_lock(ABT_mutex mutex)
{
    ABTI_local *p_local = ABTI_local_get_local();
    ABTI_mutex *p_mutex = ABTI_mutex_get_ptr(mutex);
    ABTI_CHECK_NULL_MUTEX_PTR(p_mutex);

    if (p_mutex->attr.attrs == ABTI_MUTEX_ATTR_NONE) {
        /* default attributes */
        ABTI_mutex_lock(&p_local, p_mutex);

    } else if (p_mutex->attr.attrs & ABTI_MUTEX_ATTR_RECURSIVE) {
        /* recursive mutex */
        ABTI_thread_id self_id = ABTI_self_get_thread_id(p_local);
        if (self_id != p_mutex->attr.owner_id) {
            ABTI_mutex_lock(&p_local, p_mutex);
            p_mutex->attr.owner_id = self_id;
            ABTI_ASSERT(p_mutex->attr.nesting_cnt == 0);
        } else {
            p_mutex->attr.nesting_cnt++;
        }

    } else {
        /* unknown attributes */
        ABTI_mutex_lock(&p_local, p_mutex);
    }
    return ABT_SUCCESS;
}

/**
 * @ingroup MUTEX
 * @brief   Lock the mutex with low priority.
 *
 * \c ABT_mutex_lock_low() locks the mutex with low priority, while
 * \c ABT_mutex_lock() does with high priority.  Apart from the priority,
 * other semantics of \c ABT_mutex_lock_low() are the same as those of
 * \c ABT_mutex_lock().
 *
 * @param[in] mutex  handle to the mutex
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_mutex_lock_low(ABT_mutex mutex)
{
    ABTI_local *p_local = ABTI_local_get_local();
    ABTI_mutex *p_mutex = ABTI_mutex_get_ptr(mutex);
    ABTI_CHECK_NULL_MUTEX_PTR(p_mutex);

    if (p_mutex->attr.attrs == ABTI_MUTEX_ATTR_NONE) {
        /* default attributes */
        mutex_lock_low(&p_local, p_mutex);

    } else if (p_mutex->attr.attrs & ABTI_MUTEX_ATTR_RECURSIVE) {
        /* recursive mutex */
        ABTI_thread_id self_id = ABTI_self_get_thread_id(p_local);
        if (self_id != p_mutex->attr.owner_id) {
            mutex_lock_low(&p_local, p_mutex);
            p_mutex->attr.owner_id = self_id;
            ABTI_ASSERT(p_mutex->attr.nesting_cnt == 0);
        } else {
            p_mutex->attr.nesting_cnt++;
        }

    } else {
        /* unknown attributes */
        mutex_lock_low(&p_local, p_mutex);
    }
    return ABT_SUCCESS;
}

int ABT_mutex_lock_high(ABT_mutex mutex)
{
    return ABT_mutex_lock(mutex);
}

/**
 * @ingroup MUTEX
 * @brief   Attempt to lock a mutex without blocking.
 *
 * \c ABT_mutex_trylock() attempts to lock the mutex \c mutex without blocking
 * the caller work unit.  If this routine successfully returns, the caller
 * acquires the mutex.
 *
 * If the mutex has already been locked and there happens no error,
 * \c ABT_ERR_MUTEX_LOCKED will be returned immediately without blocking
 * the caller.
 *
 * @param[in] mutex  handle to the mutex
 * @return Error code
 * @retval ABT_SUCCESS          on success
 * @retval ABT_ERR_MUTEX_LOCKED when mutex has already been locked
 */
int ABT_mutex_trylock(ABT_mutex mutex)
{
    ABTI_mutex *p_mutex = ABTI_mutex_get_ptr(mutex);
    ABTI_CHECK_NULL_MUTEX_PTR(p_mutex);

    int abt_errno;
    if (p_mutex->attr.attrs == ABTI_MUTEX_ATTR_NONE) {
        /* default attributes */
        abt_errno = ABTI_mutex_trylock(p_mutex);

    } else if (p_mutex->attr.attrs & ABTI_MUTEX_ATTR_RECURSIVE) {
        /* recursive mutex */
        ABTI_local *p_local = ABTI_local_get_local();
        ABTI_thread_id self_id = ABTI_self_get_thread_id(p_local);
        if (self_id != p_mutex->attr.owner_id) {
            abt_errno = ABTI_mutex_trylock(p_mutex);
            if (abt_errno == ABT_SUCCESS) {
                p_mutex->attr.owner_id = self_id;
                ABTI_ASSERT(p_mutex->attr.nesting_cnt == 0);
            }
        } else {
            p_mutex->attr.nesting_cnt++;
            abt_errno = ABT_SUCCESS;
        }

    } else {
        /* unknown attributes */
        abt_errno = ABTI_mutex_trylock(p_mutex);
    }
    /* Trylock always needs to return an error code. */
    return abt_errno;
}

/**
 * @ingroup MUTEX
 * @brief   Lock the mutex without context switch.
 *
 * \c ABT_mutex_spinlock() locks the mutex without context switch.  If this
 * routine successfully returns, the caller thread acquires the mutex.
 * If the mutex has already been locked, the caller will be blocked until
 * the mutex becomes available.  Unlike \c ABT_mutex_lock(), the ULT calling
 * this routine continuously tries to lock the mutex without context switch.
 *
 * @param[in] mutex  handle to the mutex
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_mutex_spinlock(ABT_mutex mutex)
{
    ABTI_mutex *p_mutex = ABTI_mutex_get_ptr(mutex);
    ABTI_CHECK_NULL_MUTEX_PTR(p_mutex);

    if (p_mutex->attr.attrs == ABTI_MUTEX_ATTR_NONE) {
        /* default attributes */
        ABTI_mutex_spinlock(p_mutex);

    } else if (p_mutex->attr.attrs & ABTI_MUTEX_ATTR_RECURSIVE) {
        /* recursive mutex */
        ABTI_local *p_local = ABTI_local_get_local();
        ABTI_thread_id self_id = ABTI_self_get_thread_id(p_local);
        if (self_id != p_mutex->attr.owner_id) {
            ABTI_mutex_spinlock(p_mutex);
            p_mutex->attr.owner_id = self_id;
            ABTI_ASSERT(p_mutex->attr.nesting_cnt == 0);
        } else {
            p_mutex->attr.nesting_cnt++;
        }

    } else {
        /* unknown attributes */
        ABTI_mutex_spinlock(p_mutex);
    }
    return ABT_SUCCESS;
}

/**
 * @ingroup MUTEX
 * @brief   Unlock the mutex.
 *
 * \c ABT_mutex_unlock() unlocks the mutex \c mutex.  If the caller locked the
 * mutex, this routine unlocks the mutex.  However, if the caller did not lock
 * the mutex, this routine may result in undefined behavior.
 *
 * @param[in] mutex  handle to the mutex
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_mutex_unlock(ABT_mutex mutex)
{
    ABTI_local *p_local = ABTI_local_get_local();
    ABTI_mutex *p_mutex = ABTI_mutex_get_ptr(mutex);
    ABTI_CHECK_NULL_MUTEX_PTR(p_mutex);

    if (p_mutex->attr.attrs == ABTI_MUTEX_ATTR_NONE) {
        /* default attributes */
        ABTI_mutex_unlock(p_local, p_mutex);

    } else if (p_mutex->attr.attrs & ABTI_MUTEX_ATTR_RECURSIVE) {
        /* recursive mutex */
        ABTI_CHECK_TRUE(ABTI_self_get_thread_id(p_local) ==
                            p_mutex->attr.owner_id,
                        ABT_ERR_INV_THREAD);
        if (p_mutex->attr.nesting_cnt == 0) {
            p_mutex->attr.owner_id = 0;
            ABTI_mutex_unlock(p_local, p_mutex);
        } else {
            p_mutex->attr.nesting_cnt--;
        }

    } else {
        /* unknown attributes */
        ABTI_mutex_unlock(p_local, p_mutex);
    }
    return ABT_SUCCESS;
}

/**
 * @ingroup MUTEX
 * @brief   Hand over the mutex within the ES.
 *
 * \c ABT_mutex_unlock_se() first tries to hand over the mutex to a ULT, which
 * is waiting for this mutex and is running on the same ES as the caller.  If
 * no ULT on the same ES is waiting, it unlocks the mutex like
 * \c ABT_mutex_unlock().
 *
 * If the caller ULT locked the mutex, this routine unlocks the mutex.
 * However, if the caller ULT did not lock the mutex, this routine may result
 * in undefined behavior.
 *
 * @param[in] mutex  handle to the mutex
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_mutex_unlock_se(ABT_mutex mutex)
{
    ABTI_local *p_local = ABTI_local_get_local();
    ABTI_mutex *p_mutex = ABTI_mutex_get_ptr(mutex);
    ABTI_CHECK_NULL_MUTEX_PTR(p_mutex);

    if (p_mutex->attr.attrs == ABTI_MUTEX_ATTR_NONE) {
        /* default attributes */
        mutex_unlock_se(&p_local, p_mutex);

    } else if (p_mutex->attr.attrs & ABTI_MUTEX_ATTR_RECURSIVE) {
        /* recursive mutex */
        ABTI_CHECK_TRUE(ABTI_self_get_thread_id(p_local) ==
                            p_mutex->attr.owner_id,
                        ABT_ERR_INV_THREAD);
        if (p_mutex->attr.nesting_cnt == 0) {
            p_mutex->attr.owner_id = 0;
            mutex_unlock_se(&p_local, p_mutex);
        } else {
            p_mutex->attr.nesting_cnt--;
        }

    } else {
        /* unknown attributes */
        mutex_unlock_se(&p_local, p_mutex);
    }
    return ABT_SUCCESS;
}

int ABT_mutex_unlock_de(ABT_mutex mutex)
{
    ABTI_local *p_local = ABTI_local_get_local();
    ABTI_mutex *p_mutex = ABTI_mutex_get_ptr(mutex);
    ABTI_CHECK_NULL_MUTEX_PTR(p_mutex);

    ABTI_mutex_unlock(p_local, p_mutex);
    return ABT_SUCCESS;
}

/**
 * @ingroup MUTEX
 * @brief   Compare two mutex handles for equality.
 *
 * \c ABT_mutex_equal() compares two mutex handles for equality.  If two
 * handles are associated with the same mutex object, \c result will be set to
 * \c ABT_TRUE.  Otherwise, \c result will be set to \c ABT_FALSE.
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

/*****************************************************************************/
/* Internal static functions                                                 */
/*****************************************************************************/

static inline void mutex_lock_low(ABTI_local **pp_local, ABTI_mutex *p_mutex)
{
    ABTI_ythread *p_ythread = NULL;
    ABTI_xstream *p_local_xstream = ABTI_local_get_xstream_or_null(*pp_local);
    if (!ABTI_IS_EXT_THREAD_ENABLED || p_local_xstream) {
        p_ythread = ABTI_thread_get_ythread_or_null(p_local_xstream->p_thread);
    }
    if (p_ythread) {
        LOG_DEBUG("%p: lock_low - try\n", p_mutex);
        while (!ABTD_atomic_bool_cas_strong_uint32(&p_mutex->val, 0, 1)) {
            ABTI_ythread_yield(&p_local_xstream, p_ythread,
                               ABT_SYNC_EVENT_TYPE_MUTEX, (void *)p_mutex);
            *pp_local = ABTI_xstream_get_local(p_local_xstream);
        }
        LOG_DEBUG("%p: lock_low - acquired\n", p_mutex);
    } else {
        ABTI_mutex_spinlock(p_mutex);
    }
}

/* Hand over the mutex to other ULT on the same ES */
static inline void mutex_unlock_se(ABTI_local **pp_local, ABTI_mutex *p_mutex)
{
    ABTI_ythread *p_ythread = NULL;
    ABTI_xstream *p_local_xstream = ABTI_local_get_xstream_or_null(*pp_local);
    if (!ABTI_IS_EXT_THREAD_ENABLED || p_local_xstream) {
        p_ythread = ABTI_thread_get_ythread_or_null(p_local_xstream->p_thread);
    }
    ABTD_atomic_release_store_uint32(&p_mutex->val, 0);
    LOG_DEBUG("%p: unlock_se\n", p_mutex);
    if (p_ythread) {
        ABTI_ythread_yield(&p_local_xstream, p_ythread,
                           ABT_SYNC_EVENT_TYPE_MUTEX, (void *)p_mutex);
        *pp_local = ABTI_xstream_get_local(p_local_xstream);
    }
}
