/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"

static void ABTI_xstream_set_new_rank(ABTI_xstream *);
static ABT_bool ABTI_xstream_take_rank(ABTI_xstream *, int);
static void ABTI_xstream_return_rank(ABTI_xstream *);


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
    ABTI_sched *p_sched;
    ABTI_xstream *p_newxstream;

    if (sched == ABT_SCHED_NULL) {
        abt_errno = ABT_sched_create_basic(ABT_SCHED_DEFAULT, 0, NULL,
                                           ABT_SCHED_CONFIG_NULL, &sched);
        ABTI_CHECK_ERROR(abt_errno);
        p_sched = ABTI_sched_get_ptr(sched);
    } else {
        p_sched = ABTI_sched_get_ptr(sched);
        ABTI_CHECK_TRUE(p_sched->used == ABTI_SCHED_NOT_USED,
                        ABT_ERR_INV_SCHED);
    }

    abt_errno = ABTI_xstream_create(p_sched, &p_newxstream);
    ABTI_CHECK_ERROR(abt_errno);

#ifdef ABT_XSTREAM_USE_VIRTUAL
        ABTI_xstream *v_newxstream;
	abt_errno = ABTI_xstream_create_virtual_basic(&v_newxstream, p_newxstream);
#endif

    /* Start this ES */
    abt_errno = ABTI_xstream_start(p_newxstream);
    ABTI_CHECK_ERROR(abt_errno);

    /* Return value */
    *newxstream = ABTI_xstream_get_handle(p_newxstream);

  fn_exit:
    return abt_errno;

  fn_fail:
    *newxstream = ABT_XSTREAM_NULL;
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}


int ABTI_xstream_create(ABTI_sched *p_sched, ABTI_xstream **pp_xstream)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_xstream *p_newxstream;

    p_newxstream = (ABTI_xstream *)ABTU_malloc(sizeof(ABTI_xstream));

    ABTI_xstream_set_new_rank(p_newxstream);

    /* Create a wrapper unit */
    ABTI_elem_create_from_xstream(p_newxstream);

    p_newxstream->type         = ABTI_XSTREAM_TYPE_SECONDARY;
    p_newxstream->state        = ABT_XSTREAM_STATE_CREATED;
    p_newxstream->scheds       = NULL;
    p_newxstream->num_scheds   = 0;
    p_newxstream->max_scheds   = 0;
    p_newxstream->request      = 0;
    p_newxstream->p_req_arg    = NULL;
    p_newxstream->p_main_sched = NULL;

    /* Create the spinlock */
    ABTI_spinlock_create(&p_newxstream->sched_lock);

    /* Set the main scheduler */
    abt_errno = ABTI_xstream_set_main_sched(p_newxstream, p_sched);
    ABTI_CHECK_ERROR(abt_errno);

#ifdef ABT_XSTREAM_USE_VIRTUAL
    p_sched->is_master_sched = ABT_TRUE;

    /*  Initialize virtual xstreams count to 0 and allocate memory  */
    p_sched->num_vxstreams = 0;
    p_sched->v_xstreams = (ABTI_xstream **) ABTU_malloc(sizeof(ABTI_xstream*) * gp_ABTI_global->max_vxstreams);
#endif

    LOG_EVENT("[E%d] created\n", p_newxstream->rank);

    /* Return value */
    *pp_xstream = p_newxstream;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

int ABTI_xstream_create_primary(ABTI_xstream **pp_xstream)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_xstream *p_newxstream;
    ABT_sched sched;

    /* For the primary ES, a default scheduler is created. */
    abt_errno = ABT_sched_create_basic(ABT_SCHED_DEFAULT, 0, NULL,
                                       ABT_SCHED_CONFIG_NULL, &sched);
    ABTI_CHECK_ERROR(abt_errno);

    abt_errno = ABTI_xstream_create(ABTI_sched_get_ptr(sched), &p_newxstream);
    ABTI_CHECK_ERROR(abt_errno);

    p_newxstream->type = ABTI_XSTREAM_TYPE_PRIMARY;

    *pp_xstream = p_newxstream;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

#ifdef ABT_XSTREAM_USE_VIRTUAL
int ABTI_xstream_create_virtual_basic(ABTI_xstream **v_xstream, ABTI_xstream *p_xstream)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_xstream *v_newxstream;
    ABT_sched sched;
    
    abt_errno = ABT_sched_create_basic(ABT_SCHED_DEFAULT, 0, NULL,
		                                       ABT_SCHED_CONFIG_NULL, &sched);
    ABTI_CHECK_ERROR(abt_errno);

    abt_errno = ABTI_xstream_create(ABTI_sched_get_ptr(sched), &v_newxstream);
    ABTI_CHECK_ERROR(abt_errno);
    
    v_newxstream->type = ABTI_XSTREAM_TYPE_VIRTUAL;
    *v_xstream = v_newxstream;

    ABTI_sched *p_sched = ABTI_sched_get_ptr(p_xstream->p_main_sched);
    v_newxstream->rank = p_sched->num_vxstreams;
    p_sched->v_xstreams[(p_sched->num_vxstreams)++] = v_newxstream;

    //TODO: Push this to the scheduler pool
    ABTI_sched *v_sched = v_newxstream->p_main_sched;
    abt_errno = ABTI_thread_create_main_sched_virtual(p_xstream, v_newxstream, v_sched);
    ABTI_CHECK_ERROR(abt_errno);
    v_sched->p_thread->p_last_xstream = p_xstream;

    LOG_EVENT("[vE%d] created\n", v_newxstream->rank);
  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}
