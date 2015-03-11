/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"

static uint64_t ABTI_xstream_get_new_rank(void);


/** @defgroup ES Execution Stream (ES)
 * This group is for Execution Stream.
 */

/**
 * @ingroup ES
 * @brief   Create a new ES and return its handle through newxstream.
 *
 * @param[in]  sched  handle to the scheduler used for a new ES. If this is
 *                    ABT_SCHED_NULL, the runtime-provided scheduler is used.
 * @param[out] newxstream  handle to a newly created ES. This cannot be NULL
 *                    because unnamed ES is not allowed.
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_xstream_create(ABT_sched sched, ABT_xstream *newxstream)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_xstream *p_newxstream;
    ABT_xstream h_newxstream;

    if (sched == ABT_SCHED_NULL) {
        abt_errno = ABT_xstream_create_basic(ABT_SCHED_DEFAULT_POOL_FIFO_PR_SW,
                                             0, NULL, newxstream);
        ABTI_CHECK_ERROR(abt_errno);
        return abt_errno;
    }

    p_newxstream = (ABTI_xstream *)ABTU_malloc(sizeof(ABTI_xstream));
    if (!p_newxstream) {
        HANDLE_ERROR("ABTU_malloc");
        *newxstream = ABT_XSTREAM_NULL;
        abt_errno = ABT_ERR_MEM;
        goto fn_fail;
    }

    /* Create a wrapper unit */
    p_newxstream->elem = ABTI_elem_create_from_xstream(p_newxstream);

    p_newxstream->rank         = ABTI_xstream_get_new_rank();
    p_newxstream->p_name       = NULL;
    p_newxstream->type         = ABTI_XSTREAM_TYPE_SECONDARY;
    p_newxstream->state        = ABT_XSTREAM_STATE_CREATED;
    p_newxstream->scheds       = NULL;
    p_newxstream->num_scheds   = 0;
    p_newxstream->max_scheds   = 0;
    p_newxstream->request      = 0;
    p_newxstream->p_main_sched = NULL;

    /* Create mutex */
    abt_errno = ABT_mutex_create(&p_newxstream->mutex);
    ABTI_CHECK_ERROR(abt_errno);
    abt_errno = ABT_mutex_create(&p_newxstream->top_sched_mutex);
    ABTI_CHECK_ERROR(abt_errno);

    /* Set the main scheduler */
    ABT_xstream xstream = ABTI_xstream_get_handle(p_newxstream);
    abt_errno = ABT_xstream_set_main_sched(xstream, sched);
    ABTI_CHECK_ERROR(abt_errno);

    /* Create a work unit pool that contains terminated work units */
    abt_errno = ABTI_contn_create(&p_newxstream->deads);
    ABTI_CHECK_ERROR(abt_errno);

    /* Add this xstream to the global ES pool */
    ABTI_global_add_xstream(p_newxstream);

    /* Return value */
    h_newxstream = ABTI_xstream_get_handle(p_newxstream);
    *newxstream = h_newxstream;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_xstream_create", abt_errno);
    goto fn_exit;
}

/**
 * @ingroup ES
 * @brief   Create a new ES with a predefined scheduler and return its handle
 *          through \c newxstream.
 *
 * If \c predef is a scheduler that includes automatic creation of pools,
 * \c pools will be equal to NULL.
 *
 * @param[in]  predef       predefined scheduler
 * @param[in]  num_pools    number of pools associated with this scheduler
 * @param[in]  pools        pools associated with this scheduler
 * @param[out] newxstream   handle to the target ES
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_xstream_create_basic(ABT_sched_predef predef, int num_pools,
        ABT_pool *pools, ABT_xstream *newxstream)
{
    int abt_errno = ABT_SUCCESS;

    ABT_sched sched;
    abt_errno = ABT_sched_create_basic(predef, num_pools, pools, 1, &sched);
    ABTI_CHECK_ERROR(abt_errno);

    abt_errno = ABT_xstream_create(sched, newxstream);
    ABTI_CHECK_ERROR(abt_errno);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_xstream_create_basic", abt_errno);
    goto fn_exit;
}

/**
 * @ingroup ES
 * @brief   Start the target ES.
 *
 * The primary ES does not need to be started explicitly. Other ESs will be
 * started automatically if a ULT or a tasklet is pushed to a pool that belongs
 * exclusively to them. Except these cases, the user needs to start ES manually
 * by calling this routine.
 *
 * @param[in] xstream  handle to the target ES
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_xstream_start(ABT_xstream xstream)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_xstream *p_xstream = ABTI_xstream_get_ptr(xstream);

    /* Set the xstream's state as READY */
    ABT_xstream_state old_state;
    old_state = ABTD_atomic_cas_int32((int32_t *)&p_xstream->state,
            ABT_XSTREAM_STATE_CREATED, ABT_XSTREAM_STATE_READY);
    if (old_state != ABT_XSTREAM_STATE_CREATED) goto fn_exit;

    /* Add the main scheduler to the stack of schedulers */
    ABTI_xstream_push_sched(p_xstream, p_xstream->p_main_sched);

    if (p_xstream->type == ABTI_XSTREAM_TYPE_PRIMARY) {
        abt_errno = ABTD_xstream_context_self(&p_xstream->ctx);
        ABTI_CHECK_ERROR_MSG(abt_errno, "ABTD_xstream_context_self");
        /* Create the context of the main scheduler */
        ABTI_sched *p_sched = p_xstream->p_main_sched;
        abt_errno = ABTI_thread_create_main_sched(p_sched, &p_sched->thread);
        ABTI_CHECK_ERROR(abt_errno);

    } else {
        /* Start the main scheduler on a different kernel thread */
        abt_errno = ABTD_xstream_context_create(
                ABTI_xstream_launch_main_sched, (void *)p_xstream,
                &p_xstream->ctx);
        ABTI_CHECK_ERROR_MSG(abt_errno, "ABTD_xstream_context_create");
    }

    /* Move the xstream to the global active ES pool */
    ABTI_global_move_xstream(p_xstream);

  fn_exit:
    return abt_errno;

  fn_fail:
    goto fn_exit;
}

