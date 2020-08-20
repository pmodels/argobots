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
    ABTI_xstream *p_local_xstream = ABTI_local_get_xstream();
    ABTI_sched *p_sched;
    ABTI_xstream *p_newxstream;

    if (sched == ABT_SCHED_NULL) {
        abt_errno = ABTI_sched_create_basic(ABT_SCHED_DEFAULT, 0, NULL,
                                            ABT_SCHED_CONFIG_NULL, &p_sched);
        ABTI_CHECK_ERROR(abt_errno);
    } else {
        p_sched = ABTI_sched_get_ptr(sched);
        ABTI_CHECK_TRUE(p_sched->used == ABTI_SCHED_NOT_USED,
                        ABT_ERR_INV_SCHED);
    }

    abt_errno = ABTI_xstream_create(p_sched, &p_newxstream);
    ABTI_CHECK_ERROR(abt_errno);

    /* Start this ES */
    abt_errno = ABTI_xstream_start(p_local_xstream, p_newxstream);
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

    p_newxstream->type = ABTI_XSTREAM_TYPE_SECONDARY;
    ABTD_atomic_relaxed_store_int(&p_newxstream->state,
                                  ABT_XSTREAM_STATE_RUNNING);
    p_newxstream->scheds = NULL;
    p_newxstream->p_main_sched = NULL;
    ABTD_atomic_relaxed_store_uint32(&p_newxstream->request, 0);
    p_newxstream->p_req_arg = NULL;
    p_newxstream->p_unit = NULL;
    ABTI_mem_init_local(p_newxstream);

    /* Set the main scheduler */
    abt_errno = ABTI_xstream_init_main_sched(p_newxstream, p_sched);
    ABTI_CHECK_ERROR(abt_errno);

    LOG_DEBUG("[E%d] created\n", p_newxstream->rank);

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
    ABTI_sched *p_sched;

    /* For the primary ES, a default scheduler is created. */
    abt_errno = ABTI_sched_create_basic(ABT_SCHED_DEFAULT, 0, NULL,
                                        ABT_SCHED_CONFIG_NULL, &p_sched);
    ABTI_CHECK_ERROR(abt_errno);

    abt_errno = ABTI_xstream_create(p_sched, &p_newxstream);
    ABTI_CHECK_ERROR(abt_errno);

    p_newxstream->type = ABTI_XSTREAM_TYPE_PRIMARY;

    *pp_xstream = p_newxstream;

fn_exit:
    return abt_errno;

fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
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
    ABTI_xstream *p_local_xstream = ABTI_local_get_xstream();
    ABTI_xstream *p_newxstream;

    ABTI_sched *p_sched;
    abt_errno =
        ABTI_sched_create_basic(predef, num_pools, pools, config, &p_sched);
    ABTI_CHECK_ERROR(abt_errno);

    abt_errno = ABTI_xstream_create(p_sched, &p_newxstream);
    ABTI_CHECK_ERROR(abt_errno);

    /* Start this ES */
    abt_errno = ABTI_xstream_start(p_local_xstream, p_newxstream);
    ABTI_CHECK_ERROR(abt_errno);

    *newxstream = ABTI_xstream_get_handle(p_newxstream);

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
    ABTI_xstream *p_local_xstream = ABTI_local_get_xstream();
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
        abt_errno = ABTI_sched_create_basic(ABT_SCHED_DEFAULT, 0, NULL,
                                            ABT_SCHED_CONFIG_NULL, &p_sched);
        ABTI_CHECK_ERROR(abt_errno);
    } else {
        p_sched = ABTI_sched_get_ptr(sched);
        ABTI_CHECK_TRUE(p_sched->used == ABTI_SCHED_NOT_USED,
                        ABT_ERR_INV_SCHED);
    }

    p_newxstream->type = ABTI_XSTREAM_TYPE_SECONDARY;
    ABTD_atomic_relaxed_store_int(&p_newxstream->state,
                                  ABT_XSTREAM_STATE_RUNNING);
    p_newxstream->scheds = NULL;
    p_newxstream->p_main_sched = NULL;
    ABTD_atomic_relaxed_store_uint32(&p_newxstream->request, 0);
    p_newxstream->p_req_arg = NULL;
    p_newxstream->p_unit = NULL;
    ABTI_mem_init_local(p_newxstream);

    /* Set the main scheduler */
    abt_errno = ABTI_xstream_init_main_sched(p_newxstream, p_sched);
    ABTI_CHECK_ERROR(abt_errno);

    LOG_DEBUG("[E%d] created\n", p_newxstream->rank);

    /* Start this ES */
    abt_errno = ABTI_xstream_start(p_local_xstream, p_newxstream);
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

