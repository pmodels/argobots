/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"

/** @defgroup BARRIER Barrier
 * This group is for Barrier.
 */

/**
 * @ingroup BARRIER
 * @brief   Create a new barrier.
 *
 * \c ABT_barrier_create() creates a new barrier and returns its handle through
 * \c newbarrier.
 * If an error occurs in this routine, a non-zero error code will be returned
 * and \c newbarrier will be set to \c ABT_BARRIER_NULL.
 *
 * @param[in]  num_waiters  number of waiters
 * @param[out] newbarrier   handle to a new barrier
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_barrier_create(uint32_t num_waiters, ABT_barrier *newbarrier)
{
    int abt_errno;
    ABTI_barrier *p_newbarrier;
    size_t arg_num_waiters = num_waiters;

    abt_errno = ABTU_malloc(sizeof(ABTI_barrier), (void **)&p_newbarrier);
    ABTI_CHECK_ERROR(abt_errno);

    ABTI_spinlock_clear(&p_newbarrier->lock);
    p_newbarrier->num_waiters = arg_num_waiters;
    p_newbarrier->counter = 0;
    ABTI_waitlist_init(&p_newbarrier->waitlist);
    /* Return value */
    *newbarrier = ABTI_barrier_get_handle(p_newbarrier);
    return ABT_SUCCESS;
}

/**
 * @ingroup BARRIER
 * @brief   Reinitialize the barrier.
 *
 * \c ABT_barrier_reinit() reinitializes the barrier \c barrier with a new
 * number of waiters \c num_waiters.  \c num_waiters can be the same as or
 * different from the one passed to \c ABT_barrier_create().
 *
 * @param[in] barrier      handle to the barrier
 * @param[in] num_waiters  number of waiters
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_barrier_reinit(ABT_barrier barrier, uint32_t num_waiters)
{
    ABTI_barrier *p_barrier = ABTI_barrier_get_ptr(barrier);
    ABTI_CHECK_NULL_BARRIER_PTR(p_barrier);
    ABTI_ASSERT(p_barrier->counter == 0);
    size_t arg_num_waiters = num_waiters;

    /* Only when num_waiters is different from p_barrier->num_waiters, we
     * change p_barrier. */
    if (arg_num_waiters != p_barrier->num_waiters) {
        /* We can reuse waiters and waiter_type arrays */
        p_barrier->num_waiters = arg_num_waiters;
    }
    return ABT_SUCCESS;
}

/**
 * @ingroup BARRIER
 * @brief   Free the barrier.
 *
 * \c ABT_barrier_free() deallocates the memory used for the barrier object
 * associated with the handle \c barrier. If it is successfully processed,
 * \c barrier is set to \c ABT_BARRIER_NULL.
 *
 * @param[in,out] barrier  handle to the barrier
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_barrier_free(ABT_barrier *barrier)
{
    ABT_barrier h_barrier = *barrier;
    ABTI_barrier *p_barrier = ABTI_barrier_get_ptr(h_barrier);
    ABTI_CHECK_NULL_BARRIER_PTR(p_barrier);

    /* The lock needs to be acquired to safely free the barrier structure.
     * However, we do not have to unlock it because the entire structure is
     * freed here. */
    ABTI_spinlock_acquire(&p_barrier->lock);

    /* p_barrier->counter must be checked after taking a lock. */
    ABTI_ASSERT(p_barrier->counter == 0);

    ABTU_free(p_barrier);

    /* Return value */
    *barrier = ABT_BARRIER_NULL;
    return ABT_SUCCESS;
}

/**
 * @ingroup BARRIER
 * @brief   Wait on the barrier.
 *
 * The ULT calling \c ABT_barrier_wait() waits on the barrier until all the
 * ULTs reach the barrier.
 *
 * @param[in] barrier  handle to the barrier
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_barrier_wait(ABT_barrier barrier)
{
    ABTI_local *p_local = ABTI_local_get_local();
    ABTI_barrier *p_barrier = ABTI_barrier_get_ptr(barrier);
    ABTI_CHECK_NULL_BARRIER_PTR(p_barrier);

    ABTI_spinlock_acquire(&p_barrier->lock);

    ABTI_ASSERT(p_barrier->counter < p_barrier->num_waiters);
    p_barrier->counter++;

    /* If we do not have all the waiters yet */
    if (p_barrier->counter < p_barrier->num_waiters) {
        ABTI_waitlist_wait_and_unlock(&p_local, &p_barrier->waitlist,
                                      &p_barrier->lock, ABT_FALSE,
                                      ABT_SYNC_EVENT_TYPE_BARRIER,
                                      (void *)p_barrier);
    } else {
        ABTI_waitlist_broadcast(p_local, &p_barrier->waitlist);
        /* Reset counter */
        p_barrier->counter = 0;
        ABTI_spinlock_release(&p_barrier->lock);
    }
    return ABT_SUCCESS;
}

/**
 * @ingroup BARRIER
 * @brief   Get the number of waiters for the barrier.
 *
 * \c ABT_barrier_get_num_waiters() returns the number of waiters, which was
 * passed to \c ABT_barrier_create() or \c ABT_barrier_reinit(), for the given
 * barrier \c barrier.
 *
 * @param[in]  barrier      handle to the barrier
 * @param[out] num_waiters  number of waiters
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_barrier_get_num_waiters(ABT_barrier barrier, uint32_t *num_waiters)
{
    ABTI_barrier *p_barrier = ABTI_barrier_get_ptr(barrier);
    ABTI_CHECK_NULL_BARRIER_PTR(p_barrier);

    *num_waiters = p_barrier->num_waiters;
    return ABT_SUCCESS;
}