/**
 * @ingroup ES
 * @brief   Release the ES object associated with ES handle.
 *
 * This routine deallocates memory used for the ES object. If the xstream
 * is still running when this routine is called, the deallocation happens
 * after the xstream terminates and then this routine returns. If it is
 * successfully processed, xstream is set as ABT_XSTREAM_NULL. The primary
 * ES cannot be freed with this routine.
 *
 * @param[in,out] xstream  handle to the target ES
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_xstream_free(ABT_xstream *xstream)
{
    int abt_errno = ABT_SUCCESS;
    ABT_xstream h_xstream = *xstream;

    ABTI_xstream *p_xstream = ABTI_xstream_get_ptr(h_xstream);
    if (p_xstream == NULL) goto fn_exit;

    if (p_xstream == ABTI_local_get_xstream()) {
        HANDLE_ERROR("The current xstream cannot be freed.");
        abt_errno = ABT_ERR_INV_XSTREAM;
        goto fn_fail;
    }

    if (p_xstream->type == ABTI_XSTREAM_TYPE_PRIMARY) {
        HANDLE_ERROR("The primary xstream cannot be freed explicitly.");
        abt_errno = ABT_ERR_INV_XSTREAM;
        goto fn_fail;
    }

    /* If the xstream is running, wait until it terminates */
    if (p_xstream->state == ABT_XSTREAM_STATE_RUNNING) {
        abt_errno = ABT_xstream_join(h_xstream);
        ABTI_CHECK_ERROR(abt_errno);
    }

    /* Remove this xstream from the global ES pool */
    abt_errno = ABTI_global_del_xstream(p_xstream);
    ABTI_CHECK_ERROR(abt_errno);

    /* Free the xstream object */
    abt_errno = ABTI_xstream_free(p_xstream);
    ABTI_CHECK_ERROR(abt_errno);

    /* Return value */
    *xstream = ABT_XSTREAM_NULL;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_xstream_free", abt_errno);
    goto fn_exit;
}

/**
 * @ingroup ES
 * @brief   Wait for xstream to terminate.
 *
 * The target xstream cannot be the same as the xstream associated with calling
 * thread. If they are identical, this routine returns immediately without
 * waiting for the xstream's termination.
 *
 * @param[in] xstream  handle to the target ES
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_xstream_join(ABT_xstream xstream)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_xstream *p_xstream = ABTI_xstream_get_ptr(xstream);

    /* The target ES must not be the same as the calling thread's ES */
    if (p_xstream == ABTI_local_get_xstream()) {
        HANDLE_ERROR("The target ES should be different.");
        abt_errno = ABT_ERR_INV_XSTREAM;
        goto fn_fail;
    }

    if (p_xstream->type == ABTI_XSTREAM_TYPE_PRIMARY) {
        HANDLE_ERROR("The primary ES cannot be joined.");
        abt_errno = ABT_ERR_INV_XSTREAM;
        goto fn_fail;
    }

    if (p_xstream->state == ABT_XSTREAM_STATE_CREATED) {
        ABT_mutex_waitlock(p_xstream->mutex);
        /* If xstream's state was changed, we cannot terminate it here */
        if (ABTD_atomic_cas_int32((int32_t *)&p_xstream->state,
                                  ABT_XSTREAM_STATE_CREATED,
                                  ABT_XSTREAM_STATE_TERMINATED)
            != ABT_XSTREAM_STATE_CREATED) {
            ABT_mutex_unlock(p_xstream->mutex);
            goto fn_body;
        }
        abt_errno = ABTI_global_move_xstream(p_xstream);
        ABT_mutex_unlock(p_xstream->mutex);
        ABTI_CHECK_ERROR_MSG(abt_errno, "ABTI_global_move_xstream");
        goto fn_exit;
    }

  fn_body:
    /* Set the join request */
    ABTD_atomic_fetch_or_uint32(&p_xstream->request, ABTI_XSTREAM_REQ_JOIN);

    /* Wait until the target ES terminates */
    while (p_xstream->state != ABT_XSTREAM_STATE_TERMINATED) {
        ABT_thread_yield();
    }

    /* Normal join request */
    abt_errno = ABTD_xstream_context_join(p_xstream->ctx);
    ABTI_CHECK_ERROR_MSG(abt_errno, "ABTD_xstream_context_join");

  fn_exit:
    return abt_errno;

  fn_fail:
    goto fn_exit;
}

/**
 * @ingroup ES
 * @brief   Terminate the ES associated with the calling ULT.
 *
 * Since the calling ULT's ES terminates, this routine never returns.
 * Tasklets are not allowed to call this routine.
 *
 * @return Error code
 * @retval ABT_SUCCESS           on success
 * @retval ABT_ERR_UNINITIALIZED Argobots has not been initialized
 * @retval ABT_ERR_INV_XSTREAM   called by an external thread, e.g., pthread
 */
int ABT_xstream_exit(void)
{
    int abt_errno = ABT_SUCCESS;

    /* In case that Argobots has not been initialized or this routine is called
     * by an external thread, e.g., pthread, return an error code instead of
     * making the call fail. */
    if (gp_ABTI_global == NULL) {
        abt_errno = ABT_ERR_UNINITIALIZED;
        goto fn_exit;
    }
    if (lp_ABTI_local == NULL) {
        abt_errno = ABT_ERR_INV_XSTREAM;
        goto fn_exit;
    }

    ABTI_xstream *p_xstream = ABTI_local_get_xstream();
    ABTI_CHECK_NULL_XSTREAM_PTR(p_xstream);

    /* Set the exit request */
    ABTD_atomic_fetch_or_uint32(&p_xstream->request, ABTI_XSTREAM_REQ_EXIT);

    /* Wait until the ES terminates */
    do {
        ABT_thread_yield();
    } while (p_xstream->state != ABT_XSTREAM_STATE_TERMINATED);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_xstream_exit", abt_errno);
    goto fn_exit;
}

/**
 * @ingroup ES
 * @brief   Request the cancelation of the target ES.
 *
 * @param[in] xstream  handle to the target ES
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_xstream_cancel(ABT_xstream xstream)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_xstream *p_xstream = ABTI_xstream_get_ptr(xstream);

    if (p_xstream->type == ABTI_XSTREAM_TYPE_PRIMARY) {
        HANDLE_ERROR("The primary xstream cannot be canceled.");
        abt_errno = ABT_ERR_INV_XSTREAM;
        goto fn_fail;
    }

    /* Set the cancel request */
    ABTD_atomic_fetch_or_uint32(&p_xstream->request, ABTI_XSTREAM_REQ_CANCEL);

  fn_exit:
    return abt_errno;

  fn_fail:
    goto fn_exit;
}

/**
 * @ingroup ES
 * @brief   Return the ES handle associated with the caller work unit.
 *
 * \c ABT_xstream_self() returns the handle to ES object associated with
 * the caller work unit through \c xstream.
 * When an error occurs, \c xstream is set to \c ABT_XSTREAM_NULL.
 *
 * @param[out] xstream  ES handle
 * @return Error code
 * @retval ABT_SUCCESS           on success
 * @retval ABT_ERR_UNINITIALIZED Argobots has not been initialized
 * @retval ABT_ERR_INV_XSTREAM   called by an external thread, e.g., pthread
 */
