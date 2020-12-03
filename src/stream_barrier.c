/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"

/** @defgroup ES_BARRIER ES barrier
 * This group is for ES barrier.
 */

/**
 * @ingroup ES_BARRIER
 * @brief   Create a new ES barrier.
 *
 * \c ABT_xstream_barrier_create() creates a new ES barrier and returns its
 * handle through \c newbarrier.
 * If an error occurs in this routine, a non-zero error code will be returned
 * and \c newbarrier will be set to \c ABT_XSTREAM_BARRIER_NULL.
 *
 * @param[in]  num_waiters  number of waiters
 * @param[out] newbarrier   handle to a new ES barrier
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_xstream_barrier_create(uint32_t num_waiters,
                               ABT_xstream_barrier *newbarrier)
{
    int abt_errno;
    ABTI_xstream_barrier *p_newbarrier;

    abt_errno =
        ABTU_malloc(sizeof(ABTI_xstream_barrier), (void **)&p_newbarrier);
    ABTI_CHECK_ERROR(abt_errno);

    p_newbarrier->num_waiters = num_waiters;
#ifdef HAVE_PTHREAD_BARRIER_INIT
    abt_errno = ABTD_xstream_barrier_init(num_waiters, &p_newbarrier->bar);
    if (ABTI_IS_ERROR_CHECK_ENABLED && abt_errno != ABT_SUCCESS) {
        ABTU_free(p_newbarrier);
        ABTI_HANDLE_ERROR(abt_errno);
    }
#else
    ABTI_spinlock_clear(&p_newbarrier->lock);
    p_newbarrier->counter = 0;
    ABTD_atomic_relaxed_store_uint64(&p_newbarrier->tag, 0);
#endif

    /* Return value */
    *newbarrier = ABTI_xstream_barrier_get_handle(p_newbarrier);
    return ABT_SUCCESS;
}

/**
 * @ingroup ES_BARRIER
 * @brief   Free the ES barrier.
 *
 * \c ABT_xstream_barrier_free() deallocates the memory used for the ES barrier
 * object associated with the handle \c barrier.  If it is successfully
 * processed, \c barrier is set to \c ABT_XSTREAM_BARRIER_NULL.
 *
 * @param[in,out] barrier  handle to the ES barrier
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_xstream_barrier_free(ABT_xstream_barrier *barrier)
{
    ABT_xstream_barrier h_barrier = *barrier;
    ABTI_xstream_barrier *p_barrier = ABTI_xstream_barrier_get_ptr(h_barrier);
    ABTI_CHECK_NULL_XSTREAM_BARRIER_PTR(p_barrier);

#ifdef HAVE_PTHREAD_BARRIER_INIT
    ABTD_xstream_barrier_destroy(&p_barrier->bar);
#endif
    ABTU_free(p_barrier);

    /* Return value */
    *barrier = ABT_XSTREAM_BARRIER_NULL;
    return ABT_SUCCESS;
}

/**
 * @ingroup ES_BARRIER
 * @brief   Wait on the barrier.
 *
 * The work unit calling \c ABT_xstream_barrier_wait() waits on the barrier and
 * blocks the entire ES until all the participants reach the barrier.
 *
 * @param[in] barrier  handle to the ES barrier
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_xstream_barrier_wait(ABT_xstream_barrier barrier)
{
    ABTI_xstream_barrier *p_barrier = ABTI_xstream_barrier_get_ptr(barrier);
    ABTI_CHECK_NULL_XSTREAM_BARRIER_PTR(p_barrier);

    if (p_barrier->num_waiters > 1) {
#ifdef HAVE_PTHREAD_BARRIER_INIT
        ABTD_xstream_barrier_wait(&p_barrier->bar);
#else
        /* The following implementation is a simple sense-reversal barrier
         * implementation while it uses uint64_t instead of boolean to prevent
         * a sense variable from wrapping around. */
        ABTI_spinlock_acquire(&p_barrier->lock);
        p_barrier->counter++;
        if (p_barrier->counter == p_barrier->num_waiters) {
            /* Wake up the other waiters. */
            p_barrier->counter = 0;
            /* Updating tag wakes up other waiters.  Note that this tag is
             * sufficiently large, so it will not wrap around. */
            uint64_t cur_tag = ABTD_atomic_relaxed_load_uint64(&p_barrier->tag);
            uint64_t new_tag = (cur_tag + 1) & (UINT64_MAX >> 1);
            ABTD_atomic_release_store_uint64(&p_barrier->tag, new_tag);
            ABTI_spinlock_release(&p_barrier->lock);
        } else {
            /* Wait until the tag is updated by the last waiter */
            uint64_t cur_tag = ABTD_atomic_relaxed_load_uint64(&p_barrier->tag);
            ABTI_spinlock_release(&p_barrier->lock);
            while (cur_tag == ABTD_atomic_acquire_load_uint64(&p_barrier->tag))
                ABTD_atomic_pause();
        }
#endif
    }
    return ABT_SUCCESS;
}