int ABTI_xstream_start(ABTI_xstream *p_local_xstream, ABTI_xstream *p_xstream)
{
    int abt_errno = ABT_SUCCESS;

    /* The ES's state must be RUNNING */
    ABTI_ASSERT(ABTD_atomic_relaxed_load_int(&p_xstream->state) ==
                ABT_XSTREAM_STATE_RUNNING);

    if (p_xstream->type == ABTI_XSTREAM_TYPE_PRIMARY) {
        LOG_DEBUG("[E%d] start\n", p_xstream->rank);

        abt_errno = ABTD_xstream_context_set_self(&p_xstream->ctx);
        ABTI_CHECK_ERROR_MSG(abt_errno, "ABTD_xstream_context_set_self");

        /* Create the main sched ULT */
        ABTI_sched *p_sched = p_xstream->p_main_sched;
        abt_errno =
            ABTI_thread_create_main_sched(p_local_xstream, p_xstream, p_sched);
        ABTI_CHECK_ERROR(abt_errno);
        p_sched->p_thread->unit_def.p_last_xstream = p_xstream;

    } else {
        /* Start the main scheduler on a different ES */
        abt_errno =
            ABTD_xstream_context_create(ABTI_xstream_launch_main_sched,
                                        (void *)p_xstream, &p_xstream->ctx);
        ABTI_CHECK_ERROR_MSG(abt_errno, "ABTD_xstream_context_create");
    }

    /* Set the CPU affinity for the ES */
    if (gp_ABTI_global->set_affinity == ABT_TRUE) {
        ABTD_affinity_cpuset_apply_default(&p_xstream->ctx, p_xstream->rank);
    }

fn_exit:
    return abt_errno;

fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

/**
 * @ingroup ES
 * @brief   Restart an ES that has been joined by \c ABT_xstream_join().
 *
 * @param[in] xstream  handle to an ES that has been joined but not freed.
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_xstream_revive(ABT_xstream xstream)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_xstream *p_xstream = ABTI_xstream_get_ptr(xstream);
    ABTI_CHECK_NULL_XSTREAM_PTR(p_xstream);

    ABTD_atomic_relaxed_store_int(&p_xstream->state, ABT_XSTREAM_STATE_RUNNING);
    ABTD_atomic_relaxed_store_uint32(&p_xstream->request, 0);
    p_xstream->p_req_arg = NULL;
    abt_errno = ABTD_xstream_context_revive(&p_xstream->ctx);
    ABTI_CHECK_ERROR(abt_errno);

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
int ABTI_xstream_start_primary(ABTI_xstream **pp_local_xstream,
                               ABTI_xstream *p_xstream, ABTI_ythread *p_thread)
{
    int abt_errno = ABT_SUCCESS;

    /* The ES's state must be running here. */
    ABTI_ASSERT(ABTD_atomic_relaxed_load_int(&p_xstream->state) ==
                ABT_XSTREAM_STATE_RUNNING);

    LOG_DEBUG("[E%d] start\n", p_xstream->rank);

    abt_errno = ABTD_xstream_context_set_self(&p_xstream->ctx);
    ABTI_CHECK_ERROR_MSG(abt_errno, "ABTD_xstream_context_set_self");

    /* Set the CPU affinity for the ES */
    if (gp_ABTI_global->set_affinity == ABT_TRUE) {
        ABTD_affinity_cpuset_apply_default(&p_xstream->ctx, p_xstream->rank);
    }

    /* Create the main sched ULT */
    ABTI_sched *p_sched = p_xstream->p_main_sched;
    abt_errno =
        ABTI_thread_create_main_sched(*pp_local_xstream, p_xstream, p_sched);
    ABTI_CHECK_ERROR(abt_errno);
    p_sched->p_thread->unit_def.p_last_xstream = p_xstream;
    p_thread->unit_def.p_parent = &p_sched->p_thread->unit_def;

    /* Start the scheduler by context switching to it */
    LOG_DEBUG("[U%" PRIu64 ":E%d] yield\n", ABTI_thread_get_id(p_thread),
              p_thread->unit_def.p_last_xstream->rank);
    ABTI_thread_context_switch_to_parent(pp_local_xstream, p_thread,
                                         ABT_SYNC_EVENT_TYPE_OTHER, NULL);

    /* Back to the main ULT */
    LOG_DEBUG("[U%" PRIu64 ":E%d] resume\n", ABTI_thread_get_id(p_thread),
              p_thread->unit_def.p_last_xstream->rank);

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
    ABTI_xstream *p_local_xstream = ABTI_local_get_xstream();
    ABT_xstream h_xstream = *xstream;

    ABTI_xstream *p_xstream = ABTI_xstream_get_ptr(h_xstream);
    if (p_xstream == NULL)
        goto fn_exit;

    /* We first need to check whether p_local_xstream is NULL because this
     * routine might be called by external threads. */
    ABTI_CHECK_TRUE_MSG(p_local_xstream == NULL || p_xstream != p_local_xstream,
                        ABT_ERR_INV_XSTREAM,
                        "The current xstream cannot be freed.");

    ABTI_CHECK_TRUE_MSG(p_xstream->type != ABTI_XSTREAM_TYPE_PRIMARY,
                        ABT_ERR_INV_XSTREAM,
                        "The primary xstream cannot be freed explicitly.");

    /* Wait until xstream terminates */
    if (ABTD_atomic_acquire_load_int(&p_xstream->state) !=
        ABT_XSTREAM_STATE_TERMINATED) {
        abt_errno = ABTI_xstream_join(&p_local_xstream, p_xstream);
        ABTI_CHECK_ERROR(abt_errno);
    }

    /* Free the xstream object */
    abt_errno = ABTI_xstream_free(p_local_xstream, p_xstream, ABT_FALSE);
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
    ABTI_xstream *p_local_xstream = ABTI_local_get_xstream();
    ABTI_xstream *p_xstream = ABTI_xstream_get_ptr(xstream);
    ABTI_CHECK_NULL_XSTREAM_PTR(p_xstream);

    abt_errno = ABTI_xstream_join(&p_local_xstream, p_xstream);
    ABTI_CHECK_ERROR(abt_errno);

fn_exit:
    return abt_errno;

fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
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
    ABTI_xstream *p_local_xstream = ABTI_local_get_xstream();

    /* In case that Argobots has not been initialized or this routine is called
     * by an external thread, e.g., pthread, return an error code instead of
     * making the call fail. */
    if (gp_ABTI_global == NULL) {
        abt_errno = ABT_ERR_UNINITIALIZED;
        goto fn_exit;
    }
    if (p_local_xstream == NULL) {
        abt_errno = ABT_ERR_INV_XSTREAM;
        goto fn_exit;
    }

    /* Set the exit request */
    ABTI_xstream_set_request(p_local_xstream, ABTI_XSTREAM_REQ_EXIT);

    /* Wait until the ES terminates */
    ABTI_unit *p_self = p_local_xstream->p_unit;
    do {
#ifndef ABT_CONFIG_DISABLE_EXT_THREAD
        if (!ABTI_unit_type_is_thread(p_self->type)) {
            ABTD_atomic_pause();
            continue;
        }
#endif
        ABTI_thread_yield(&p_local_xstream, ABTI_unit_get_thread(p_self),
                          ABT_SYNC_EVENT_TYPE_OTHER, NULL);
    } while (ABTD_atomic_acquire_load_int(&p_local_xstream->state) !=
             ABT_XSTREAM_STATE_TERMINATED);

fn_exit:
    return abt_errno;
}