int ABT_xstream_self(ABT_xstream *xstream)
{
    int abt_errno = ABT_SUCCESS;

    /* In case that Argobots has not been initialized or this routine is called
     * by an external thread, e.g., pthread, return an error code instead of
     * making the call fail. */
    if (gp_ABTI_global == NULL) {
        abt_errno = ABT_ERR_UNINITIALIZED;
        *xstream = ABT_XSTREAM_NULL;
        goto fn_exit;
    }
    if (lp_ABTI_local == NULL) {
        abt_errno = ABT_ERR_INV_XSTREAM;
        *xstream = ABT_XSTREAM_NULL;
        goto fn_exit;
    }

    ABTI_xstream *p_xstream = ABTI_local_get_xstream();
    ABTI_CHECK_NULL_XSTREAM_PTR(p_xstream);

    /* Return value */
    *xstream = ABTI_xstream_get_handle(p_xstream);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_xstream_self", abt_errno);
    goto fn_exit;
}

/**
 * @ingroup ES
 * @brief   Return the rank of ES associated with the caller work unit.
 *
 * @param[out] rank  ES rank
 * @return Error code
 * @retval ABT_SUCCESS           on success
 * @retval ABT_ERR_UNINITIALIZED Argobots has not been initialized
 * @retval ABT_ERR_INV_XSTREAM   called by an external thread, e.g., pthread
 */
int ABT_xstream_self_rank(int *rank)
{
    int abt_errno = ABT_SUCCESS;

    /* In case that Argobots has not been initialized or this routine is called
     * by an external thread, e.g., pthread, return an error code instead of
     * making the call fail. */
    if (gp_ABTI_global == NULL) {
        abt_errno = ABT_ERR_UNINITIALIZED;
        goto fn_exit;
    }
    if (lp_ABTI_local == NULL) {
        abt_errno = ABT_ERR_INV_XSTREAM;
        goto fn_exit;
    }

    ABTI_xstream *p_xstream = ABTI_local_get_xstream();
    ABTI_CHECK_NULL_XSTREAM_PTR(p_xstream);

    /* Return value */
    *rank = (int)p_xstream->rank;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_xstream_self_rank", abt_errno);
    goto fn_exit;
}

/**
 * @ingroup ES
 * @brief   Set the rank for target ES
 *
 * @param[in] xstream  handle to the target ES
 * @param[in] rank     ES rank
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_xstream_set_rank(ABT_xstream xstream, const int rank)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_xstream *p_xstream = ABTI_xstream_get_ptr(xstream);
    ABTI_CHECK_NULL_XSTREAM_PTR(p_xstream);

    p_xstream->rank = (uint64_t)rank;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_xstream_set_rank", abt_errno);
    goto fn_exit;
}

/**
 * @ingroup ES
 * @brief   Return the rank of ES
 *
 * @param[in]  xstream  handle to the target ES
 * @param[out] rank     ES rank
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_xstream_get_rank(ABT_xstream xstream, int *rank)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_xstream *p_xstream = ABTI_xstream_get_ptr(xstream);
    ABTI_CHECK_NULL_XSTREAM_PTR(p_xstream);
    *rank = (int)p_xstream->rank;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_xstream_get_rank", abt_errno);
    goto fn_exit;
}

/**
 * @ingroup ES
 * @brief   Set \c sched as the main scheduler for \c xstream.
 *
 * The scheduler \c sched will be the main scheduler that will first run
 * when the ES is started. Once the ES is started, it is currently not allowed
 * to change the main scheduler for the ES.
 *
 * @param[in] xstream  handle to the target ES
 * @param[in] sched    handle to the scheduler
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_xstream_set_main_sched(ABT_xstream xstream, ABT_sched sched)
{
    int abt_errno = ABT_SUCCESS;
    int p;

    ABTI_xstream *p_xstream = ABTI_xstream_get_ptr(xstream);
    ABTI_CHECK_NULL_XSTREAM_PTR(p_xstream);

    ABTI_sched *p_sched = ABTI_sched_get_ptr(sched);
    ABTI_CHECK_NULL_SCHED_PTR(p_sched);

    if (sched == ABT_SCHED_NULL) {
        abt_errno = ABT_xstream_set_main_sched_basic(xstream,
                ABT_SCHED_DEFAULT_POOL_FIFO_PR_SW, 0, NULL);
        ABTI_CHECK_ERROR(abt_errno);
        return abt_errno;
    }

    /* TODO: permit to change the scheduler even when running */
    /* We check that the ES is just created */
    if (p_xstream->state != ABT_XSTREAM_STATE_CREATED &&
            p_xstream->state != ABT_XSTREAM_STATE_READY) {
        abt_errno = ABT_ERR_XSTREAM_STATE;
        goto fn_fail;
    }

    /* We check that from the pool set of the scheduler we do not find a pool
     * with another associated pool, and set the right value if it is okay  */
    for (p = 0; p < p_sched->num_pools; p++) {
      abt_errno =  ABTI_pool_set_reader(p_sched->pools[p], p_xstream);
      ABTI_CHECK_ERROR(abt_errno);
    }

    /* We free the old scheduler */
    if (p_xstream->p_main_sched != NULL) {
        /* The primary ES is in this state if this call is explicit */
        if (p_xstream->state == ABT_XSTREAM_STATE_READY) {
            abt_errno = ABTI_xstream_pop_sched(p_xstream);
            ABTI_CHECK_ERROR(abt_errno);
        }
        // TODO free
    }

    /* The main scheduler will to be a ULT, not a tasklet */
    p_sched->type = ABT_SCHED_TYPE_ULT;

    /* Set the scheduler */
    p_xstream->p_main_sched = p_sched;

    /* Set the associated ES for each pool of the scheduler */
    for (p = 0; p < p_sched->num_pools; p++) {
      abt_errno =  ABTI_pool_set_reader(p_sched->pools[p], p_xstream);
      ABTI_CHECK_ERROR(abt_errno);
    }

    /* Set the scheduler as a main scheduler */
    abt_errno = ABTI_sched_associate(p_sched, ABTI_SCHED_MAIN);
    ABTI_CHECK_ERROR(abt_errno);

    /* If it is the primary ES, we need to start it again */
    if (p_xstream->type == ABTI_XSTREAM_TYPE_PRIMARY) {
        p_xstream->state = ABT_XSTREAM_STATE_CREATED;
        ABT_xstream_start(xstream);
        ABTI_CHECK_ERROR_MSG(abt_errno, "ABT_xstream_start");
    }

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_xstream_set_main_sched", abt_errno);
    goto fn_exit;
}

