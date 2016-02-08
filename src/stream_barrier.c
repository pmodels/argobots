/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"


/** @defgroup ES_BARRIER ES barrier
 * This group is for ES barrier.
 */

typedef struct {
    uint32_t num_waiters;
    ABTD_xstream_barrier bar;
} ABTI_xstream_barrier;


#ifdef HAVE_PTHREAD_BARRIER_INIT
static inline
ABTI_xstream_barrier *ABTI_xstream_barrier_get_ptr(ABT_xstream_barrier barrier)
{
#ifndef ABT_CONFIG_DISABLE_ERROR_CHECK
    ABTI_xstream_barrier *p_barrier;
    if (barrier == ABT_XSTREAM_BARRIER_NULL) {
        p_barrier = NULL;
    } else {
        p_barrier = (ABTI_xstream_barrier *)barrier;
    }
    return p_barrier;
#else
    return (ABTI_xstream_barrier *)barrier;
#endif
}

static inline
ABT_xstream_barrier ABTI_xstream_barrier_get_handle(ABTI_xstream_barrier *p_barrier)
{
#ifndef ABT_CONFIG_DISABLE_ERROR_CHECK
    ABT_xstream_barrier h_barrier;
    if (p_barrier == NULL) {
        h_barrier = ABT_XSTREAM_BARRIER_NULL;
    } else {
        h_barrier = (ABT_xstream_barrier)p_barrier;
    }
    return h_barrier;
#else
    return (ABT_xstream_barrier)p_barrier;
#endif
}
#endif


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
int ABT_xstream_barrier_create(uint32_t num_waiters, ABT_xstream_barrier *newbarrier)
{
#ifdef HAVE_PTHREAD_BARRIER_INIT
    int abt_errno = ABT_SUCCESS;
    ABTI_xstream_barrier *p_newbarrier;

    p_newbarrier = (ABTI_xstream_barrier *)ABTU_malloc(sizeof(ABTI_xstream_barrier));

    p_newbarrier->num_waiters = num_waiters;
    abt_errno = ABTD_xstream_barrier_init(num_waiters, &p_newbarrier->bar);
    ABTI_CHECK_ERROR(abt_errno);

    /* Return value */
    *newbarrier = ABTI_xstream_barrier_get_handle(p_newbarrier);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
#else
    return ABT_ERR_FEATURE_NA;
#endif
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
int ABT_xstream_barrier_free(ABT_barrier *barrier)
{
#ifdef HAVE_PTHREAD_BARRIER_INIT
    int abt_errno = ABT_SUCCESS;
    ABT_xstream_barrier h_barrier = *barrier;
    ABTI_xstream_barrier *p_barrier = ABTI_xstream_barrier_get_ptr(h_barrier);
    ABTI_CHECK_NULL_BARRIER_PTR(p_barrier);

    abt_errno = ABTD_xstream_barrier_destroy(&p_barrier->bar);
    ABTI_CHECK_ERROR(abt_errno);

    ABTU_free(p_barrier);

    /* Return value */
    *barrier = ABT_XSTREAM_BARRIER_NULL;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
#else
    return ABT_ERR_FEATURE_NA;
#endif
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
#ifdef HAVE_PTHREAD_BARRIER_INIT
    int abt_errno = ABT_SUCCESS;
    ABTI_xstream_barrier *p_barrier = ABTI_xstream_barrier_get_ptr(barrier);
    ABTI_CHECK_NULL_BARRIER_PTR(p_barrier);

    if (p_barrier->num_waiters > 1) {
        ABTD_xstream_barrier_wait(&p_barrier->bar);
    }

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
#else
    return ABT_ERR_FEATURE_NA;
#endif
}

