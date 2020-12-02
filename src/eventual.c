/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"

/** @defgroup EVENTUAL Eventual
 * In Argobots, an \a eventual corresponds to the traditional behavior of
 * the future concept (refer to \ref FUTURE "Future"). A ULT creates an
 * eventual, which is a memory buffer that will eventually contain a value
 * of interest. Many ULTs can wait on the eventual (a blocking call),
 * until one ULT signals on that future.
 */

/**
 * @ingroup EVENTUAL
 * @brief   Create an eventual.
 *
 * \c ABT_eventual_create creates an eventual and returns a handle to the newly
 * created eventual into \c neweventual.  If \c nbytes is not zero, this routine
 * allocates a memory buffer of \c nbytes size and creates a list of entries
 * for all the ULTs that will be blocked waiting for the eventual to be ready.
 * The list is initially empty.  If \c nbytes is zero, the eventual is used
 * without passing the data.
 *
 * @param[in]  nbytes       size in bytes of the memory buffer
 * @param[out] neweventual  handle to a new eventual
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_eventual_create(int nbytes, ABT_eventual *neweventual)
{
    int abt_errno;
    ABTI_eventual *p_eventual;
    size_t arg_nbytes = nbytes;

    abt_errno = ABTU_malloc(sizeof(ABTI_eventual), (void **)&p_eventual);
    ABTI_CHECK_ERROR(abt_errno);

    ABTI_spinlock_clear(&p_eventual->lock);
    p_eventual->ready = ABT_FALSE;
    p_eventual->nbytes = arg_nbytes;
    if (arg_nbytes == 0) {
        p_eventual->value = NULL;
    } else {
        abt_errno = ABTU_malloc(arg_nbytes, &p_eventual->value);
        if (ABTI_IS_ERROR_CHECK_ENABLED && abt_errno != ABT_SUCCESS) {
            ABTU_free(p_eventual);
            ABTI_HANDLE_ERROR(abt_errno);
        }
    }
    ABTI_waitlist_init(&p_eventual->waitlist);

    *neweventual = ABTI_eventual_get_handle(p_eventual);
    return ABT_SUCCESS;
}

/**
 * @ingroup EVENTUAL
 * @brief   Free the eventual object.
 *
 * \c ABT_eventual_free releases memory associated with the eventual
 * \c eventual. It also deallocates the memory buffer of the eventual.
 * If it is successfully processed, \c eventual is set to \c ABT_EVENTUAL_NULL.
 *
 * @param[in,out] eventual  handle to the eventual
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_eventual_free(ABT_eventual *eventual)
{
    ABTI_eventual *p_eventual = ABTI_eventual_get_ptr(*eventual);
    ABTI_CHECK_NULL_EVENTUAL_PTR(p_eventual);

    /* The lock needs to be acquired to safely free the eventual structure.
     * However, we do not have to unlock it because the entire structure is
     * freed here. */
    ABTI_spinlock_acquire(&p_eventual->lock);

    if (p_eventual->value)
        ABTU_free(p_eventual->value);
    ABTU_free(p_eventual);

    *eventual = ABT_EVENTUAL_NULL;
    return ABT_SUCCESS;
}

/**
 * @ingroup EVENTUAL
 * @brief   Wait on the eventual.
 *
 * \c ABT_eventual_wait blocks the caller ULT until the eventual \c eventual
 * is resolved. If the eventual is not ready, the ULT calling this routine
 * suspends and goes to the state BLOCKED. Internally, an entry is created
 * per each blocked ULT to be awaken when the eventual is signaled.
 * If the eventual is ready, the pointer pointed to by \c value will point to
 * the memory buffer associated with the eventual. The system keeps a list of
 * all the ULTs waiting on the eventual.
 *
 * @param[in]  eventual handle to the eventual
 * @param[out] value    pointer to the memory buffer of the eventual
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_eventual_wait(ABT_eventual eventual, void **value)
{
    ABTI_local *p_local = ABTI_local_get_local();
    ABTI_eventual *p_eventual = ABTI_eventual_get_ptr(eventual);
    ABTI_CHECK_NULL_EVENTUAL_PTR(p_eventual);

    ABTI_spinlock_acquire(&p_eventual->lock);
    if (p_eventual->ready == ABT_FALSE) {
        ABTI_waitlist_wait_and_unlock(&p_local, &p_eventual->waitlist,
                                      &p_eventual->lock, ABT_FALSE,
                                      ABT_SYNC_EVENT_TYPE_EVENTUAL,
                                      (void *)p_eventual);
    } else {
        ABTI_spinlock_release(&p_eventual->lock);
    }
    /* This value is updated outside the critical section, but it is okay since
     * the "pointer" to the memory buffer is constant and there is no way to
     * avoid updating this memory buffer by ABT_eventual_set() etc. */
    if (value)
        *value = p_eventual->value;
    return ABT_SUCCESS;
}