/**
 * @ingroup ES
 * @brief   Set the main scheduler for \c xstream with a predefined scheduler.
 *
 * See \c ABT_xstream_set_main_sched() for more details.
 *
 * @param[in] xstream     handle to the target ES
 * @param[in] kind        kind of scheduler among predefined schedulers
 * @param[in] num_pools   number of pools associated with this scheduler
 * @param[in] pools       pools associated with this scheduler
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_xstream_set_main_sched_basic(ABT_xstream xstream,
        ABT_sched_predef predef, int num_pools, ABT_pool *pools)
{
    int abt_errno = ABT_SUCCESS;

    ABT_sched sched;
    abt_errno = ABT_sched_create_basic(predef, num_pools, pools, 1, &sched);
    ABTI_CHECK_ERROR(abt_errno);

    abt_errno = ABT_xstream_set_main_sched(xstream, sched);
    ABTI_CHECK_ERROR(abt_errno);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_xstream_set_main_sched_basic", abt_errno);
    goto fn_exit;
}

/**
 * @ingroup ES
 * @brief   Get the main scheduler of the target ES.
 *
 * \c ABT_xstream_get_main_sched() gets the handle of the main scheduler
 * for the target ES \c xstream through \c sched.
 *
 * @param[in] xstream  handle to the target ES
 * @param[out] sched   handle to the scheduler
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_xstream_get_main_sched(ABT_xstream xstream, ABT_sched *sched)
{
    int abt_errno = ABT_SUCCESS;

    ABTI_xstream *p_xstream = ABTI_xstream_get_ptr(xstream);
    ABTI_CHECK_NULL_XSTREAM_PTR(p_xstream);

    *sched = ABTI_sched_get_handle(p_xstream->p_main_sched);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_xstream_get_main_sched", abt_errno);
    goto fn_exit;
}

/**
 * @ingroup ES
 * @brief   Get the pools of the main scheduler of the target ES.
 *
 * This function is a convenient function that calls
 * \c ABT_xstream_get_main_sched() to get the main scheduler, and then
 * \c ABT_sched_get_pools() to get retrieve the associated pools.
 *
 * @param[in]  xstream   handle to the target ES
 * @param[in]  max_pools maximum number of pools
 * @param[out] pools     array of handles to the pools
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_xstream_get_main_pools(ABT_xstream xstream, int max_pools,
                               ABT_pool *pools)
{
    int abt_errno = ABT_SUCCESS;
    ABT_sched sched;

    abt_errno = ABT_xstream_get_main_sched(xstream, &sched);
    ABTI_CHECK_ERROR(abt_errno);

    abt_errno = ABT_sched_get_pools(sched, max_pools, 0, pools);
    ABTI_CHECK_ERROR(abt_errno);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_xstream_set_sched", abt_errno);
    goto fn_exit;
}

/**
 * @ingroup ES
 * @brief   Return the state of xstream.
 *
 * @param[in]  xstream  handle to the target ES
 * @param[out] state    the xstream's state
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_xstream_get_state(ABT_xstream xstream, ABT_xstream_state *state)
{
    int abt_errno = ABT_SUCCESS;

    ABTI_xstream *p_xstream = ABTI_xstream_get_ptr(xstream);
    ABTI_CHECK_NULL_XSTREAM_PTR(p_xstream);

    /* Return value */
    *state = p_xstream->state;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_xstream_get_state", abt_errno);
    goto fn_exit;
}

/**
 * @ingroup ES
 * @brief   Compare two ES handles for equality.
 *
 * \c ABT_xstream_equal() compares two ES handles for equality. If two handles
 * are associated with the same ES, \c result will be set to \c ABT_TRUE.
 * Otherwise, \c result will be set to \c ABT_FALSE.
 *
 * @param[in]  xstream1  handle to the ES 1
 * @param[in]  xstream2  handle to the ES 2
 * @param[out] result    comparison result (<tt>ABT_TRUE</tt>: same,
 *                       <tt>ABT_FALSE</tt>: not same)
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_xstream_equal(ABT_xstream xstream1, ABT_xstream xstream2,
                      ABT_bool *result)
{
    ABTI_xstream *p_xstream1 = ABTI_xstream_get_ptr(xstream1);
    ABTI_xstream *p_xstream2 = ABTI_xstream_get_ptr(xstream2);
    *result = (p_xstream1 == p_xstream2) ? ABT_TRUE : ABT_FALSE;
    return ABT_SUCCESS;
}

/**
 * @ingroup ES
 * @brief   Set the name for target ES
 *
 * @param[in] xstream  handle to the target ES
 * @param[in] name     ES name
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_xstream_set_name(ABT_xstream xstream, const char *name)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_xstream *p_xstream = ABTI_xstream_get_ptr(xstream);
    ABTI_CHECK_NULL_XSTREAM_PTR(p_xstream);

    size_t len = strlen(name);
    ABT_mutex_waitlock(p_xstream->mutex);
    if (p_xstream->p_name) ABTU_free(p_xstream->p_name);
    p_xstream->p_name = (char *)ABTU_malloc(len + 1);
    if (!p_xstream->p_name) {
        ABT_mutex_unlock(p_xstream->mutex);
        abt_errno = ABT_ERR_MEM;
        goto fn_fail;
    }
    ABTU_strcpy(p_xstream->p_name, name);
    ABT_mutex_unlock(p_xstream->mutex);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_xstream_set_name", abt_errno);
    goto fn_exit;
}

/**
 * @ingroup ES
 * @brief   Return the name of ES and its length.
 *
 * \c ABT_xstream_get_name() gets the string name of target ES and the length
 * of name in bytes. If \c name is NULL, only \c len is returned.
 * If \c name is not NULL, it should have enough space to save \c len bytes of
 * characters. If \c len is NULL, \c len is ignored.
 *
 * @param[in]  xstream  handle to the target ES
 * @param[out] name     ES name
 * @param[out] len      the length of name in bytes
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_xstream_get_name(ABT_xstream xstream, char *name, size_t *len)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_xstream *p_xstream = ABTI_xstream_get_ptr(xstream);
    ABTI_CHECK_NULL_XSTREAM_PTR(p_xstream);

    if (name != NULL) {
        ABTU_strcpy(name, p_xstream->p_name);
    }
    if (len != NULL) {
        *len = strlen(p_xstream->p_name);
    }

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_xstream_get_name", abt_errno);
    goto fn_exit;
}

/**
 * @ingroup ES
 * @brief   Return the number of current existing ESs.
 *
 * \c ABT_xstream_get_num() returns the number of ESs that exist in the current
 * Argobots environment through \c num_xstreams.
 *
 * @param[out] num_xstreams  the number of ESs
 * @return Error code
 * @retval ABT_SUCCESS           on success
 * @retval ABT_ERR_UNINITIALIZED Argobots has not been initialized
 */