/**
 * @ingroup ES
 * @brief   Request the cancellation of the target ES.
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
    ABTI_xstream *p_local_xstream = ABTI_local_get_xstream();

    /* In case that Argobots has not been initialized or this routine is called
     * by an external thread, e.g., pthread, return an error code instead of
     * making the call fail. */
    if (gp_ABTI_global == NULL) {
        abt_errno = ABT_ERR_UNINITIALIZED;
        *xstream = ABT_XSTREAM_NULL;
        goto fn_exit;
    }
    if (p_local_xstream == NULL) {
        abt_errno = ABT_ERR_INV_XSTREAM;
        *xstream = ABT_XSTREAM_NULL;
        goto fn_exit;
    }

    /* Return value */
    *xstream = ABTI_xstream_get_handle(p_local_xstream);

fn_exit:
    return abt_errno;
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
    ABTI_xstream *p_local_xstream = ABTI_local_get_xstream();

    /* In case that Argobots has not been initialized or this routine is called
     * by an external thread, e.g., pthread, return an error code instead of
     * making the call fail. */
    if (gp_ABTI_global == NULL) {
        abt_errno = ABT_ERR_UNINITIALIZED;
        goto fn_exit;
    }
    if (p_local_xstream == NULL) {
        abt_errno = ABT_ERR_INV_XSTREAM;
        goto fn_exit;
    }

    /* Return value */
    *rank = (int)p_local_xstream->rank;

