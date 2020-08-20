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
    int abt_errno = ABT_SUCCESS;
    ABTI_barrier *p_newbarrier;

    p_newbarrier = (ABTI_barrier *)ABTU_malloc(sizeof(ABTI_barrier));

    ABTI_spinlock_clear(&p_newbarrier->lock);
    p_newbarrier->num_waiters = num_waiters;
    p_newbarrier->counter = 0;
    p_newbarrier->waiters =
        (ABTI_ythread **)ABTU_malloc(num_waiters * sizeof(ABTI_ythread *));
    p_newbarrier->waiter_type =
        (ABT_unit_type *)ABTU_malloc(num_waiters * sizeof(ABT_unit_type));

    /* Return value */
    *newbarrier = ABTI_barrier_get_handle(p_newbarrier);

    return abt_errno;
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
    int abt_errno = ABT_SUCCESS;
    ABTI_barrier *p_barrier = ABTI_barrier_get_ptr(barrier);
    ABTI_CHECK_NULL_BARRIER_PTR(p_barrier);

    ABTI_ASSERT(p_barrier->counter == 0);

    /* Only when num_waiters is different from p_barrier->num_waiters, we
     * change p_barrier. */
    if (num_waiters < p_barrier->num_waiters) {
        /* We can reuse waiters and waiter_type arrays */
        p_barrier->num_waiters = num_waiters;
    } else if (num_waiters > p_barrier->num_waiters) {
        /* Free existing arrays and reallocate them */
        p_barrier->num_waiters = num_waiters;
        ABTU_free(p_barrier->waiters);
        ABTU_free(p_barrier->waiter_type);
        p_barrier->waiters =
            (ABTI_ythread **)ABTU_malloc(num_waiters * sizeof(ABTI_ythread *));
        p_barrier->waiter_type =
            (ABT_unit_type *)ABTU_malloc(num_waiters * sizeof(ABT_unit_type));
    }

fn_exit:
    return abt_errno;

fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_barrier_reinit", abt_errno);
    goto fn_exit;
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
    int abt_errno = ABT_SUCCESS;
    ABT_barrier h_barrier = *barrier;
    ABTI_barrier *p_barrier = ABTI_barrier_get_ptr(h_barrier);
    ABTI_CHECK_NULL_BARRIER_PTR(p_barrier);

    ABTI_ASSERT(p_barrier->counter == 0);

    /* The lock needs to be acquired to safely free the barrier structure.
     * However, we do not have to unlock it because the entire structure is
     * freed here. */
    ABTI_spinlock_acquire(&p_barrier->lock);

    ABTU_free(p_barrier->waiters);
    ABTU_free(p_barrier->waiter_type);
    ABTU_free(p_barrier);

    /* Return value */
    *barrier = ABT_BARRIER_NULL;

fn_exit:
    return abt_errno;

fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_barrier_free", abt_errno);
    goto fn_exit;
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
    int abt_errno = ABT_SUCCESS;
    ABTI_xstream *p_local_xstream = ABTI_local_get_xstream();
    ABTI_barrier *p_barrier = ABTI_barrier_get_ptr(barrier);
    ABTI_CHECK_NULL_BARRIER_PTR(p_barrier);
    uint32_t pos;

    ABTI_spinlock_acquire(&p_barrier->lock);

    ABTI_ASSERT(p_barrier->counter < p_barrier->num_waiters);
    pos = p_barrier->counter++;

    /* If we do not have all the waiters yet */
    if (p_barrier->counter < p_barrier->num_waiters) {
        ABTI_ythread *p_ythread;
        ABT_unit_type type;
        ABTD_atomic_int32 ext_signal = ABTD_ATOMIC_INT32_STATIC_INITIALIZER(0);

        if (p_local_xstream != NULL) {
            ABTI_CHECK_YIELDABLE(p_local_xstream->p_thread, &p_ythread,
                                 ABT_ERR_BARRIER);
            type = ABT_UNIT_TYPE_THREAD;
        } else {
            /* external thread */
            /* Check size if ext_signal can be stored in p_thread. */
            ABTI_STATIC_ASSERT(sizeof(ext_signal) <= sizeof(p_ythread));
            p_ythread = (ABTI_ythread *)&ext_signal;
            type = ABT_UNIT_TYPE_EXT;
        }

        /* Keep the waiter's information */
        p_barrier->waiters[pos] = p_ythread;
        p_barrier->waiter_type[pos] = type;

        if (type == ABT_UNIT_TYPE_THREAD) {
            /* Change the ULT's state to BLOCKED */
            ABTI_ythread_set_blocked(p_ythread);
        }

        ABTI_spinlock_release(&p_barrier->lock);

        if (type == ABT_UNIT_TYPE_THREAD) {
            /* Suspend the current ULT */
            ABTI_ythread_suspend(&p_local_xstream, p_ythread,
                                 ABT_SYNC_EVENT_TYPE_BARRIER,
                                 (void *)p_barrier);
        } else {
            /* External thread is waiting here polling ext_signal. */
            /* FIXME: need a better implementation */
            while (!ABTD_atomic_acquire_load_int32(&ext_signal))
                ;
        }
    } else {
        /* Signal all the waiting ULTs */
        int i;
        for (i = 0; i < p_barrier->num_waiters - 1; i++) {
            ABTI_ythread *p_ythread = p_barrier->waiters[i];
            if (p_barrier->waiter_type[i] == ABT_UNIT_TYPE_THREAD) {
                ABTI_ythread_set_ready(p_local_xstream, p_ythread);
            } else {
                /* When p_cur is an external thread */
                ABTD_atomic_int32 *p_ext_signal =
                    (ABTD_atomic_int32 *)p_ythread;
                ABTD_atomic_release_store_int32(p_ext_signal, 1);
            }

            p_barrier->waiters[i] = NULL;
        }

        /* Reset counter */
        p_barrier->counter = 0;

        ABTI_spinlock_release(&p_barrier->lock);
    }

fn_exit:
    return abt_errno;

fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
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
    int abt_errno = ABT_SUCCESS;
    ABTI_barrier *p_barrier = ABTI_barrier_get_ptr(barrier);
    ABTI_CHECK_NULL_BARRIER_PTR(p_barrier);

    *num_waiters = p_barrier->num_waiters;

fn_exit:
    return abt_errno;

fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_barrier_get_num_waiters", abt_errno);
    goto fn_exit;
}