int ABT_xstream_get_num(int *num_xstreams)
{
    int abt_errno = ABT_SUCCESS;

    /* In case that Argobots has not been initialized, return an error code
     * instead of making the call fail. */
    if (gp_ABTI_global == NULL) {
        abt_errno = ABT_ERR_UNINITIALIZED;
        *num_xstreams = 0;
        goto fn_exit;
    }

    ABTI_xstream_contn *p_xstreams = gp_ABTI_global->p_xstreams;
    *num_xstreams = (int)(ABTI_contn_get_size(p_xstreams->created)
            + ABTI_contn_get_size(p_xstreams->active));

  fn_exit:
    return abt_errno;
}

/**
 * @ingroup ES
 * @brief   Check if the target ES is the primary ES.
 *
 * \c ABT_xstream_is_primary() checks whether the target ES is the primary ES.
 * If the ES \c xstream is the primary ES, \c flag is set to \c ABT_TRUE.
 * Otherwise, \c flag is set to \c ABT_FALSE.
 *
 * @param[in]  xstream  handle to the target ES
 * @param[out] flag     result (<tt>ABT_TRUE</tt>: primary ES,
 *                      <tt>ABT_FALSE</tt>: not)
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_xstream_is_primary(ABT_xstream xstream, ABT_bool *flag)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_xstream *p_xstream;

    p_xstream = ABTI_xstream_get_ptr(xstream);
    ABTI_CHECK_NULL_XSTREAM_PTR(p_xstream);

    /* Return value */
    *flag = (p_xstream->type == ABTI_XSTREAM_TYPE_PRIMARY)
          ? ABT_TRUE : ABT_FALSE;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_xstream_is_primary", abt_errno);
    goto fn_exit;
}

/**
 * @ingroup ES
 * @brief   Execute a unit on the local ES.
 *
 * This function can be called by a scheduler after picking one unit. So a user
 * will use it for his own defined scheduler.
 *
 * @param[in] unit handle to the unit to run
 * @param[in] pool pool where unit is from
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_xstream_run_unit(ABT_unit unit, ABT_pool pool)
{
    int abt_errno = ABT_SUCCESS;

    ABTI_pool *p_pool = ABTI_pool_get_ptr(pool);

    ABT_unit_type type = p_pool->u_get_type(unit);

    /* Unset the current running ULT/tasklet */
    ABTI_thread *last_thread = ABTI_thread_current();
    ABTI_local_set_thread(NULL);
    ABTI_task *last_task = ABTI_task_current();
    ABTI_local_set_task(NULL);

    if (type == ABT_UNIT_TYPE_THREAD) {
        ABT_thread thread = p_pool->u_get_thread(unit);
        ABTI_thread *p_thread = ABTI_thread_get_ptr(thread);
        /* Switch the context */
        abt_errno = ABTI_xstream_schedule_thread(p_thread);
        ABTI_CHECK_ERROR(abt_errno);

    } else if (type == ABT_UNIT_TYPE_TASK) {
        ABT_task task = p_pool->u_get_task(unit);
        ABTI_task *p_task = ABTI_task_get_ptr(task);
        /* Execute the task */
        abt_errno = ABTI_xstream_schedule_task(p_task);
        ABTI_CHECK_ERROR(abt_errno);

    } else {
        HANDLE_ERROR("Not supported type!");
        abt_errno = ABT_ERR_INV_UNIT;
        goto fn_fail;
    }

    /* Set the current running ULT/tasklet */
    ABTI_local_set_thread(last_thread);
    ABTI_local_set_task(last_task);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_xstream_run_unit", abt_errno);
    goto fn_exit;
}

/**
 * @ingroup ES
 * @brief   Check the events and process them
 *
 * This function must be called by a scheduler periodically. Therefore, a user
 * will use it on his own defined scheduler.
 *
 * @param[in] sched handle to the scheduler where this call is from
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_xstream_check_events(ABT_sched sched)
{
    ABTI_xstream *p_xstream = ABTI_local_get_xstream();

    if (p_xstream->request & ABTI_XSTREAM_REQ_JOIN) {
        ABT_sched_finish(sched);
    }

    if ((p_xstream->request & ABTI_XSTREAM_REQ_EXIT) ||
        (p_xstream->request & ABTI_XSTREAM_REQ_CANCEL)) {
        ABT_sched_exit(sched);
    }

    // TODO: check event queue
    return 0;
}


/*****************************************************************************/
/* Private APIs                                                              */
/*****************************************************************************/
int ABTI_xstream_free(ABTI_xstream *p_xstream)
{
    int abt_errno = ABT_SUCCESS;

    ABTI_elem_free(&p_xstream->elem);
    if (p_xstream->p_name) ABTU_free(p_xstream->p_name);

    /* Free the scheduler */
    ABTI_sched *p_cursched = p_xstream->p_main_sched;
    if (p_cursched != NULL && p_cursched->automatic == ABT_TRUE) {
        ABT_sched h_sched = ABTI_sched_get_handle(p_cursched );
        abt_errno = ABT_sched_free(&h_sched);
        ABTI_CHECK_ERROR(abt_errno);
    }

    /* Clean up the deads pool */
    ABT_mutex_waitlock(p_xstream->mutex);
    ABTI_contn_free(&p_xstream->deads);
    ABT_mutex_unlock(p_xstream->mutex);

    /* Free the mutex */
    abt_errno = ABT_mutex_free(&p_xstream->mutex);
    ABTI_CHECK_ERROR(abt_errno);

    /* Free the array of sched contexts */
    ABTU_free(p_xstream->scheds);

    /* Free the context */
    abt_errno = ABTD_xstream_context_free(&p_xstream->ctx);
    ABTI_CHECK_ERROR(abt_errno);

    ABTU_free(p_xstream);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABTI_xstream_free", abt_errno);
    goto fn_exit;
}

int ABTI_xstream_start_any(void)
{
    int abt_errno = ABT_SUCCESS;

    ABTI_xstream_contn *p_xstreams = gp_ABTI_global->p_xstreams;

    /* If there exist one or more active xstreams or there is no xstream
     * created, we have nothing to do. */
    if (ABTI_contn_get_size(p_xstreams->active) > 0) goto fn_exit;
    if (ABTI_contn_get_size(p_xstreams->created) == 0) goto fn_exit;

    /* Pop one xstream from the global ES pool and start it */
    ABTI_xstream *p_xstream;
    abt_errno = ABTI_global_get_created_xstream(&p_xstream);
    ABTI_CHECK_ERROR(abt_errno);
    if (p_xstream == NULL) goto fn_exit;

    if (p_xstream->state == ABT_XSTREAM_STATE_CREATED) {
        ABT_xstream xstream = ABTI_xstream_get_handle(p_xstream);
        abt_errno = ABT_xstream_start(xstream);
        ABTI_CHECK_ERROR_MSG(abt_errno, "ABT_xstream_start");
    }

  fn_exit:
    return abt_errno;

  fn_fail:
    goto fn_exit;
}