#endif

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
 * @param[in]  config       specific config used during the scheduler creation
 * @param[out] newxstream   handle to the target ES
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_xstream_create_basic(ABT_sched_predef predef, int num_pools,
                             ABT_pool *pools, ABT_sched_config config,
                             ABT_xstream *newxstream)
{
    int abt_errno = ABT_SUCCESS;

    ABT_sched sched;
    abt_errno = ABT_sched_create_basic(predef, num_pools, pools,
                                       config, &sched);
    ABTI_CHECK_ERROR(abt_errno);

    abt_errno = ABT_xstream_create(sched, newxstream);
    ABTI_CHECK_ERROR(abt_errno);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

/**
 * @ingroup ES
 * @brief   Create a new ES with a specific rank.
 *
 * @param[in]  sched  handle to the scheduler used for a new ES. If this is
 *                    ABT_SCHED_NULL, the runtime-provided scheduler is used.
 * @param[in]  rank   target rank
 * @param[out] newxstream  handle to a newly created ES. This cannot be NULL
 *                    because unnamed ES is not allowed.
 * @return Error code
 * @retval ABT_SUCCESS               on success
 * @retval ABT_ERR_INV_XSTREAM_RANK  invalid rank
 */
int ABT_xstream_create_with_rank(ABT_sched sched, int rank,
                                 ABT_xstream *newxstream)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_xstream *p_newxstream;
    ABTI_sched *p_sched;

    ABTI_CHECK_TRUE(rank >= 0, ABT_ERR_INV_XSTREAM_RANK);

    p_newxstream = (ABTI_xstream *)ABTU_malloc(sizeof(ABTI_xstream));

    if (ABTI_xstream_take_rank(p_newxstream, rank) == ABT_FALSE) {
        ABTU_free(p_newxstream);
        abt_errno = ABT_ERR_INV_XSTREAM_RANK;
        *newxstream = ABT_XSTREAM_NULL;
        return abt_errno;
    }

    if (sched == ABT_SCHED_NULL) {
        abt_errno = ABT_sched_create_basic(ABT_SCHED_DEFAULT, 0, NULL,
                                           ABT_SCHED_CONFIG_NULL, &sched);
        ABTI_CHECK_ERROR(abt_errno);
        p_sched = ABTI_sched_get_ptr(sched);
    } else {
        p_sched = ABTI_sched_get_ptr(sched);
        ABTI_CHECK_TRUE(p_sched->used == ABTI_SCHED_NOT_USED,
                        ABT_ERR_INV_SCHED);
    }

    /* Create a wrapper unit */
    ABTI_elem_create_from_xstream(p_newxstream);

    p_newxstream->type         = ABTI_XSTREAM_TYPE_SECONDARY;
    p_newxstream->state        = ABT_XSTREAM_STATE_CREATED;
    p_newxstream->scheds       = NULL;
    p_newxstream->num_scheds   = 0;
    p_newxstream->max_scheds   = 0;
    p_newxstream->request      = 0;
    p_newxstream->p_req_arg    = NULL;
    p_newxstream->p_main_sched = NULL;

    /* Create the spinlock */
    ABTI_spinlock_create(&p_newxstream->sched_lock);

    /* Set the main scheduler */
    abt_errno = ABTI_xstream_set_main_sched(p_newxstream, p_sched);
    ABTI_CHECK_ERROR(abt_errno);

    LOG_EVENT("[E%d] created\n", p_newxstream->rank);

    /* Start this ES */
    abt_errno = ABTI_xstream_start(p_newxstream);
    ABTI_CHECK_ERROR(abt_errno);

    /* Return value */
    *newxstream = ABTI_xstream_get_handle(p_newxstream);

  fn_exit:
    return abt_errno;

  fn_fail:
    *newxstream = ABT_XSTREAM_NULL;
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

/**
 * @ingroup ES
 * @brief   Start the target ES.
 *
 * \c ABT_xstream_start() starts the target ES \c xstream if it has not been
 * started.  That is, this routine is effective only when the state of the
 * target ES is CREATED or READY, and once this routine returns, the ES's state
 * becomes RUNNING.
 *
 * @param[in] xstream  handle to the target ES
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_xstream_start(ABT_xstream xstream)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_xstream *p_xstream = ABTI_xstream_get_ptr(xstream);
    ABTI_CHECK_NULL_XSTREAM_PTR(p_xstream);

    abt_errno = ABTI_xstream_start(p_xstream);
    ABTI_CHECK_ERROR(abt_errno);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

int ABTI_xstream_start(ABTI_xstream *p_xstream)
{
    int abt_errno = ABT_SUCCESS;

    /* Set the ES's state as READY */
    if (!ABTD_atomic_bool_cas_strong_int32((int32_t *)&p_xstream->state,
                                           ABT_XSTREAM_STATE_CREATED,
                                           ABT_XSTREAM_STATE_READY)) {
	goto fn_exit;
    }

    /* Add the main scheduler to the stack of schedulers */
    ABTI_xstream_push_sched(p_xstream, p_xstream->p_main_sched);

    if (p_xstream->type == ABTI_XSTREAM_TYPE_PRIMARY) {
        LOG_EVENT("[E%d] start\n", p_xstream->rank);

        abt_errno = ABTD_xstream_context_self(&p_xstream->ctx);
        ABTI_CHECK_ERROR_MSG(abt_errno, "ABTD_xstream_context_self");

        /* Create the main sched ULT */
        ABTI_sched *p_sched = p_xstream->p_main_sched;
        abt_errno = ABTI_thread_create_main_sched(p_xstream, p_sched);
        ABTI_CHECK_ERROR(abt_errno);
        p_sched->p_thread->p_last_xstream = p_xstream;

    } else {
        /* Start the main scheduler on a different ES */
        abt_errno = ABTD_xstream_context_create(
                ABTI_xstream_launch_main_sched, (void *)p_xstream,
                &p_xstream->ctx);
        ABTI_CHECK_ERROR_MSG(abt_errno, "ABTD_xstream_context_create");
    }

    /* Set the CPU affinity for the ES */
    if (gp_ABTI_global->set_affinity == ABT_TRUE) {
        ABTD_affinity_set(p_xstream->ctx, p_xstream->rank);
    }

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

/* This routine starts the primary ES. It should be called in ABT_init.
 * [in] p_xstream  the primary ES
 * [in] p_thread   the main ULT
 */
int ABTI_xstream_start_primary(ABTI_xstream *p_xstream, ABTI_thread *p_thread)
{
    int abt_errno = ABT_SUCCESS;

    /* Add the main scheduler to the stack of schedulers */
    ABTI_xstream_push_sched(p_xstream, p_xstream->p_main_sched);

    /* Set the ES's state to READY.  The ES's state will be set to RUNNING in
     * ABTI_xstream_schedule(). */
    p_xstream->state = ABT_XSTREAM_STATE_READY;

    LOG_EVENT("[E%d] start\n", p_xstream->rank);

    abt_errno = ABTD_xstream_context_self(&p_xstream->ctx);
    ABTI_CHECK_ERROR_MSG(abt_errno, "ABTD_xstream_context_self");

    /* Set the CPU affinity for the ES */
    if (gp_ABTI_global->set_affinity == ABT_TRUE) {
        ABTD_affinity_set(p_xstream->ctx, p_xstream->rank);
    }

    /* Create the main sched ULT */
    ABTI_sched *p_sched = p_xstream->p_main_sched;
    abt_errno = ABTI_thread_create_main_sched(p_xstream, p_sched);
    ABTI_CHECK_ERROR(abt_errno);
    p_sched->p_thread->p_last_xstream = p_xstream;

    /* Start the scheduler by context switching to it */
    LOG_EVENT("[U%" PRIu64 ":E%d] yield\n",
              ABTI_thread_get_id(p_thread), p_thread->p_last_xstream->rank);
    ABTI_thread_context_switch_thread_to_sched(p_thread, p_sched);

    /* Back to the main ULT */
    LOG_EVENT("[U%" PRIu64 ":E%d] resume\n",
              ABTI_thread_get_id(p_thread), p_thread->p_last_xstream->rank);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
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

    /* We first need to check whether lp_ABTI_local is NULL because this
     * routine might be called by external threads. */
    ABTI_CHECK_TRUE_MSG(lp_ABTI_local == NULL ||
                          p_xstream != ABTI_local_get_xstream(),
                        ABT_ERR_INV_XSTREAM,
                        "The current xstream cannot be freed.");

    ABTI_CHECK_TRUE_MSG(p_xstream->type != ABTI_XSTREAM_TYPE_PRIMARY,
                        ABT_ERR_INV_XSTREAM,
                        "The primary xstream cannot be freed explicitly.");

    /* Wait until xstream terminates */
    if (p_xstream->state != ABT_XSTREAM_STATE_TERMINATED) {
        abt_errno = ABT_xstream_join(h_xstream);
        ABTI_CHECK_ERROR(abt_errno);
    }

    /* Free the xstream object */
    abt_errno = ABTI_xstream_free(p_xstream);
    ABTI_CHECK_ERROR(abt_errno);

    /* Return value */
    *xstream = ABT_XSTREAM_NULL;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
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
    ABTI_thread *p_thread;
    ABT_bool is_blockable = ABT_FALSE;

    ABTI_CHECK_TRUE_MSG(p_xstream->type != ABTI_XSTREAM_TYPE_PRIMARY,
                        ABT_ERR_INV_XSTREAM,
                        "The primary ES cannot be joined.");

    if (p_xstream->state == ABT_XSTREAM_STATE_CREATED) {
        /* If xstream's state was changed, we cannot terminate it here */
        if (!ABTD_atomic_bool_cas_strong_int32((int32_t *)&p_xstream->state,
                                               ABT_XSTREAM_STATE_CREATED,
                                               ABT_XSTREAM_STATE_TERMINATED)) {
            goto fn_body;
        }
        goto fn_exit;
    }

  fn_body:
    /* When the associated pool of the caller ULT has multiple-writer access
     * mode, the ULT can be blocked. Otherwise, the access mode, if it is a
     * single-writer access mode, may be violated because another ES has to set
     * the blocked ULT ready. */
    p_thread = lp_ABTI_local ? ABTI_local_get_thread() : NULL;
    if (p_thread) {
        ABT_pool_access access = p_thread->p_pool->access;
        if (access == ABT_POOL_ACCESS_MPSC || access == ABT_POOL_ACCESS_MPMC) {
            is_blockable = ABT_TRUE;
        }

        /* The target ES must not be the same as the caller ULT's ES if the
         * access mode of the associated pool is not MPMC. */
        if (access != ABT_POOL_ACCESS_MPMC) {
            ABTI_CHECK_TRUE_MSG(p_xstream != ABTI_local_get_xstream(),
                                ABT_ERR_INV_XSTREAM,
                                "The target ES should be different.");
        }
    }

    if (ABTD_atomic_load_uint32((uint32_t *)&p_xstream->state)
        == ABT_XSTREAM_STATE_TERMINATED) {
        goto fn_join;
    }

    /* Wait until the target ES terminates */
    if (is_blockable == ABT_TRUE) {
        ABTI_POOL_SET_CONSUMER(p_thread->p_pool, ABTI_local_get_xstream());

        /* Save the caller ULT to set it ready when the ES is terminated */
        p_xstream->p_req_arg = (void *)p_thread;
        ABTI_thread_set_blocked(p_thread);

        /* Set the join request */
        ABTI_xstream_set_request(p_xstream, ABTI_XSTREAM_REQ_JOIN);

        /* If the caller is a ULT, it is blocked here */
        ABTI_thread_suspend(p_thread);
    } else {
        /* Set the join request */
        ABTI_xstream_set_request(p_xstream, ABTI_XSTREAM_REQ_JOIN);

        while (ABTD_atomic_load_uint32((uint32_t *)&p_xstream->state)
               != ABT_XSTREAM_STATE_TERMINATED) {
            ABT_thread_yield();
        }
    }

  fn_join:
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
    ABTI_xstream_set_request(p_xstream, ABTI_XSTREAM_REQ_EXIT);

    /* Wait until the ES terminates */
    do {
        ABT_thread_yield();
    } while (ABTD_atomic_load_uint32((uint32_t *)&p_xstream->state)
             != ABT_XSTREAM_STATE_TERMINATED);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
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

    ABTI_CHECK_TRUE_MSG(p_xstream->type != ABTI_XSTREAM_TYPE_PRIMARY,
                        ABT_ERR_INV_XSTREAM,
                        "The primary xstream cannot be canceled.");

    /* Set the cancel request */
    ABTI_xstream_set_request(p_xstream, ABTI_XSTREAM_REQ_CANCEL);

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

#ifdef ABT_XSTREAM_USE_VIRTUAL
    ABTI_sched* p_sched = ABTI_sched_get_ptr(p_xstream->p_main_sched);
    //TODO: Determine which virtual xstream to self for better load balancing.
    *xstream = p_sched->v_xstreams[0];
#else
    /* Return value */
    *xstream = ABTI_xstream_get_handle(p_xstream);
#endif

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
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
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
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

    p_xstream->rank = rank;

    /* Set the CPU affinity for the ES */
    if (gp_ABTI_global->set_affinity == ABT_TRUE) {
        ABTD_affinity_set(p_xstream->ctx, p_xstream->rank);
    }

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
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
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

/**
 * @ingroup ES
 * @brief   Set the main scheduler of the target ES.
 *
 * \c ABT_xstream_set_main_sched() sets \c sched as the main scheduler for
 * \c xstream.  The scheduler \c sched will first run when the ES \c xstream is
 * started.  Only ULTs can call this routine.
 * If \c xstream is a handle to the primary ES, \c sched will be automatically
 * freed on \c ABT_finalize() or when the main scheduler of the primary ES is
 * changed again.  In this case, the explicit call \c ABT_sched_free() for
 * \c sched may cause undefined behavior.
 *
 * NOTE: The current implementation of this routine has some limitations.
 * 1. If the target ES \c xstream is running, the caller ULT must be running on
 * the same ES. However, if the target ES is not in the RUNNING state, the
 * caller can be any ULT that is running on any ES.
 * 2. If the current main scheduler of \c xstream has work units residing in
 * its associated pools, this routine will not be successful. In this case, the
 * user has to complete all work units in the main scheduler's pools or migrate
 * them to unassociated pools.
 *
 * @param[in] xstream  handle to the target ES
 * @param[in] sched    handle to the scheduler
 * @return Error code
 * @retval ABT_SUCCESS         on success
 * @retval ABT_ERR_INV_THREAD  the caller is not ULT
 * @retval ABT_ERR_XSTREAM     the current main scheduler of \c xstream has
 *                             work units in its associated pools
 */
int ABT_xstream_set_main_sched(ABT_xstream xstream, ABT_sched sched)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_sched *p_sched;

    ABTI_CHECK_TRUE(lp_ABTI_local != NULL, ABT_ERR_INV_THREAD);

    ABTI_xstream *p_xstream = ABTI_xstream_get_ptr(xstream);
    ABTI_CHECK_NULL_XSTREAM_PTR(p_xstream);

    ABTI_thread *p_thread = ABTI_local_get_thread();
    ABTI_CHECK_TRUE(p_thread != NULL, ABT_ERR_INV_THREAD);

    /* For now, if the target ES is running, we allow to change the main
     * scheduler of the ES only when the caller is running on the same ES. */
    /* TODO: a new state representing that the scheduler is changed is needed
     * to avoid running xstreams while the scheduler is changed in this
     * function. */
    if (p_xstream->state == ABT_XSTREAM_STATE_RUNNING) {
        if (p_thread->p_last_xstream != p_xstream) {
            abt_errno = ABT_ERR_XSTREAM_STATE;
            goto fn_fail;
        }
    }

    /* TODO: permit to change the scheduler even when having work units in pools */
    if (p_xstream->p_main_sched) {
        /* We only allow to change the main scheduler when the current main
         * scheduler of p_xstream has no work unit in its associated pools. */
        if (ABTI_sched_get_effective_size(p_xstream->p_main_sched) > 0) {
            abt_errno = ABT_ERR_XSTREAM;
            goto fn_fail;
        }
    }

    if (sched == ABT_SCHED_NULL) {
        abt_errno = ABT_sched_create_basic(ABT_SCHED_DEFAULT, 0, NULL,
                                           ABT_SCHED_CONFIG_NULL, &sched);
        ABTI_CHECK_ERROR(abt_errno);
        p_sched = ABTI_sched_get_ptr(sched);
    } else {
        p_sched = ABTI_sched_get_ptr(sched);
        ABTI_CHECK_TRUE(p_sched->used == ABTI_SCHED_NOT_USED,
                        ABT_ERR_INV_SCHED);
    }

    abt_errno = ABTI_xstream_set_main_sched(p_xstream, p_sched);
    ABTI_CHECK_ERROR(abt_errno);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

/**
 * @ingroup ES
 * @brief   Set the main scheduler for \c xstream with a predefined scheduler.
 *
 * See \c ABT_xstream_set_main_sched() for more details.
 *
 * @param[in] xstream     handle to the target ES
 * @param[in] predef      predefined scheduler
 * @param[in] num_pools   number of pools associated with this scheduler
 * @param[in] pools       pools associated with this scheduler
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_xstream_set_main_sched_basic(ABT_xstream xstream,
        ABT_sched_predef predef, int num_pools, ABT_pool *pools)
{
    int abt_errno = ABT_SUCCESS;

    ABTI_xstream *p_xstream = ABTI_xstream_get_ptr(xstream);
    ABTI_CHECK_NULL_XSTREAM_PTR(p_xstream);

    ABT_sched sched;
    abt_errno = ABT_sched_create_basic(predef, num_pools, pools,
                                       ABT_SCHED_CONFIG_NULL, &sched);
    ABTI_CHECK_ERROR(abt_errno);
    ABTI_sched *p_sched = ABTI_sched_get_ptr(sched);

    abt_errno = ABTI_xstream_set_main_sched(p_xstream, p_sched);
    ABTI_CHECK_ERROR(abt_errno);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
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
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
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
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
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
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
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

    *num_xstreams = gp_ABTI_global->num_xstreams;

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
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
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
    int abt_errno;
    ABTI_xstream *p_xstream = ABTI_local_get_xstream();
    ABTI_pool *p_pool = ABTI_pool_get_ptr(pool);

    abt_errno = ABTI_xstream_run_unit(p_xstream, unit, p_pool);
    ABTI_CHECK_ERROR(abt_errno);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

int ABTI_xstream_run_unit(ABTI_xstream *p_xstream, ABT_unit unit,
                          ABTI_pool *p_pool)
{
    int abt_errno = ABT_SUCCESS;

    ABT_unit_type type = p_pool->u_get_type(unit);

    if (type == ABT_UNIT_TYPE_THREAD) {
        ABT_thread thread = p_pool->u_get_thread(unit);
        ABTI_thread *p_thread = ABTI_thread_get_ptr(thread);
        /* Switch the context */
        abt_errno = ABTI_xstream_schedule_thread(p_xstream, p_thread);
        ABTI_CHECK_ERROR(abt_errno);

    } else if (type == ABT_UNIT_TYPE_TASK) {
        ABT_task task = p_pool->u_get_task(unit);
        ABTI_task *p_task = ABTI_task_get_ptr(task);
        /* Execute the task */
        ABTI_xstream_schedule_task(p_xstream, p_task);

    } else {
        HANDLE_ERROR("Not supported type!");
        ABTI_CHECK_TRUE(0, ABT_ERR_INV_UNIT);
    }

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
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

    abt_errno = ABTI_xstream_check_events(p_xstream, sched);
    ABTI_CHECK_ERROR(abt_errno);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

int ABTI_xstream_check_events(ABTI_xstream *p_xstream, ABT_sched sched)
{
    int abt_errno = ABT_SUCCESS;

    ABTI_info_check_print_all_thread_stacks();

    if (p_xstream->request & ABTI_XSTREAM_REQ_JOIN) {
        abt_errno = ABT_sched_finish(sched);
        ABTI_CHECK_ERROR(abt_errno);
    }

    if ((p_xstream->request & ABTI_XSTREAM_REQ_EXIT) ||
        (p_xstream->request & ABTI_XSTREAM_REQ_CANCEL)) {
        abt_errno = ABT_sched_exit(sched);
        ABTI_CHECK_ERROR(abt_errno);
    }

    // TODO: check event queue
#ifdef ABT_CONFIG_HANDLE_POWER_EVENT
    if (ABTI_event_check_power() == ABT_TRUE) {
        abt_errno = ABT_sched_exit(sched);
        ABTI_CHECK_ERROR(abt_errno);
    }
#endif
    ABTI_EVENT_PUBLISH_INFO();

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}


/**
 * @ingroup ES
 * @brief   Bind the target ES to a target CPU.
 *
 * \c ABT_xstream_set_cpubind() binds the target ES \c xstream to the target
 * CPU whose ID is \c cpuid.  Here, the CPU ID corresponds to the processor
 * index used by OS.
 *
 * @param[in] xstream  handle to the target ES
 * @param[in] cpuid    CPU ID
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_xstream_set_cpubind(ABT_xstream xstream, int cpuid)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_xstream *p_xstream = ABTI_xstream_get_ptr(xstream);
    ABTI_CHECK_NULL_XSTREAM_PTR(p_xstream);

    abt_errno = ABTD_affinity_set_cpuset(p_xstream->ctx, 1, &cpuid);
    ABTI_CHECK_ERROR(abt_errno);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

/**
 * @ingroup ES
 * @brief   Get the CPU binding for the target ES.
 *
 * \c ABT_xstream_get_cpubind() returns the ID of CPU, which the target ES
 * \c xstream is bound to.  If \c xstream is bound to more than one CPU, only
 * the first CPU ID is returned.
 *
 * @param[in] xstream  handle to the target ES
 * @param[out] cpuid   CPU ID
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_xstream_get_cpubind(ABT_xstream xstream, int *cpuid)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_xstream *p_xstream = ABTI_xstream_get_ptr(xstream);
    ABTI_CHECK_NULL_XSTREAM_PTR(p_xstream);

    abt_errno = ABTD_affinity_get_cpuset(p_xstream->ctx, 1, cpuid, NULL);
    ABTI_CHECK_ERROR(abt_errno);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

/**
 * @ingroup ES
 * @brief   Set the CPU affinity of the target ES.
 *
 * \c ABT_xstream_set_cpubind() binds the target ES \c xstream on the given CPU
 * set, \c cpuset, which is an array of CPU IDs.  Here, the CPU IDs correspond
 * to processor indexes used by OS.
 *
 * @param[in] xstream      handle to the target ES
 * @param[in] cpuset_size  the number of \c cpuset entries
 * @param[in] cpuset       array of CPU IDs
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_xstream_set_affinity(ABT_xstream xstream, int cpuset_size, int *cpuset)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_xstream *p_xstream = ABTI_xstream_get_ptr(xstream);
    ABTI_CHECK_NULL_XSTREAM_PTR(p_xstream);

    abt_errno = ABTD_affinity_set_cpuset(p_xstream->ctx, cpuset_size, cpuset);
    ABTI_CHECK_ERROR(abt_errno);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

/**
 * @ingroup ES
 * @brief   Get the CPU affinity for the target ES.
 *
 * \c ABT_xstream_get_cpubind() writes CPU IDs (at most, \c cpuset_size) to
 * \c cpuset and returns the number of elements written to \c cpuset to
 * \c num_cpus.  If \c num_cpus is \c NULL, it is ignored.
 *
 * If \c cpuset is \c NULL, \c cpuset_size is ignored and the nubmer of all
 * CPUs on which \c xstream is bound is returned through \c num_cpus.
 * Otherwise, i.e., if \c cpuset is \c NULL, \c cpuset_size must be greater
 * than zero.
 *
 * @param[in]  xstream      handle to the target ES
 * @param[in]  cpuset_size  the number of \c cpuset entries
 * @param[out] cpuset       array of CPU IDs
 * @param[out] num_cpus     the number of total CPU IDs
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_xstream_get_affinity(ABT_xstream xstream, int cpuset_size, int *cpuset,
                             int *num_cpus)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_xstream *p_xstream = ABTI_xstream_get_ptr(xstream);
    ABTI_CHECK_NULL_XSTREAM_PTR(p_xstream);

    if (cpuset == NULL && num_cpus == NULL) {
        goto fn_exit;
    }

    abt_errno = ABTD_affinity_get_cpuset(p_xstream->ctx, cpuset_size, cpuset,
                                         num_cpus);
    ABTI_CHECK_ERROR(abt_errno);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}


/*****************************************************************************/
/* Private APIs                                                              */
/*****************************************************************************/
int ABTI_xstream_free(ABTI_xstream *p_xstream)
{
    int abt_errno = ABT_SUCCESS;

    LOG_EVENT("[E%d] freed\n", p_xstream->rank);

    /* Return rank for reuse. rank must be returned prior to other free
     * functions so that other xstreams cannot refer to this xstream via
     * global->p_xstreams. */
    ABTI_xstream_return_rank(p_xstream);

    /* Free the scheduler */
    ABTI_sched *p_cursched = p_xstream->p_main_sched;
    if (p_cursched != NULL) {
        abt_errno = ABTI_sched_discard_and_free(p_cursched);
        ABTI_CHECK_ERROR(abt_errno);
    }

    /* Free the array of sched contexts */
    ABTU_free(p_xstream->scheds);

    /* Free the context */
    abt_errno = ABTD_xstream_context_free(&p_xstream->ctx);
    ABTI_CHECK_ERROR(abt_errno);

    /* Free the spinlock */
    ABTI_spinlock_free(&p_xstream->sched_lock);

    ABTU_free(p_xstream);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

/* The main scheduler of each ES executes this routine. */
void ABTI_xstream_schedule(void *p_arg)
{
    ABTI_xstream *p_xstream = (ABTI_xstream *)p_arg;

    while (1) {
        uint32_t request;

        ABTD_atomic_store_uint32((uint32_t *)&p_xstream->state,
                                 ABT_XSTREAM_STATE_RUNNING);

        /* Execute the run function of scheduler */
        ABTI_sched *p_sched = p_xstream->p_main_sched;
        /* This function can be invoked without user-level context switches
         * (e.g., directly called on top of Pthreads), so ABTI_LOG_SET_SCHED
         * must be called manually here. */
        ABTI_LOG_SET_SCHED(p_sched);
        p_sched->state = ABT_SCHED_STATE_RUNNING;
#ifdef ABT_XSTREAM_USE_VIRTUAL
	if(p_xstream->type == ABTI_XSTREAM_TYPE_VIRTUAL)
    	    LOG_EVENT("[vS%" PRIu64 "] start\n", p_sched->id);
	else
#endif
	LOG_EVENT("[S%" PRIu64 "] start\n", p_sched->id);
        p_sched->run(ABTI_sched_get_handle(p_sched));
#ifdef ABT_XSTREAM_USE_VIRTUAL
	if(p_xstream->type == ABTI_XSTREAM_TYPE_VIRTUAL)
	    LOG_EVENT("[vS%" PRIu64 "] end\n", p_sched->id);
	else
#endif
	LOG_EVENT("[S%" PRIu64 "] end\n", p_sched->id);
        p_sched->state = ABT_SCHED_STATE_TERMINATED;

        ABTD_atomic_store_uint32((uint32_t *)&p_xstream->state,
                                 ABT_XSTREAM_STATE_READY);
        ABTI_spinlock_release(&p_xstream->sched_lock);

#ifdef ABT_XSTREAM_USE_VIRTUAL
	//Need to join here so we can switch to another scheduler
	ABTI_xstream_set_request(p_xstream, ABTI_XSTREAM_REQ_JOIN);
#endif

        request = ABTD_atomic_load_uint32(&p_xstream->request);
#ifdef ABT_CONFIG_HANDLE_POWER_EVENT
        /* If there is a stop request, the ES has to be terminated/ */
        if (request & ABTI_XSTREAM_REQ_STOP) break;
#endif

	/* If there is an exit or a cancel request, the ES terminates
         * regardless of remaining work units. */
        if ((request & ABTI_XSTREAM_REQ_EXIT) ||
            (request & ABTI_XSTREAM_REQ_CANCEL))
            break;

        /* When join is requested, the ES terminates after finishing
         * execution of all work units. */
        if (request & ABTI_XSTREAM_REQ_JOIN) {
            if (ABTI_sched_get_effective_size(p_xstream->p_main_sched) == 0) {
                /* If a ULT has been blocked on the join call, we make it ready */
                if (p_xstream->p_req_arg) {
                    ABTI_thread_set_ready((ABTI_thread *)p_xstream->p_req_arg);
                    p_xstream->p_req_arg = NULL;
                }
                break;
            }
        }

    }

    /* Set the ES's state as TERMINATED */
    ABTD_atomic_store_uint32((uint32_t *)&p_xstream->state,
                             ABT_XSTREAM_STATE_TERMINATED);
    LOG_EVENT("[E%d] terminated\n", p_xstream->rank);
}

int ABTI_xstream_schedule_thread(ABTI_xstream *p_xstream, ABTI_thread *p_thread)
{
    int abt_errno = ABT_SUCCESS;

#ifndef ABT_CONFIG_DISABLE_THREAD_CANCEL
    if (p_thread->request & ABTI_THREAD_REQ_CANCEL) {
        LOG_EVENT("[U%" PRIu64 ":E%d] canceled\n",
                  ABTI_thread_get_id(p_thread), p_xstream->rank);
        ABTD_thread_cancel(p_thread);
        ABTI_xstream_terminate_thread(p_thread);
        goto fn_exit;
    }
#endif

#ifndef ABT_CONFIG_DISABLE_MIGRATION
    if (p_thread->request & ABTI_THREAD_REQ_MIGRATE) {
        abt_errno = ABTI_xstream_migrate_thread(p_thread);
        ABTI_CHECK_ERROR(abt_errno);
        goto fn_exit;
    }
#endif

#ifndef ABT_CONFIG_DISABLE_STACKABLE_SCHED
    /* Add the new scheduler if the ULT is a scheduler */
    if (p_thread->is_sched != NULL) {
        p_thread->is_sched->p_ctx = &p_thread->ctx;
        ABTI_xstream_push_sched(p_xstream, p_thread->is_sched);
        p_thread->is_sched->state = ABT_SCHED_STATE_RUNNING;
    }
#endif

#ifdef ABT_XSTREAM_USE_VIRTUAL
    if (p_thread->is_sched != NULL)
    {
	if (p_thread->is_sched->used == ABTI_SCHED_NOT_USED)
	    return abt_errno;
    }

#endif
    /* Change the last ES */
    p_thread->p_last_xstream = p_xstream;

    /* Change the ULT state */
    p_thread->state = ABT_THREAD_STATE_RUNNING;

    /* Switch the context */
    LOG_EVENT("[U%" PRIu64 ":E%d] start running\n",
              ABTI_thread_get_id(p_thread), p_xstream->rank);
    ABTI_sched *p_sched = ABTI_xstream_get_top_sched(p_xstream);
#ifndef ABT_CONFIG_DISABLE_STACKABLE_SCHED
    if (p_thread->is_sched != NULL) {
        ABTI_thread_context_switch_sched_to_sched(p_sched, p_thread->is_sched);
        /* The scheduler continues from here. */
        /* Because of the stackable scheduler concept, the previous ULT must
         * be the same as one to which the context has been switched. */
    } else {
#endif
        ABTI_thread_context_switch_sched_to_thread(p_sched, p_thread);
        /* The scheduler continues from here. */
        /* The previous ULT may not be the same as one to which the
         * context has been switched. */
        p_thread = ABTI_local_get_thread();
#ifndef ABT_CONFIG_DISABLE_STACKABLE_SCHED
    }
#endif

    p_xstream = p_thread->p_last_xstream;
    LOG_EVENT("[U%" PRIu64 ":E%d] stopped\n",
              ABTI_thread_get_id(p_thread), p_xstream->rank);

#ifndef ABT_CONFIG_DISABLE_STACKABLE_SCHED
    /* Delete the last scheduler if the ULT was a scheduler */
    if (p_thread->is_sched != NULL) {
        ABTI_xstream_pop_sched(p_xstream);
        /* If a migration is trying to read the state of the scheduler, we need
         * to let it finish before freeing the scheduler */
        p_thread->is_sched->state = ABT_SCHED_STATE_STOPPED;
        ABTI_spinlock_release(&p_xstream->sched_lock);
    }
#endif

    /* Request handling. */
    /* We do not need to acquire-load request since all critical requests
     * (BLOCK, ORPHAN, STOP, and NOPUSH) are written by p_thread. CANCEL might
     * be delayed. */
    if (p_thread->request & ABTI_THREAD_REQ_STOP) {
        /* The ULT has completed its execution or it called the exit request. */
        LOG_EVENT("[U%" PRIu64 ":E%d] %s\n",
                  ABTI_thread_get_id(p_thread), p_xstream->rank,
                  (p_thread->request & ABTI_THREAD_REQ_TERMINATE ? "finished" :
                  ((p_thread->request & ABTI_THREAD_REQ_EXIT) ? "exit called" :
                  "UNKNOWN")));
        ABTI_xstream_terminate_thread(p_thread);
#ifndef ABT_CONFIG_DISABLE_THREAD_CANCEL
    } else if (p_thread->request & ABTI_THREAD_REQ_CANCEL) {
        LOG_EVENT("[U%" PRIu64 ":E%d] canceled\n",
                  ABTI_thread_get_id(p_thread), p_xstream->rank);
        ABTD_thread_cancel(p_thread);
        ABTI_xstream_terminate_thread(p_thread);
#endif
    } else if (!(p_thread->request & ABTI_THREAD_REQ_NON_YIELD)) {
        /* The ULT did not finish its execution.
         * Change the state of current running ULT and
         * add it to the pool again. */
        ABTI_POOL_ADD_THREAD(p_thread, p_xstream);
    } else if (p_thread->request & ABTI_THREAD_REQ_BLOCK) {
        LOG_EVENT("[U%" PRIu64 ":E%d] check blocked\n",
                  ABTI_thread_get_id(p_thread), p_xstream->rank);
        ABTI_thread_unset_request(p_thread, ABTI_THREAD_REQ_BLOCK);
#ifndef ABT_CONFIG_DISABLE_MIGRATION
    } else if (p_thread->request & ABTI_THREAD_REQ_MIGRATE) {
        /* This is the case when the ULT requests migration of itself. */
        abt_errno = ABTI_xstream_migrate_thread(p_thread);
        ABTI_CHECK_ERROR(abt_errno);
#endif
    } else if (p_thread->request & ABTI_THREAD_REQ_ORPHAN) {
        /* The ULT is not pushed back to the pool and is disconnected from any
         * pool. */
        LOG_EVENT("[U%" PRIu64 ":E%d] orphaned\n",
                  ABTI_thread_get_id(p_thread), p_xstream->rank);
        ABTI_thread_unset_request(p_thread, ABTI_THREAD_REQ_ORPHAN);
        p_thread->p_pool->u_free(&p_thread->unit);
        p_thread->p_pool = NULL;
    } else if (p_thread->request & ABTI_THREAD_REQ_NOPUSH) {
        /* The ULT is not pushed back to the pool */
        LOG_EVENT("[U%" PRIu64 ":E%d] not pushed\n",
                  ABTI_thread_get_id(p_thread), p_xstream->rank);
        ABTI_thread_unset_request(p_thread, ABTI_THREAD_REQ_NOPUSH);
    } else {
        abt_errno = ABT_ERR_THREAD;
        goto fn_fail;
    }

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

void ABTI_xstream_schedule_task(ABTI_xstream *p_xstream, ABTI_task *p_task)
{
#ifndef ABT_CONFIG_DISABLE_TASK_CANCEL
    if (p_task->request & ABTI_TASK_REQ_CANCEL) {
        ABTI_xstream_terminate_task(p_task);
        return;
    }
#endif

    /* Set the current running tasklet */
    ABTI_local_set_task(p_task);
    ABTI_local_set_thread(NULL);

    /* Change the task state */
    p_task->state = ABT_TASK_STATE_RUNNING;

    /* Set the associated ES */
    p_task->p_xstream = p_xstream;

#ifdef ABT_CONFIG_DISABLE_STACKABLE_SCHED
    /* Execute the task function */
    LOG_EVENT("[T%" PRIu64 ":E%d] running\n",
              ABTI_task_get_id(p_task), p_xstream->rank);
    ABTI_LOG_SET_SCHED(NULL);
    p_task->f_task(p_task->p_arg);
#else
    /* Add a new scheduler if the task is a scheduler */
    if (p_task->is_sched != NULL) {
        ABTI_sched *current_sched = ABTI_xstream_get_top_sched(p_xstream);
        ABTI_thread *p_last_thread = current_sched->p_thread;

        p_task->is_sched->p_ctx = current_sched->p_ctx;
        ABTI_xstream_push_sched(p_xstream, p_task->is_sched);
        p_task->is_sched->state = ABT_SCHED_STATE_RUNNING;
        p_task->is_sched->p_thread = p_last_thread;
        LOG_EVENT("[S%" PRIu64 ":E%d] stacked sched start\n",
                  p_task->is_sched->id, p_xstream->rank);
    }

    /* Execute the task function */
    LOG_EVENT("[T%" PRIu64 ":E%d] running\n",
              ABTI_task_get_id(p_task), p_xstream->rank);
    ABTI_LOG_SET_SCHED(p_task->is_sched ? p_task->is_sched : NULL);

    p_task->f_task(p_task->p_arg);

    /* Delete the last scheduler if the tasklet was a scheduler */
    if (p_task->is_sched != NULL) {
        ABTI_xstream_pop_sched(p_xstream);
        /* If a migration is trying to read the state of the scheduler, we need
         * to let it finish before freeing the scheduler */
        ABTI_spinlock_release(&p_xstream->sched_lock);
        ABTI_LOG_SET_SCHED(ABTI_xstream_get_top_sched(p_xstream));
        LOG_EVENT("[S%" PRIu64 ":E%d] stacked sched end\n",
                  p_task->is_sched->id, p_xstream->rank);
    }
#endif

    ABTI_LOG_SET_SCHED(ABTI_xstream_get_top_sched(p_xstream));
    LOG_EVENT("[T%" PRIu64 ":E%d] stopped\n",
              ABTI_task_get_id(p_task), p_xstream->rank);

    /* Terminate the tasklet */
    ABTI_xstream_terminate_task(p_task);
}

int ABTI_xstream_migrate_thread(ABTI_thread *p_thread)
{
#ifdef ABT_CONFIG_DISABLE_MIGRATION
    return ABT_ERR_MIGRATION_NA;
#else
    int abt_errno = ABT_SUCCESS;
    ABTI_pool *p_pool;
    ABT_pool pool;
    ABTI_xstream *newstream = NULL;

    /* callback function */
    if (p_thread->attr.f_cb) {
        ABT_thread thread = ABTI_thread_get_handle(p_thread);
        p_thread->attr.f_cb(thread, p_thread->attr.p_cb_arg);
    }

    ABTI_spinlock_acquire(&p_thread->lock); // TODO: mutex useful?
    {
        /* extracting argument in migration request */
        p_pool = (ABTI_pool *)ABTI_thread_extract_req_arg(p_thread,
                ABTI_THREAD_REQ_MIGRATE);
        pool = ABTI_pool_get_handle(p_pool);
        ABTI_thread_unset_request(p_thread, ABTI_THREAD_REQ_MIGRATE);

#ifndef ABT_CONFIG_DISABLE_POOL_CONSUMER_CHECK
        newstream = p_pool->consumer;
#endif
        LOG_EVENT("[U%" PRIu64 "] migration: E%d -> E%d\n",
                ABTI_thread_get_id(p_thread), p_thread->p_last_xstream->rank,
                newstream ? newstream->rank : -1);

        /* Change the associated pool */
        p_thread->p_pool = p_pool;

        /* Add the unit to the scheduler's pool */
        abt_errno = ABT_pool_push(pool, p_thread->unit);
    }
    ABTI_spinlock_release(&p_thread->lock);

    ABTI_pool_dec_num_migrations(p_pool);

    /* Check the push */
    ABTI_CHECK_ERROR(abt_errno);

    /* checking the state destination xstream */
    if (newstream && newstream->state == ABT_XSTREAM_STATE_CREATED) {
        abt_errno = ABTI_xstream_start(newstream);
        ABTI_CHECK_ERROR_MSG(abt_errno, "ABTI_xstream_start");
    }

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
#endif
}

int ABTI_xstream_set_main_sched(ABTI_xstream *p_xstream, ABTI_sched *p_sched)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_thread *p_thread = NULL;
    ABTI_sched *p_main_sched;
    ABTI_pool *p_tar_pool = NULL;
    int p;

    if (p_xstream->p_main_sched) {
	if (ABTI_sched_get_effective_size(p_xstream->p_main_sched) > 0) {
		abt_errno = ABT_ERR_XSTREAM;
		return abt_errno;
	}
    }

#ifndef ABT_CONFIG_DISABLE_POOL_CONSUMER_CHECK
    /* We check that from the pool set of the scheduler we do not find a pool
     * with another associated pool, and set the right value if it is okay  */
    for (p = 0; p < p_sched->num_pools; p++) {
        ABTI_pool *p_pool = ABTI_pool_get_ptr(p_sched->pools[p]);
        abt_errno = ABTI_pool_set_consumer(p_pool, p_xstream);
        ABTI_CHECK_ERROR(abt_errno);
    }
#endif

    /* The main scheduler will to be a ULT, not a tasklet */
    p_sched->type = ABT_SCHED_TYPE_ULT;

    /* Set the scheduler as a main scheduler */
    p_sched->used = ABTI_SCHED_MAIN;

    p_main_sched = p_xstream->p_main_sched;
    if (p_main_sched == NULL) {
        /* Set the scheduler */
        p_xstream->p_main_sched = p_sched;

        goto fn_exit;
    }
    
    /* If the ES has a main scheduler, we have to free it */
    p_thread = ABTI_local_get_thread();
    ABTI_ASSERT(p_thread != NULL);

    p_tar_pool = ABTI_pool_get_ptr(p_sched->pools[0]);

    /* If the caller ULT is associated with a pool of the current main
     * scheduler, it needs to be associated to a pool of new scheduler. */
    for (p = 0; p < p_main_sched->num_pools; p++) {
        if (p_thread->p_pool == ABTI_pool_get_ptr(p_main_sched->pools[p])) {
            /* Associate the work unit to the first pool of new scheduler */
            p_thread->p_pool->u_free(&p_thread->unit);
            ABT_thread h_thread = ABTI_thread_get_handle(p_thread);
            p_thread->unit = p_tar_pool->u_create_from_thread(h_thread);
            p_thread->p_pool = p_tar_pool;
            break;
        }
    }

    if (p_xstream->type == ABTI_XSTREAM_TYPE_PRIMARY) {
	ABTI_CHECK_TRUE(p_thread->type == ABTI_THREAD_TYPE_MAIN, ABT_ERR_THREAD);

        /* Free the current main scheduler */
        abt_errno = ABTI_sched_discard_and_free(p_main_sched);
        ABTI_CHECK_ERROR(abt_errno);

        /* Since the primary ES does not finish its execution until ABT_finalize
         * is called, its main scheduler needs to be automatically freed when
         * it is freed in ABT_finalize. */
        p_sched->automatic = ABT_TRUE;

        ABTI_POOL_PUSH(p_tar_pool, p_thread->unit, p_xstream);

        /* Pop the top scheduler */
        ABTI_xstream_pop_sched(p_xstream);

        /* Set the scheduler */
        p_xstream->p_main_sched = p_sched;

        /* Start the primary ES again because we have to create a sched ULT for
         * the new scheduler */
        abt_errno = ABTI_xstream_start_primary(p_xstream, p_thread);
        ABTI_CHECK_ERROR_MSG(abt_errno, "ABTI_xstream_start");
    }
#ifdef ABT_XSTREAM_USE_VIRTUAL
    else if (p_xstream->type == ABTI_XSTREAM_TYPE_VIRTUAL) {
	
	ABTI_sched_set_request(p_main_sched, ABTI_SCHED_REQ_FINISH);	
	p_sched->p_thread = p_main_sched->p_thread;
	
	ABTI_xstream *parent_xstream = p_thread->p_last_xstream;
	ABTI_CHECK_ERROR(abt_errno);

	p_sched->p_ctx = parent_xstream->p_main_sched->p_ctx;
	p_main_sched->p_thread = NULL;

	p_xstream->p_main_sched = p_sched;

	ABTI_sched* parent_sched = parent_xstream->p_main_sched;
	if(parent_sched == ABT_SCHED_NULL)  {
	    abt_errno = ABT_ERR_INV_SCHED;
	    goto fn_exit;
	}

	abt_errno = ABTI_thread_create_main_sched_virtual(parent_xstream, p_xstream, p_sched);
	ABTI_CHECK_ERROR(abt_errno);
	p_sched->p_thread->p_last_xstream = parent_xstream;
	//Do not context switch here, let the scheduler run and pop the next scheduler.
    }
#endif
    else {
        /* Finish the current main scheduler */
        ABTI_sched_set_request(p_main_sched, ABTI_SCHED_REQ_FINISH);

        /* If the ES is secondary, we should take the associated ULT of the
         * current main scheduler and keep it in the new scheduler. */
        p_sched->p_thread = p_main_sched->p_thread;
        p_sched->p_ctx = p_main_sched->p_ctx;
        p_main_sched->p_thread = NULL;

        /* The current ULT is pushed to the new scheduler's pool so that when
         * the new scheduler starts (see below), it can be scheduled by the new
         * scheduler. When the current ULT resumes its execution, it will free
         * the current main scheduler (see below). */
        ABTI_POOL_PUSH(p_tar_pool, p_thread->unit, p_xstream);

        /* Set the scheduler */
        p_xstream->p_main_sched = p_sched;

        /* Replace the top scheduler with the new scheduler */
        ABTI_xstream_replace_top_sched(p_xstream, p_sched);

        /* Switch to the current main scheduler */
        ABTI_thread_set_request(p_thread, ABTI_THREAD_REQ_NOPUSH);
        ABTI_thread_context_switch_thread_to_sched(p_thread, p_main_sched);

        /* Now, we free the current main scheduler */
        abt_errno = ABTI_sched_discard_and_free(p_main_sched);
        ABTI_CHECK_ERROR(abt_errno);
    }

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

void ABTI_xstream_print(ABTI_xstream *p_xstream, FILE *p_os, int indent,
                        ABT_bool print_sub)
{
    char *prefix = ABTU_get_indent_str(indent);

    if (p_xstream == NULL) {
        fprintf(p_os, "%s== NULL ES ==\n", prefix);
        goto fn_exit;
    }

    char *type, *state;
    char *scheds_str;
    int i;
    size_t size, pos;

    switch (p_xstream->type) {
        case ABTI_XSTREAM_TYPE_PRIMARY:   type = "PRIMARY"; break;
        case ABTI_XSTREAM_TYPE_SECONDARY: type = "SECONDARY"; break;
        default:                          type = "UNKNOWN"; break;
    }
    switch (p_xstream->state) {
        case ABT_XSTREAM_STATE_CREATED:    state = "CREATED"; break;
        case ABT_XSTREAM_STATE_READY:      state = "READY"; break;
        case ABT_XSTREAM_STATE_RUNNING:    state = "RUNNING"; break;
        case ABT_XSTREAM_STATE_TERMINATED: state = "TERMINATED"; break;
        default:                           state = "UNKNOWN"; break;
    }

    size = sizeof(char) * (p_xstream->num_scheds * 20 + 4);
    scheds_str = (char *)ABTU_calloc(size, 1);
    scheds_str[0] = '[';
    scheds_str[1] = ' ';
    pos = 2;
    for (i = 0; i < p_xstream->num_scheds; i++) {
        sprintf(&scheds_str[pos], "%p ", p_xstream->scheds[i]);
        pos = strlen(scheds_str);
    }
    scheds_str[pos] = ']';

    fprintf(p_os,
        "%s== ES (%p) ==\n"
        "%srank      : %d\n"
        "%stype      : %s\n"
        "%sstate     : %s\n"
        "%srequest   : 0x%x\n"
        "%smax_scheds: %d\n"
        "%snum_scheds: %d\n"
        "%sscheds    : %s\n"
        "%smain_sched: %p\n",
        prefix, p_xstream,
        prefix, p_xstream->rank,
        prefix, type,
        prefix, state,
        prefix, p_xstream->request,
        prefix, p_xstream->max_scheds,
        prefix, p_xstream->num_scheds,
        prefix, scheds_str,
        prefix, p_xstream->p_main_sched
    );
    ABTU_free(scheds_str);

    if (print_sub == ABT_TRUE) {
        ABTI_sched_print(p_xstream->p_main_sched, p_os, indent + ABTI_INDENT,
                         ABT_TRUE);
    }

  fn_exit:
    fflush(p_os);
    ABTU_free(prefix);
}

void *ABTI_xstream_launch_main_sched(void *p_arg)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_xstream *p_xstream = (ABTI_xstream *)p_arg;

    /* Initialization of the local variables */
    abt_errno = ABTI_local_init();
    ABTI_CHECK_ERROR(abt_errno);
    ABTI_local_set_xstream(p_xstream);

    /* Create the main sched ULT */
    ABTI_sched *p_sched = p_xstream->p_main_sched;
    abt_errno = ABTI_thread_create_main_sched(p_xstream, p_sched);
    ABTI_CHECK_ERROR(abt_errno);
    p_sched->p_thread->p_last_xstream = p_xstream;

    /* Set the sched ULT as the current ULT */
    ABTI_local_set_thread(p_sched->p_thread);

    /* Execute the main scheduler of this ES */
    LOG_EVENT("[E%d] start\n", p_xstream->rank);
    ABTI_xstream_schedule(p_arg);
    LOG_EVENT("[E%d] end\n", p_xstream->rank);

    /* Reset the current ES and its local info. */
    ABTI_local_finalize();

    ABTD_xstream_context_exit();

  fn_exit:
    return NULL;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}


/*****************************************************************************/
/* Internal static functions                                                 */
/*****************************************************************************/

/* Set a new rank to ES */
static void ABTI_xstream_set_new_rank(ABTI_xstream *p_xstream)
{
    int i, rank;
    ABT_bool found = ABT_FALSE;

    while (found == ABT_FALSE) {
        if (gp_ABTI_global->num_xstreams >= gp_ABTI_global->max_xstreams) {
            ABTI_global_update_max_xstreams(0);
        }

        ABTI_spinlock_acquire(&gp_ABTI_global->xstreams_lock);
        for (i = 0; i < gp_ABTI_global->max_xstreams; i++) {
            if (gp_ABTI_global->p_xstreams[i] == NULL) {
                /* Add this ES to the global ES array */
                gp_ABTI_global->p_xstreams[i] = p_xstream;
                gp_ABTI_global->num_xstreams++;
                rank = i;
                found = ABT_TRUE;
                break;
            }
        }
        ABTI_spinlock_release(&gp_ABTI_global->xstreams_lock);
    }

    /* Set the rank */
    p_xstream->rank = rank;
}

static ABT_bool ABTI_xstream_take_rank(ABTI_xstream *p_xstream, int rank)
{
    ABT_bool ret;

    if (rank >= gp_ABTI_global->max_xstreams) {
        ABTI_global_update_max_xstreams(rank + 1);
    }

    ABTI_spinlock_acquire(&gp_ABTI_global->xstreams_lock);
    if (gp_ABTI_global->p_xstreams[rank] == NULL) {
        /* Add this ES to the global ES array */
        gp_ABTI_global->p_xstreams[rank] = p_xstream;
        gp_ABTI_global->num_xstreams++;
        ret = ABT_TRUE;
    } else {
        ret = ABT_FALSE;
    }
    ABTI_spinlock_release(&gp_ABTI_global->xstreams_lock);

    if (ret == ABT_TRUE) {

        /* Set the rank */
        p_xstream->rank = rank;
    }

    return ret;
}

static void ABTI_xstream_return_rank(ABTI_xstream *p_xstream)
{
    /* Remove this xstream from the global ES array */
    ABTI_spinlock_acquire(&gp_ABTI_global->xstreams_lock);
    gp_ABTI_global->p_xstreams[p_xstream->rank] = NULL;
    gp_ABTI_global->num_xstreams--;
    ABTI_spinlock_release(&gp_ABTI_global->xstreams_lock);
}