/**
 * @ingroup EVENTUAL
 * @brief   Test the readiness of an eventual.
 *
 * \c ABT_eventual_test does a nonblocking test on the eventual \c eventual
 * if resolved. If the eventual is not ready, \c is_ready would equal FALSE.
 * If the eventual is ready, the pointer pointed to by \c value will point to
 * the memory buffer associated with the eventual.
 *
 * @param[in]  eventual handle to the eventual
 * @param[out] value    pointer to the memory buffer of the eventual
 * @param[out] is_ready pointer to the a user flag
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_eventual_test(ABT_eventual eventual, void **value, ABT_bool *is_ready)
{
    ABTI_eventual *p_eventual = ABTI_eventual_get_ptr(eventual);
    ABTI_CHECK_NULL_EVENTUAL_PTR(p_eventual);
    ABT_bool flag = ABT_FALSE;

    ABTI_spinlock_acquire(&p_eventual->lock);
    if (p_eventual->ready != ABT_FALSE) {
        if (value)
            *value = p_eventual->value;
        flag = ABT_TRUE;
    }
    ABTI_spinlock_release(&p_eventual->lock);

    *is_ready = flag;
    return ABT_SUCCESS;
}

/**
 * @ingroup EVENTUAL
 * @brief   Signal the eventual.
 *
 * \c ABT_eventual_set sets a value in the eventual's buffer and releases all
 * waiting ULTs. It copies \c nbytes bytes from the buffer pointed to by
 * \c value into the internal buffer of eventual and awakes all ULTs waiting
 * on the eventual. Therefore, all ULTs waiting on this eventual will be ready
 * to be scheduled.
 *
 * @param[in] eventual  handle to the eventual
 * @param[in] value     pointer to the memory buffer containing the data that
 *                      will be copied to the memory buffer of the eventual
 * @param[in] nbytes    number of bytes to be copied
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_eventual_set(ABT_eventual eventual, void *value, int nbytes)
{
    ABTI_local *p_local = ABTI_local_get_local();
    ABTI_eventual *p_eventual = ABTI_eventual_get_ptr(eventual);
    ABTI_CHECK_NULL_EVENTUAL_PTR(p_eventual);
    size_t arg_nbytes = nbytes;
    ABTI_CHECK_TRUE(arg_nbytes <= p_eventual->nbytes, ABT_ERR_INV_EVENTUAL);

    ABTI_spinlock_acquire(&p_eventual->lock);

    p_eventual->ready = ABT_TRUE;
    if (p_eventual->value)
        memcpy(p_eventual->value, value, arg_nbytes);
    /* Wake up all waiting ULTs */
    ABTI_waitlist_broadcast(p_local, &p_eventual->waitlist);

    ABTI_spinlock_release(&p_eventual->lock);
    return ABT_SUCCESS;
}

/**
 * @ingroup EVENTUAL
 * @brief   Reset the readiness of the target eventual.
 *
 * \c ABT_eventual_reset() resets the readiness of the target eventual
 * \c eventual so that it can be reused.  That is, it makes \c eventual
 * unready irrespective of its readiness.
 *
 * @param[in] eventual  handle to the target eventual
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_eventual_reset(ABT_eventual eventual)
{
    ABTI_eventual *p_eventual = ABTI_eventual_get_ptr(eventual);
    ABTI_CHECK_NULL_EVENTUAL_PTR(p_eventual);

    ABTI_spinlock_acquire(&p_eventual->lock);
    p_eventual->ready = ABT_FALSE;
    ABTI_spinlock_release(&p_eventual->lock);
    return ABT_SUCCESS;
}