int ABTI_xstream_schedule(ABTI_xstream *p_xstream)
{
    int abt_errno = ABT_SUCCESS;

    ABTI_CHECK_NULL_XSTREAM_PTR(p_xstream);

    p_xstream->state = ABT_XSTREAM_STATE_RUNNING;
    ABTI_sched *p_sched = p_xstream->p_main_sched;
    ABT_sched sched = ABTI_sched_get_handle(p_sched);
    ABTI_CHECK_NULL_SCHED_PTR(p_sched);

    p_sched->state = ABT_SCHED_STATE_RUNNING;

    p_sched->run(sched);
    p_sched->state = ABT_SCHED_STATE_TERMINATED;

    p_xstream->state = ABT_XSTREAM_STATE_READY;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABTI_xstream_schedule", abt_errno);
    goto fn_exit;
}

int ABTI_xstream_schedule_thread(ABTI_thread *p_thread)
{
    int abt_errno = ABT_SUCCESS;

    if ((p_thread->request & ABTI_THREAD_REQ_CANCEL) ||
        (p_thread->request & ABTI_THREAD_REQ_EXIT)) {
        abt_errno = ABTI_xstream_terminate_thread(p_thread);
        ABTI_CHECK_ERROR(abt_errno);
        goto fn_exit;
    }

    if (p_thread->request & ABTI_THREAD_REQ_MIGRATE) {
        abt_errno = ABTI_xstream_migrate_thread(p_thread);
        ABTI_CHECK_ERROR(abt_errno);
        goto fn_exit;
    }

    ABTI_xstream *p_xstream = ABTI_local_get_xstream();

    /* Set the current running ULT */
    ABTI_local_set_thread(p_thread);

    /* Now we can set the right link in the context */
    ABTD_thread_context *p_ctx = ABTI_xstream_get_sched_ctx();
    ABTD_thread_context_change_link(&p_thread->ctx, p_ctx);

    /* Add the new scheduler if the ULT is a scheduler */
    if (p_thread->is_sched != NULL) {
        p_thread->is_sched->p_ctx = &p_thread->ctx;
        ABTI_xstream_push_sched(p_xstream, p_thread->is_sched);
        p_thread->is_sched->state = ABT_SCHED_STATE_RUNNING;
    }

    /* Change the last ES */
    p_thread->p_last_xstream = p_xstream;

    /* Change the ULT state */
    p_thread->state = ABT_THREAD_STATE_RUNNING;

    /* Switch the context */
    DEBUG_PRINT("[S%" PRIu64 ":TH%" PRIu64 "] START\n",
                p_xstream->rank, p_thread->id);
    abt_errno = ABTD_thread_context_switch(p_ctx, &p_thread->ctx);

    /* The scheduler continues from here. */
    ABTI_CHECK_ERROR_MSG(abt_errno, "ABTD_thread_context_switch");

    /* The previous ULT may not be the same as one to which the
     * context has been switched. */
    p_thread = ABTI_local_get_thread();
    p_xstream = p_thread->p_last_xstream;
    DEBUG_PRINT("[S%" PRIu64 ":TH%" PRIu64 "] END\n",
                p_xstream->rank, p_thread->id);

    /* Delete the last scheduler if the ULT was a scheduler */
    if (p_thread->is_sched != NULL) {
        ABTI_xstream_pop_sched(p_xstream);
        /* If a migration is trying to read the state of the scheduler, we need
         * to let it finish before freeing the scheduler */
        p_thread->is_sched->state = ABT_SCHED_STATE_STOPPED;
        ABT_mutex_unlock(p_xstream->top_sched_mutex);
    }

    if ((p_thread->request & ABTI_THREAD_REQ_TERMINATE) ||
        (p_thread->request & ABTI_THREAD_REQ_CANCEL) ||
        (p_thread->request & ABTI_THREAD_REQ_EXIT)) {
        /* The ULT needs to be terminated. */
        abt_errno = ABTI_xstream_terminate_thread(p_thread);
    } else if (p_thread->request & ABTI_THREAD_REQ_BLOCK) {
        ABTD_atomic_fetch_and_uint32(&p_thread->request,
                                     ~ABTI_THREAD_REQ_BLOCK);
    } else {
        /* The ULT did not finish its execution.
         * Change the state of current running ULT and
         * add it to the pool again. */
        abt_errno = ABTI_xstream_add_thread(p_thread);
    }
    ABTI_CHECK_ERROR(abt_errno);

    /* Now, sched ULT is running */
    ABT_thread sched_thread = ABTI_xstream_get_top_sched(p_xstream)->thread;
    ABTI_local_set_thread(ABTI_thread_get_ptr(sched_thread));

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABTI_xstream_schedule_thread", abt_errno);
    goto fn_exit;
}

int ABTI_xstream_schedule_task(ABTI_task *p_task)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_xstream *p_xstream = ABTI_local_get_xstream();

    if (p_task->request & ABTI_TASK_REQ_CANCEL) {
        abt_errno = ABTI_xstream_terminate_task(p_task);
        ABTI_CHECK_ERROR(abt_errno);
        goto fn_exit;
    }

    /* Set the current running tasklet */
    ABTI_local_set_task(p_task);

    /* Change the task state */
    p_task->state = ABT_TASK_STATE_RUNNING;

    /* Set the associated ES */
    p_task->p_xstream = p_xstream;

    /* Add a new scheduler if the task is a scheduler */
    if (p_task->is_sched != NULL) {
        ABTI_sched *current_sched = ABTI_xstream_get_top_sched(p_xstream);
        ABTI_thread *last_thread = current_sched->thread;

        p_task->is_sched->p_ctx = ABTI_xstream_get_sched_ctx();
        ABTI_xstream_push_sched(p_xstream, p_task->is_sched);
        p_task->is_sched->state = ABT_SCHED_STATE_RUNNING;
        p_task->is_sched->thread = last_thread;
    }

    /* Execute the task function */
    DEBUG_PRINT("[S%" PRIu64 ":TK%" PRIu64 "] START\n",
                p_xstream->rank, p_task->id);
    p_task->f_task(p_task->p_arg);
    DEBUG_PRINT("[S%" PRIu64 ":TK%" PRIu64 "] END\n",
                p_xstream->rank, p_task->id);

    /* Delete the last scheduler if the ULT was a scheduler */
    if (p_task->is_sched != NULL) {
        ABTI_xstream_pop_sched(p_xstream);
        /* If a migration is trying to read the state of the scheduler, we need
         * to let it finish before freeing the scheduler */
        p_task->is_sched->state = ABT_SCHED_STATE_STOPPED;
        ABT_mutex_unlock(p_xstream->top_sched_mutex);
    }

    abt_errno = ABTI_xstream_terminate_task(p_task);
    ABTI_CHECK_ERROR(abt_errno);

    /* Unset the current running tasklet */
    ABTI_local_set_task(NULL);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABTI_xstream_schedule_task", abt_errno);
    goto fn_exit;
}