fn_exit:
    return abt_errno;
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
int ABT_xstream_set_rank(ABT_xstream xstream, int rank)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_xstream *p_xstream = ABTI_xstream_get_ptr(xstream);
    ABTI_CHECK_NULL_XSTREAM_PTR(p_xstream);

    p_xstream->rank = rank;

    /* Set the CPU affinity for the ES */
    if (gp_ABTI_global->set_affinity == ABT_TRUE) {
        ABTD_affinity_cpuset_apply_default(&p_xstream->ctx, p_xstream->rank);
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
    ABTI_xstream *p_local_xstream = ABTI_local_get_xstream();
    ABTI_sched *p_sched;

    ABTI_CHECK_TRUE(p_local_xstream != NULL, ABT_ERR_INV_THREAD);

    ABTI_xstream *p_xstream = ABTI_xstream_get_ptr(xstream);
    ABTI_CHECK_NULL_XSTREAM_PTR(p_xstream);

    ABTI_unit *p_self = p_local_xstream->p_unit;
    ABTI_CHECK_TRUE(ABTI_unit_type_is_thread(p_self->type), ABT_ERR_INV_THREAD);

    /* For now, if the target ES is running, we allow to change the main
     * scheduler of the ES only when the caller is running on the same ES. */
    /* TODO: a new state representing that the scheduler is changed is needed
     * to avoid running xstreams while the scheduler is changed in this
     * function. */
    if (ABTD_atomic_acquire_load_int(&p_xstream->state) ==
        ABT_XSTREAM_STATE_RUNNING) {
        if (p_self->p_last_xstream != p_xstream) {
            abt_errno = ABT_ERR_XSTREAM_STATE;
            goto fn_fail;
        }
    }

    /* TODO: permit to change the scheduler even when having work units in pools
     */
    if (p_xstream->p_main_sched) {
        /* We only allow to change the main scheduler when the current main
         * scheduler of p_xstream has no work unit in its associated pools. */
        if (ABTI_sched_get_effective_size(p_local_xstream,
                                          p_xstream->p_main_sched) > 0) {
            abt_errno = ABT_ERR_XSTREAM;
            goto fn_fail;
        }
    }

    if (sched == ABT_SCHED_NULL) {
        abt_errno = ABTI_sched_create_basic(ABT_SCHED_DEFAULT, 0, NULL,
                                            ABT_SCHED_CONFIG_NULL, &p_sched);
        ABTI_CHECK_ERROR(abt_errno);
    } else {
        p_sched = ABTI_sched_get_ptr(sched);
        ABTI_CHECK_TRUE(p_sched->used == ABTI_SCHED_NOT_USED,
                        ABT_ERR_INV_SCHED);
    }

    abt_errno =
        ABTI_xstream_update_main_sched(&p_local_xstream, p_xstream, p_sched);
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
                                     ABT_sched_predef predef, int num_pools,
                                     ABT_pool *pools)
{
    ABTI_xstream *p_local_xstream = ABTI_local_get_xstream();
    int abt_errno = ABT_SUCCESS;

    ABTI_xstream *p_xstream = ABTI_xstream_get_ptr(xstream);
    ABTI_CHECK_NULL_XSTREAM_PTR(p_xstream);

    ABTI_sched *p_sched;
    abt_errno = ABTI_sched_create_basic(predef, num_pools, pools,
                                        ABT_SCHED_CONFIG_NULL, &p_sched);
    ABTI_CHECK_ERROR(abt_errno);

    abt_errno =
        ABTI_xstream_update_main_sched(&p_local_xstream, p_xstream, p_sched);
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
 * This function is a convenient function that retrieves the associated pools of
 * the main scheduler.
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

    ABTI_xstream *p_xstream = ABTI_xstream_get_ptr(xstream);
    ABTI_CHECK_NULL_XSTREAM_PTR(p_xstream);
    ABTI_sched *p_sched = p_xstream->p_main_sched;
    max_pools = p_sched->num_pools > max_pools ? max_pools : p_sched->num_pools;
    memcpy(pools, p_sched->pools, sizeof(ABT_pool) * max_pools);

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
    *state = (ABT_xstream_state)ABTD_atomic_acquire_load_int(&p_xstream->state);

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
    *flag =
        (p_xstream->type == ABTI_XSTREAM_TYPE_PRIMARY) ? ABT_TRUE : ABT_FALSE;

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
 * EXPERIMENTAL: this function can be called by a normal ULT, too.  The function
 * name could be changed in the future.
 *
 * @param[in] unit handle to the unit to run
 * @param[in] pool pool where unit is from
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_xstream_run_unit(ABT_unit unit, ABT_pool pool)
{
    int abt_errno;
    ABTI_xstream *p_local_xstream = ABTI_local_get_xstream();
    ABTI_pool *p_pool = ABTI_pool_get_ptr(pool);

    abt_errno = ABTI_xstream_run_unit(&p_local_xstream, unit, p_pool);
    ABTI_CHECK_ERROR(abt_errno);

fn_exit:
    return abt_errno;

fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

int ABTI_xstream_run_unit(ABTI_xstream **pp_local_xstream, ABT_unit unit,
                          ABTI_pool *p_pool)
{
    int abt_errno = ABT_SUCCESS;

    ABT_unit_type type = p_pool->u_get_type(unit);

    if (type == ABT_UNIT_TYPE_THREAD) {
        ABT_thread thread = p_pool->u_get_thread(unit);
        ABTI_ythread *p_thread = ABTI_thread_get_ptr(thread);
        /* Switch the context */
        abt_errno = ABTI_xstream_schedule_thread(pp_local_xstream, p_thread);
        ABTI_CHECK_ERROR(abt_errno);

    } else if (type == ABT_UNIT_TYPE_TASK) {
        ABT_task task = p_pool->u_get_task(unit);
        ABTI_task *p_task = ABTI_task_get_ptr(task);
        /* Execute the task */
        ABTI_xstream_schedule_task(*pp_local_xstream, p_task);

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
    ABTI_xstream *p_local_xstream = ABTI_local_get_xstream();

    /* In case that Argobots has not been initialized or this routine is called
     * by an external thread, e.g., pthread, return an error code instead of
     * making the call fail. */
    if (gp_ABTI_global == NULL) {
        abt_errno = ABT_ERR_UNINITIALIZED;
        goto fn_exit;
    }
    if (p_local_xstream == NULL) {
        abt_errno = ABT_ERR_INV_XSTREAM;
        goto fn_exit;
    }

    abt_errno = ABTI_xstream_check_events(p_local_xstream, sched);
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
    ABTI_sched *p_sched = ABTI_sched_get_ptr(sched);
    ABTI_CHECK_NULL_SCHED_PTR(p_sched);

    ABTI_info_check_print_all_thread_stacks();

    uint32_t request = ABTD_atomic_acquire_load_uint32(&p_xstream->request);
    if (request & ABTI_XSTREAM_REQ_JOIN) {
        ABTI_sched_finish(p_sched);
    }

    if ((request & ABTI_XSTREAM_REQ_EXIT) ||
        (request & ABTI_XSTREAM_REQ_CANCEL)) {
        ABTI_sched_exit(p_sched);
    }

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

    ABTD_affinity_cpuset cpuset;
    cpuset.num_cpuids = 1;
    cpuset.cpuids = &cpuid;
    abt_errno = ABTD_affinity_cpuset_apply(&p_xstream->ctx, &cpuset);
    /* Do not free cpuset since cpuids points to a user pointer. */
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

    ABTD_affinity_cpuset cpuset;
    cpuset.num_cpuids = 0;
    cpuset.cpuids = NULL;
    abt_errno = ABTD_affinity_cpuset_read(&p_xstream->ctx, &cpuset);
    ABTI_CHECK_ERROR(abt_errno);

    if (cpuset.num_cpuids != 0) {
        *cpuid = cpuset.cpuids[0];
    } else {
        abt_errno = ABT_ERR_FEATURE_NA;
    }
    ABTD_affinity_cpuset_destroy(&cpuset);
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

    ABTD_affinity_cpuset affinity;
    affinity.num_cpuids = cpuset_size;
    affinity.cpuids = cpuset;
    abt_errno = ABTD_affinity_cpuset_apply(&p_xstream->ctx, &affinity);
    /* Do not free affinity since cpuids may not be freed. */
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

    ABTD_affinity_cpuset affinity;
    abt_errno = ABTD_affinity_cpuset_read(&p_xstream->ctx, &affinity);
    ABTI_CHECK_ERROR(abt_errno);

    int i, n;
    n = affinity.num_cpuids > cpuset_size ? cpuset_size : affinity.num_cpuids;
    *num_cpus = n;
    for (i = 0; i < n; i++) {
        cpuset[i] = affinity.cpuids[i];
    }
    ABTD_affinity_cpuset_destroy(&affinity);

fn_exit:
    return abt_errno;

fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

/*****************************************************************************/
/* Private APIs                                                              */
/*****************************************************************************/

int ABTI_xstream_join(ABTI_xstream **pp_local_xstream, ABTI_xstream *p_xstream)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_xstream *p_local_xstream;
    ABTI_ythread *p_thread = NULL;
    ABT_bool is_blockable = ABT_FALSE;

    ABTI_CHECK_TRUE_MSG(p_xstream->type != ABTI_XSTREAM_TYPE_PRIMARY,
                        ABT_ERR_INV_XSTREAM,
                        "The primary ES cannot be joined.");

    /* When the associated pool of the caller ULT has multiple-writer access
     * mode, the ULT can be blocked. Otherwise, the access mode, if it is a
     * single-writer access mode, may be violated because another ES has to set
     * the blocked ULT ready. */
    p_local_xstream = *pp_local_xstream;
    if (p_local_xstream) {
        ABTI_unit *p_self = p_local_xstream->p_unit;
        if (ABTI_unit_type_is_thread(p_self->type)) {
            p_thread = ABTI_unit_get_thread(p_self);
        }
    }

    if (p_thread) {
        ABT_pool_access access = p_thread->unit_def.p_pool->access;
        if (access == ABT_POOL_ACCESS_MPSC || access == ABT_POOL_ACCESS_MPMC) {
            is_blockable = ABT_TRUE;
        }

        /* The target ES must not be the same as the caller ULT's ES if the
         * access mode of the associated pool is not MPMC. */
        if (access != ABT_POOL_ACCESS_MPMC) {
            ABTI_CHECK_TRUE_MSG(p_xstream != p_local_xstream,
                                ABT_ERR_INV_XSTREAM,
                                "The target ES should be different.");
        }
    }

    if (ABTD_atomic_acquire_load_int(&p_xstream->state) ==
        ABT_XSTREAM_STATE_TERMINATED) {
        goto fn_join;
    }

    /* Wait until the target ES terminates */
    if (is_blockable == ABT_TRUE) {
        ABTI_POOL_SET_CONSUMER(p_thread->unit_def.p_pool,
                               ABTI_self_get_native_thread_id(p_local_xstream));

        /* Save the caller ULT to set it ready when the ES is terminated */
        p_xstream->p_req_arg = (void *)p_thread;
        ABTI_thread_set_blocked(p_thread);

        /* Set the join request */
        ABTI_xstream_set_request(p_xstream, ABTI_XSTREAM_REQ_JOIN);

        /* If the caller is a ULT, it is blocked here */
        ABTI_thread_suspend(pp_local_xstream, p_thread,
                            ABT_SYNC_EVENT_TYPE_XSTREAM_JOIN,
                            (void *)p_xstream);
    } else {
        /* Set the join request */
        ABTI_xstream_set_request(p_xstream, ABTI_XSTREAM_REQ_JOIN);

        while (ABTD_atomic_acquire_load_int(&p_xstream->state) !=
               ABT_XSTREAM_STATE_TERMINATED) {
#ifndef ABT_CONFIG_DISABLE_EXT_THREAD
            if (!ABTI_unit_type_is_thread(
                    ABTI_self_get_type(p_local_xstream))) {
                ABTD_atomic_pause();
                continue;
            }
#endif
            ABTI_thread_yield(pp_local_xstream, p_thread,
                              ABT_SYNC_EVENT_TYPE_XSTREAM_JOIN,
                              (void *)p_xstream);
            p_local_xstream = *pp_local_xstream;
        }
    }

fn_join:
    /* Normal join request */
    abt_errno = ABTD_xstream_context_join(&p_xstream->ctx);
    ABTI_CHECK_ERROR_MSG(abt_errno, "ABTD_xstream_context_join");

fn_exit:
    return abt_errno;

fn_fail:
    goto fn_exit;
}

int ABTI_xstream_free(ABTI_xstream *p_local_xstream, ABTI_xstream *p_xstream,
                      ABT_bool force_free)
{
    int abt_errno = ABT_SUCCESS;

    LOG_DEBUG("[E%d] freed\n", p_xstream->rank);

    /* Clean up memory pool. */
    ABTI_mem_finalize_local(p_xstream);
    /* Return rank for reuse. rank must be returned prior to other free
     * functions so that other xstreams cannot refer to this xstream via
     * global->p_xstreams. */
    ABTI_xstream_return_rank(p_xstream);

    /* Free the scheduler */
    ABTI_sched *p_cursched = p_xstream->p_main_sched;
    if (p_cursched != NULL) {
        abt_errno = ABTI_sched_discard_and_free(p_local_xstream, p_cursched,
                                                force_free);
        ABTI_CHECK_ERROR(abt_errno);
    }

    /* Free the array of sched contexts */
    ABTU_free(p_xstream->scheds);

    /* Free the context if a given xstream is secondary. */
    if (p_xstream->type == ABTI_XSTREAM_TYPE_SECONDARY) {
        abt_errno = ABTD_xstream_context_free(&p_xstream->ctx);
        ABTI_CHECK_ERROR(abt_errno);
    }

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
    ABTI_xstream *p_local_xstream = ABTI_local_get_xstream();
    ABTI_xstream *p_xstream = (ABTI_xstream *)p_arg;

    ABTI_ASSERT(ABTD_atomic_relaxed_load_int(&p_xstream->state) ==
                ABT_XSTREAM_STATE_RUNNING);

    while (1) {
        uint32_t request;

        /* Execute the run function of scheduler */
        ABTI_sched *p_sched = p_xstream->p_main_sched;
        ABTI_ASSERT(p_local_xstream->p_unit == &p_sched->p_thread->unit_def);
        LOG_DEBUG("[S%" PRIu64 "] start\n", p_sched->id);
        p_sched->run(ABTI_sched_get_handle(p_sched));
        LOG_DEBUG("[S%" PRIu64 "] end\n", p_sched->id);

        request = ABTD_atomic_acquire_load_uint32(&p_xstream->request);

        /* If there is an exit or a cancel request, the ES terminates
         * regardless of remaining work units. */
        if ((request & ABTI_XSTREAM_REQ_EXIT) ||
            (request & ABTI_XSTREAM_REQ_CANCEL))
            break;

        /* When join is requested, the ES terminates after finishing
         * execution of all work units. */
        if (request & ABTI_XSTREAM_REQ_JOIN) {
            if (ABTI_sched_get_effective_size(p_local_xstream,
                                              p_xstream->p_main_sched) == 0) {
                /* If a ULT has been blocked on the join call, we make it ready
                 */
                if (p_xstream->p_req_arg) {
                    ABTI_thread_set_ready(p_local_xstream,
                                          (ABTI_ythread *)p_xstream->p_req_arg);
                    p_xstream->p_req_arg = NULL;
                }
                break;
            }
        }
    }

    ABTI_ASSERT(p_local_xstream->p_unit ==
                &p_xstream->p_main_sched->p_thread->unit_def);

    /* Set the ES's state as TERMINATED */
    ABTD_atomic_release_store_int(&p_xstream->state,
                                  ABT_XSTREAM_STATE_TERMINATED);
    LOG_DEBUG("[E%d] terminated\n", p_xstream->rank);

    if (p_xstream->type == ABTI_XSTREAM_TYPE_PRIMARY) {
        /* Let us jump back to the main thread. */
        ABTI_thread_finish_context_sched_to_main_thread(
            p_xstream->p_main_sched);
    }
}

int ABTI_xstream_schedule_thread(ABTI_xstream **pp_local_xstream,
                                 ABTI_ythread *p_thread)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_xstream *p_local_xstream = *pp_local_xstream;

#ifndef ABT_CONFIG_DISABLE_THREAD_CANCEL
    if (ABTD_atomic_acquire_load_uint32(&p_thread->unit_def.request) &
        ABTI_UNIT_REQ_CANCEL) {
        LOG_DEBUG("[U%" PRIu64 ":E%d] canceled\n", ABTI_thread_get_id(p_thread),
                  p_local_xstream->rank);
        ABTD_thread_cancel(p_local_xstream, p_thread);
        ABTI_xstream_terminate_thread(p_local_xstream, p_thread);
        goto fn_exit;
    }
#endif

#ifndef ABT_CONFIG_DISABLE_MIGRATION
    if (ABTD_atomic_acquire_load_uint32(&p_thread->unit_def.request) &
        ABTI_UNIT_REQ_MIGRATE) {
        abt_errno = ABTI_xstream_migrate_thread(p_local_xstream, p_thread);
        ABTI_CHECK_ERROR(abt_errno);
        goto fn_exit;
    }
#endif

    /* Change the last ES */
    p_thread->unit_def.p_last_xstream = p_local_xstream;

    /* Change the ULT state */
    ABTD_atomic_release_store_int(&p_thread->unit_def.state,
                                  ABTI_UNIT_STATE_RUNNING);

    /* Switch the context */
    LOG_DEBUG("[U%" PRIu64 ":E%d] start running\n",
              ABTI_thread_get_id(p_thread), p_local_xstream->rank);

    ABTI_ythread *p_self = ABTI_unit_get_thread(p_local_xstream->p_unit);
    p_thread =
        ABTI_thread_context_switch_to_child(pp_local_xstream, p_self, p_thread);
    /* The previous ULT (p_thread) may not be the same as one to which the
     * context has been switched. */
    /* The scheduler continues from here. */
    p_local_xstream = *pp_local_xstream;

    LOG_DEBUG("[U%" PRIu64 ":E%d] stopped\n", ABTI_thread_get_id(p_thread),
              p_local_xstream->rank);

    /* Request handling. */
    /* We do not need to acquire-load request since all critical requests
     * (BLOCK, ORPHAN, STOP, and NOPUSH) are written by p_thread. CANCEL might
     * be delayed. */
    uint32_t request =
        ABTD_atomic_acquire_load_uint32(&p_thread->unit_def.request);
    if (request & ABTI_UNIT_REQ_STOP) {
        /* The ULT has completed its execution or it called the exit request. */
        LOG_DEBUG("[U%" PRIu64 ":E%d] %s\n", ABTI_thread_get_id(p_thread),
                  p_local_xstream->rank,
                  (request & ABTI_UNIT_REQ_TERMINATE
                       ? "finished"
                       : ((request & ABTI_UNIT_REQ_EXIT) ? "exit called"
                                                         : "UNKNOWN")));
        ABTI_xstream_terminate_thread(p_local_xstream, p_thread);
#ifndef ABT_CONFIG_DISABLE_THREAD_CANCEL
    } else if (request & ABTI_UNIT_REQ_CANCEL) {
        LOG_DEBUG("[U%" PRIu64 ":E%d] canceled\n", ABTI_thread_get_id(p_thread),
                  p_local_xstream->rank);
        ABTD_thread_cancel(p_local_xstream, p_thread);
        ABTI_xstream_terminate_thread(p_local_xstream, p_thread);
#endif
    } else if (!(request & ABTI_UNIT_REQ_NON_YIELD)) {
        /* The ULT did not finish its execution.
         * Change the state of current running ULT and
         * add it to the pool again. */
        ABTI_POOL_ADD_THREAD(p_thread,
                             ABTI_self_get_native_thread_id(p_local_xstream));
    } else if (request & ABTI_UNIT_REQ_BLOCK) {
        LOG_DEBUG("[U%" PRIu64 ":E%d] check blocked\n",
                  ABTI_thread_get_id(p_thread), p_local_xstream->rank);
        ABTI_thread_unset_request(p_thread, ABTI_UNIT_REQ_BLOCK);
#ifndef ABT_CONFIG_DISABLE_MIGRATION
    } else if (request & ABTI_UNIT_REQ_MIGRATE) {
        /* This is the case when the ULT requests migration of itself. */
        abt_errno = ABTI_xstream_migrate_thread(p_local_xstream, p_thread);
        ABTI_CHECK_ERROR(abt_errno);
#endif
    } else if (request & ABTI_UNIT_REQ_ORPHAN) {
        /* The ULT is not pushed back to the pool and is disconnected from any
         * pool. */
        LOG_DEBUG("[U%" PRIu64 ":E%d] orphaned\n", ABTI_thread_get_id(p_thread),
                  p_local_xstream->rank);
        ABTI_thread_unset_request(p_thread, ABTI_UNIT_REQ_ORPHAN);
        p_thread->unit_def.p_pool->u_free(&p_thread->unit_def.unit);
        p_thread->unit_def.p_pool = NULL;
    } else if (request & ABTI_UNIT_REQ_NOPUSH) {
        /* The ULT is not pushed back to the pool */
        LOG_DEBUG("[U%" PRIu64 ":E%d] not pushed\n",
                  ABTI_thread_get_id(p_thread), p_local_xstream->rank);
        ABTI_thread_unset_request(p_thread, ABTI_UNIT_REQ_NOPUSH);
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

void ABTI_xstream_schedule_task(ABTI_xstream *p_local_xstream,
                                ABTI_task *p_task)
{
#ifndef ABT_CONFIG_DISABLE_TASK_CANCEL
    if (ABTD_atomic_acquire_load_uint32(&p_task->request) &
        ABTI_UNIT_REQ_CANCEL) {
        ABTI_tool_event_task_cancel(p_local_xstream, p_task);
        ABTI_xstream_terminate_task(p_local_xstream, p_task);
        return;
    }
#endif

    /* Change the task state */
    ABTD_atomic_release_store_int(&p_task->state, ABTI_UNIT_STATE_RUNNING);

    /* Set the associated ES */
    p_task->p_last_xstream = p_local_xstream;

    /* Execute the task function */
    LOG_DEBUG("[T%" PRIu64 ":E%d] running\n", ABTI_task_get_id(p_task),
              p_local_xstream->rank);

    ABTI_unit *p_sched_unit = p_local_xstream->p_unit;
    p_local_xstream->p_unit = p_task;
    p_task->p_parent = p_sched_unit;

    /* Execute the task function */
    ABTI_tool_event_task_run(p_local_xstream, p_task, p_sched_unit);
    LOG_DEBUG("[T%" PRIu64 ":E%d] running\n", ABTI_task_get_id(p_task),
              p_local_xstream->rank);
    p_task->f_unit(p_task->p_arg);
    ABTI_tool_event_task_finish(p_local_xstream, p_task, p_sched_unit);
    LOG_DEBUG("[T%" PRIu64 ":E%d] stopped\n", ABTI_task_get_id(p_task),
              p_local_xstream->rank);

    /* Set the current running scheduler's unit */
    p_local_xstream->p_unit = p_sched_unit;

    /* Terminate the tasklet */
    ABTI_xstream_terminate_task(p_local_xstream, p_task);
}

int ABTI_xstream_migrate_thread(ABTI_xstream *p_local_xstream,
                                ABTI_ythread *p_thread)
{
#ifdef ABT_CONFIG_DISABLE_MIGRATION
    return ABT_ERR_MIGRATION_NA;
#else
    int abt_errno = ABT_SUCCESS;
    ABTI_pool *p_pool;

    ABTI_thread_mig_data *p_mig_data =
        ABTI_thread_get_mig_data(p_local_xstream, p_thread);
    /* callback function */
    if (p_mig_data->f_migration_cb) {
        ABT_thread thread = ABTI_thread_get_handle(p_thread);
        p_mig_data->f_migration_cb(thread, p_mig_data->p_migration_cb_arg);
    }

    /* If request is set, p_migration_pool has a valid pool pointer. */
    ABTI_ASSERT(ABTD_atomic_acquire_load_uint32(&p_thread->unit_def.request) &
                ABTI_UNIT_REQ_MIGRATE);

    /* Extracting argument in migration request. */
    p_pool = ABTD_atomic_relaxed_load_ptr(&p_mig_data->p_migration_pool);
    ABTI_thread_unset_request(p_thread, ABTI_UNIT_REQ_MIGRATE);

    LOG_DEBUG("[U%" PRIu64 "] migration: E%d -> NT %p\n",
              ABTI_thread_get_id(p_thread),
              p_thread->unit_def.p_last_xstream->rank,
              (void *)p_pool->consumer_id);

    /* Change the associated pool */
    p_thread->unit_def.p_pool = p_pool;

    /* Add the unit to the scheduler's pool */
    ABTI_POOL_PUSH(p_pool, p_thread->unit_def.unit,
                   ABTI_self_get_native_thread_id(p_local_xstream));

    ABTI_pool_dec_num_migrations(p_pool);

    /* Check the push */
    ABTI_CHECK_ERROR(abt_errno);

fn_exit:
    return abt_errno;

fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
#endif
}

int ABTI_xstream_init_main_sched(ABTI_xstream *p_xstream, ABTI_sched *p_sched)
{
    int abt_errno = ABT_SUCCESS;

    ABTI_ASSERT(p_xstream->p_main_sched == NULL);

#ifndef ABT_CONFIG_DISABLE_POOL_CONSUMER_CHECK
    int p;
    /* We check that from the pool set of the scheduler we do not find a pool
     * with another associated pool, and set the right value if it is okay  */
    ABTI_native_thread_id consumer_id =
        ABTI_xstream_get_native_thread_id(p_xstream);
    for (p = 0; p < p_sched->num_pools; p++) {
        ABTI_pool *p_pool = ABTI_pool_get_ptr(p_sched->pools[p]);
        abt_errno = ABTI_pool_set_consumer(p_pool, consumer_id);
        ABTI_CHECK_ERROR(abt_errno);
    }
#endif

    /* The main scheduler will to be a ULT, not a tasklet */
    p_sched->type = ABT_SCHED_TYPE_ULT;

    /* Set the scheduler as a main scheduler */
    p_sched->used = ABTI_SCHED_MAIN;

    /* Set the scheduler */
    p_xstream->p_main_sched = p_sched;

#ifndef ABT_CONFIG_DISABLE_POOL_CONSUMER_CHECK
fn_exit:
    return abt_errno;

fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
#else
    return abt_errno;
#endif
}

int ABTI_xstream_update_main_sched(ABTI_xstream **pp_local_xstream,
                                   ABTI_xstream *p_xstream, ABTI_sched *p_sched)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_ythread *p_thread = NULL;
    ABTI_sched *p_main_sched;
    ABTI_pool *p_tar_pool = NULL;
    int p;

#ifndef ABT_CONFIG_DISABLE_POOL_CONSUMER_CHECK
    /* We check that from the pool set of the scheduler we do not find a pool
     * with another associated pool, and set the right value if it is okay  */
    ABTI_native_thread_id consumer_id =
        ABTI_xstream_get_native_thread_id(p_xstream);
    for (p = 0; p < p_sched->num_pools; p++) {
        ABTI_pool *p_pool = ABTI_pool_get_ptr(p_sched->pools[p]);
        abt_errno = ABTI_pool_set_consumer(p_pool, consumer_id);
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
    p_thread = ABTI_unit_get_thread((*pp_local_xstream)->p_unit);
    p_tar_pool = ABTI_pool_get_ptr(p_sched->pools[0]);

    /* If the caller ULT is associated with a pool of the current main
     * scheduler, it needs to be associated to a pool of new scheduler. */
    for (p = 0; p < p_main_sched->num_pools; p++) {
        if (p_thread->unit_def.p_pool ==
            ABTI_pool_get_ptr(p_main_sched->pools[p])) {
            /* Associate the work unit to the first pool of new scheduler */
            p_thread->unit_def.p_pool->u_free(&p_thread->unit_def.unit);
            ABT_thread h_thread = ABTI_thread_get_handle(p_thread);
            p_thread->unit_def.unit =
                p_tar_pool->u_create_from_thread(h_thread);
            p_thread->unit_def.p_pool = p_tar_pool;
            break;
        }
    }

    if (p_xstream->type == ABTI_XSTREAM_TYPE_PRIMARY) {
        ABTI_CHECK_TRUE(ABTI_unit_type_is_thread_main(p_thread->unit_def.type),
                        ABT_ERR_THREAD);

        /* Since the primary ES does not finish its execution until ABT_finalize
         * is called, its main scheduler needs to be automatically freed when
         * it is freed in ABT_finalize. */
        p_sched->automatic = ABT_TRUE;

        ABTI_POOL_PUSH(p_tar_pool, p_thread->unit_def.unit,
                       ABTI_self_get_native_thread_id(*pp_local_xstream));

        /* Replace the top scheduler with the new scheduler */
        p_xstream->p_main_sched = p_sched;

        /* Free the current main scheduler */
        abt_errno = ABTI_sched_discard_and_free(*pp_local_xstream, p_main_sched,
                                                ABT_FALSE);
        ABTI_CHECK_ERROR(abt_errno);

        /* Start the primary ES again because we have to create a sched ULT for
         * the new scheduler */
        abt_errno =
            ABTI_xstream_start_primary(pp_local_xstream, p_xstream, p_thread);
        ABTI_CHECK_ERROR_MSG(abt_errno, "ABTI_xstream_start");
    } else {
        /* Finish the current main scheduler */
        ABTI_sched_set_request(p_main_sched, ABTI_SCHED_REQ_FINISH);

        /* If the ES is secondary, we should take the associated ULT of the
         * current main scheduler and keep it in the new scheduler. */
        p_sched->p_thread = p_main_sched->p_thread;
        /* The current ULT is pushed to the new scheduler's pool so that when
         * the new scheduler starts (see below), it can be scheduled by the new
         * scheduler. When the current ULT resumes its execution, it will free
         * the current main scheduler (see below). */
        ABTI_POOL_PUSH(p_tar_pool, p_thread->unit_def.unit,
                       ABTI_self_get_native_thread_id(*pp_local_xstream));

        /* Set the scheduler */
        p_xstream->p_main_sched = p_sched;

        /* Switch to the current main scheduler */
        ABTI_thread_set_request(p_thread, ABTI_UNIT_REQ_NOPUSH);
        ABTI_thread_context_switch_to_parent(pp_local_xstream, p_thread,
                                             ABT_SYNC_EVENT_TYPE_OTHER, NULL);

        /* Now, we free the current main scheduler. p_main_sched->p_thread must
         * be NULL to avoid freeing it in ABTI_sched_discard_and_free(). */
        p_main_sched->p_thread = NULL;
        abt_errno = ABTI_sched_discard_and_free(*pp_local_xstream, p_main_sched,
                                                ABT_FALSE);
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

    switch (p_xstream->type) {
        case ABTI_XSTREAM_TYPE_PRIMARY:
            type = "PRIMARY";
            break;
        case ABTI_XSTREAM_TYPE_SECONDARY:
            type = "SECONDARY";
            break;
        default:
            type = "UNKNOWN";
            break;
    }
    switch (ABTD_atomic_acquire_load_int(&p_xstream->state)) {
        case ABT_XSTREAM_STATE_RUNNING:
            state = "RUNNING";
            break;
        case ABT_XSTREAM_STATE_TERMINATED:
            state = "TERMINATED";
            break;
        default:
            state = "UNKNOWN";
            break;
    }

    fprintf(p_os,
            "%s== ES (%p) ==\n"
            "%srank      : %d\n"
            "%stype      : %s\n"
            "%sstate     : %s\n"
            "%srequest   : 0x%x\n"
            "%smain_sched: %p\n",
            prefix, (void *)p_xstream, prefix, p_xstream->rank, prefix, type,
            prefix, state, prefix,
            ABTD_atomic_acquire_load_uint32(&p_xstream->request), prefix,
            (void *)p_xstream->p_main_sched);

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
    ABTI_xstream *p_local_xstream = (ABTI_xstream *)p_arg;

    /* Initialization of the local variables */
    ABTI_local_set_xstream(p_local_xstream);

    /* Create the main sched ULT if not created yet */
    ABTI_sched *p_sched = p_local_xstream->p_main_sched;
    if (!p_sched->p_thread) {
        abt_errno = ABTI_thread_create_main_sched(p_local_xstream,
                                                  p_local_xstream, p_sched);
        ABTI_CHECK_ERROR(abt_errno);
        p_sched->p_thread->unit_def.p_last_xstream = p_local_xstream;
    } else {
        ABTI_tool_event_thread_create(p_local_xstream, p_sched->p_thread, NULL,
                                      NULL);
    }

    /* Set the sched ULT as the current ULT */
    p_local_xstream->p_unit = &p_sched->p_thread->unit_def;

    /* Execute the main scheduler of this ES */
    LOG_DEBUG("[E%d] start\n", p_local_xstream->rank);
    ABTI_tool_event_thread_run(p_local_xstream, p_sched->p_thread, NULL, NULL);
    ABTI_xstream_schedule(p_arg);
    ABTI_tool_event_thread_finish(p_local_xstream,
                                  p_local_xstream->p_main_sched->p_thread,
                                  NULL);
    ABTI_tool_event_thread_free(p_local_xstream,
                                p_local_xstream->p_main_sched->p_thread, NULL);
    LOG_DEBUG("[E%d] end\n", p_local_xstream->rank);

    /* Reset the current ES and its local info. */
    ABTI_local_set_xstream(NULL);

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