int ABTI_xstream_migrate_thread(ABTI_thread *p_thread)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_pool *p_pool;
    ABT_pool pool;
    ABTI_xstream *newstream = NULL;

    /* callback function */
    if (p_thread->attr.f_cb) {
        ABT_thread thread = ABTI_thread_get_handle(p_thread);
        p_thread->attr.f_cb(thread, p_thread->attr.p_cb_arg);
    }

    ABT_mutex_waitlock(p_thread->mutex); // TODO: mutex useful?
    {
        /* extracting argument in migration request */
        p_pool = (ABTI_pool *)ABTI_thread_extract_req_arg(p_thread,
                ABTI_THREAD_REQ_MIGRATE);
        pool = ABTI_pool_get_handle(p_pool);
        ABTD_atomic_fetch_and_uint32(&p_thread->request,
                ~ABTI_THREAD_REQ_MIGRATE);

        newstream = p_pool->reader;
        DEBUG_PRINT("[TH%" PRIu64 "] Migration: S%" PRIu64 " -> S%" PRIu64 "\n",
                p_thread->id, p_thread->p_last_xstream->rank,
                newstream? newstream->rank: -1);

        /* Change the associated pool */
        p_thread->p_pool = p_pool;

        /* Add the unit to the scheduler's pool */
        abt_errno = ABT_pool_push(pool, p_thread->unit);
    }
    ABT_mutex_unlock(p_thread->mutex);

    ABTI_pool_dec_num_migrations(p_pool);

    /* Check the push */
    ABTI_CHECK_ERROR(abt_errno);

    /* checking the state destination xstream */
    if (newstream && newstream->state == ABT_XSTREAM_STATE_CREATED) {
        ABT_xstream h_newstream = ABTI_xstream_get_handle(newstream);
        abt_errno = ABT_xstream_start(h_newstream);
        ABTI_CHECK_ERROR_MSG(abt_errno, "ABT_xstream_start");
    }

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABTI_xstream_migrate_thread", abt_errno);
    goto fn_exit;
}

int ABTI_xstream_terminate_thread(ABTI_thread *p_thread)
{
    int abt_errno;
    if (p_thread->refcount == 0) {
        p_thread->state = ABT_THREAD_STATE_TERMINATED;
        abt_errno = ABTI_thread_free(p_thread);
    } else {
        abt_errno = ABTI_xstream_keep_thread(p_thread);
    }
    return abt_errno;
}

int ABTI_xstream_terminate_task(ABTI_task *p_task)
{
    int abt_errno;
    if (p_task->refcount == 0) {
        p_task->state = ABT_TASK_STATE_TERMINATED;
        abt_errno = ABTI_task_free(p_task);
    } else {
        abt_errno = ABTI_xstream_keep_task(p_task);
    }
    return abt_errno;
}

int ABTI_xstream_add_thread(ABTI_thread *p_thread)
{
    int abt_errno;
    /* The thread's ES must not be changed during this function.
     * So, its mutex is used to guarantee it. */
    ABT_mutex_waitlock(p_thread->mutex);

    ABT_pool pool = ABTI_pool_get_handle(p_thread->p_pool);

    /* Set the thread's state as READY */
    p_thread->state = ABT_THREAD_STATE_READY;

    /* Add the unit to the scheduler's pool */
    abt_errno = ABT_pool_push(pool, p_thread->unit);

    ABT_mutex_unlock(p_thread->mutex);

    ABTI_CHECK_ERROR(abt_errno);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABTI_xstream_add_thread", abt_errno);
    goto fn_exit;
}

int ABTI_xstream_keep_thread(ABTI_thread *p_thread)
{
    /* The thread's ES must not be changed during this function.
     * So, its mutex is used to guarantee it. */
    ABT_mutex_waitlock(p_thread->mutex);

    ABTI_xstream *p_xstream = p_thread->p_last_xstream;

    /* Free the unit and create an internal unit to add to the deads pool. */
    ABTI_pool *p_pool = p_thread->p_pool;
    p_pool->u_free(&p_thread->unit);
    p_thread->unit = (ABT_unit)ABTI_elem_create_from_thread(p_thread);

    /* Add the unit to the deads pool */
    ABT_mutex_waitlock(p_xstream->mutex);
    ABTI_contn_push(p_xstream->deads, p_thread->unit);
    ABT_mutex_unlock(p_xstream->mutex);

    /* Set the thread's state as TERMINATED */
    p_thread->state = ABT_THREAD_STATE_TERMINATED;

    ABT_mutex_unlock(p_thread->mutex);

    return ABT_SUCCESS;
}

int ABTI_xstream_keep_task(ABTI_task *p_task)
{
    ABTI_xstream *p_xstream = p_task->p_xstream;

    /* Free the unit and create an internal unit to add to the deads pool. */
    ABTI_pool *p_pool = p_task->p_pool;
    p_pool->u_free(&p_task->unit);

    p_task->unit = (ABT_unit)ABTI_elem_create_from_task(p_task);

    /* Add the unit to the deads pool */
    ABT_mutex_waitlock(p_xstream->mutex);
    ABTI_contn_push(p_xstream->deads, p_task->unit);
    ABT_mutex_unlock(p_xstream->mutex);

    /* Set the task's state as TERMINATED */
    p_task->state = ABT_TASK_STATE_TERMINATED;

    return ABT_SUCCESS;
}

/* Add the specified scheduler to the sched stack (field scheds) */
int ABTI_xstream_push_sched(ABTI_xstream *p_xstream, ABTI_sched *p_sched)
{
    int abt_errno = ABT_SUCCESS;

    ABTI_CHECK_NULL_XSTREAM_PTR(p_xstream);

    if (p_xstream->num_scheds == p_xstream->max_scheds) {
        int max_size = p_xstream->max_scheds+10;
        void *temp = realloc(p_xstream->scheds, max_size*sizeof(ABTI_sched *));
        if (temp == NULL) {
            abt_errno = ABT_ERR_MEM;
            goto fn_fail;
        }
        p_xstream->scheds = (ABTI_sched **)temp;
        p_xstream->max_scheds = max_size;
    }

    p_xstream->scheds[p_xstream->num_scheds++] = p_sched;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABTI_xstream_push_sched", abt_errno);
    goto fn_exit;
}

/* Remove the top scheduler from the sched stack (field scheds) */
int ABTI_xstream_pop_sched(ABTI_xstream *p_xstream)
{
    int abt_errno = ABT_SUCCESS;

    p_xstream->num_scheds--;

    if (p_xstream->num_scheds < 0) {
        abt_errno = ABT_ERR_XSTREAM;
        goto fn_fail;
    }

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABTI_xstream_pop_sched", abt_errno);
    goto fn_exit;
}

/* Get the top scheduler from the sched stack (field scheds) */
ABTI_sched *ABTI_xstream_get_top_sched(ABTI_xstream *p_xstream)
{
    return p_xstream->scheds[p_xstream->num_scheds-1];
}

/* Get the scheduling context */
ABTD_thread_context *ABTI_xstream_get_sched_ctx(void)
{
    ABTI_xstream *p_xstream = ABTI_local_get_xstream();
    ABTI_sched *p_sched = ABTI_xstream_get_top_sched(p_xstream);
    return p_sched->p_ctx;
}


int ABTI_xstream_print(ABTI_xstream *p_xstream)
{
    int abt_errno = ABT_SUCCESS;
    if (p_xstream == NULL) {
        printf("NULL XSTREAM\n");
        goto fn_exit;
    }

    printf("== XSTREAM (%p) ==\n", p_xstream);
    printf("elem      : ");
    ABTI_elem_print(p_xstream->elem);
    printf("rank      : %" PRIu64 "\n", p_xstream->rank);
    printf("name      : %s\n", p_xstream->p_name);
    printf("type      : ");
    switch (p_xstream->type) {
        case ABTI_XSTREAM_TYPE_PRIMARY:   printf("PRIMARY\n"); break;
        case ABTI_XSTREAM_TYPE_SECONDARY: printf("SECONDARY\n"); break;
        default: printf("UNKNOWN\n"); break;
    }
    printf("state     : ");
    switch (p_xstream->state) {
        case ABT_XSTREAM_STATE_CREATED:    printf("CREATED\n"); break;
        case ABT_XSTREAM_STATE_READY:      printf("READY\n"); break;
        case ABT_XSTREAM_STATE_RUNNING:    printf("RUNNING\n"); break;
        case ABT_XSTREAM_STATE_TERMINATED: printf("TERMINATED\n"); break;
        default: printf("UNKNOWN\n"); break;
    }
    printf("request   : %x\n", p_xstream->request);

    printf("main sched: %p\n", p_xstream->p_main_sched);
    abt_errno = ABTI_sched_print(p_xstream->p_main_sched);
    ABTI_CHECK_ERROR(abt_errno);

    printf("deads     :");
    abt_errno = ABTI_contn_print(p_xstream->deads);
    ABTI_CHECK_ERROR(abt_errno);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABTI_xstream_print", abt_errno);
    goto fn_exit;
}

void *ABTI_xstream_launch_main_sched(void *p_arg)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_xstream *p_xstream = (ABTI_xstream *)p_arg;

    /* Initialization of the local variables */
    abt_errno = ABTI_local_init();
    ABTI_CHECK_ERROR(abt_errno);
    ABTI_local_set_xstream(p_xstream);

    ABTI_sched *p_sched = p_xstream->p_main_sched;
    p_sched->p_ctx = (ABTD_thread_context *)
        malloc(sizeof(ABTD_thread_context));

    /* Create a context */
    // TODO size of the stack? XXX
    abt_errno = ABTD_thread_context_create(NULL, NULL, NULL,
            ABTI_global_get_default_stacksize(), NULL, p_sched->p_ctx);
    ABTI_CHECK_ERROR(abt_errno);

    ABTI_xstream_loop(p_arg);

  fn_exit:
    return NULL;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABTI_xstream_launch_main_sched", abt_errno);
    goto fn_exit;
}

// TODO merge with ABTI_xstream_schedule
void ABTI_xstream_loop(void *p_arg)
{
    ABTI_xstream *p_xstream = (ABTI_xstream *)p_arg;

    /* Set the CPU affinity for the ES */
    if (gp_ABTI_global->set_affinity == ABT_TRUE) {
        ABTD_xstream_context_set_affinity(p_xstream->ctx, p_xstream->rank);
    }

    DEBUG_PRINT("[S%" PRIu64 "] START\n", p_xstream->rank);

    /* Set this ES as the current ES */
    ABTI_local_set_xstream(p_xstream);

    /* Set the sched ULT as the current thread */
    ABT_thread sched_thread = ABTI_xstream_get_top_sched(p_xstream)->thread;
    ABTI_local_set_thread(ABTI_thread_get_ptr(sched_thread));

    while (1) {
        int abt_errno = ABTI_xstream_schedule(p_xstream);
        ABTI_CHECK_ERROR_MSG(abt_errno, "ABTI_xstream_schedule");
        ABT_mutex_unlock(p_xstream->top_sched_mutex);

        /* If there is an exit or a cancel request, the ES terminates
         * regardless of remaining work units. */
        if ((p_xstream->request & ABTI_XSTREAM_REQ_EXIT) ||
            (p_xstream->request & ABTI_XSTREAM_REQ_CANCEL))
            break;

        /* When join is requested, the ES terminates after finishing
         * execution of all work units. */
        if (p_xstream->request & ABTI_XSTREAM_REQ_JOIN)
            break;
    }

    /* Set the xstream's state as TERMINATED */
    p_xstream->state = ABT_XSTREAM_STATE_TERMINATED;

    if (p_xstream->type != ABTI_XSTREAM_TYPE_PRIMARY) {
        /* Move the xstream to the deads pool */
        ABTI_global_move_xstream(p_xstream);

        /* Reset the current ES and thread info. */
        ABTI_local_finalize();

        DEBUG_PRINT("[S%" PRIu64 "] END\n", p_xstream->rank);

        ABTD_xstream_context_exit();
    }
  fn_fail:
    return;

}

/* global rank variable for ES */
static uint64_t g_xstream_rank = 0;

/* Reset the ES rank value */
void ABTI_xstream_reset_rank(void)
{
    g_xstream_rank = 0;
}

/*****************************************************************************/
/* Internal static functions                                                 */
/*****************************************************************************/

/* Get a new ES rank */
static uint64_t ABTI_xstream_get_new_rank(void)
{
    return ABTD_atomic_fetch_add_uint64(&g_xstream_rank, 1);
}


