/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"

static uint64_t ABTI_xstream_get_new_rank(void);
static void *ABTI_xstream_loop(void *p_arg);


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

    p_newxstream = (ABTI_xstream *)ABTU_malloc(sizeof(ABTI_xstream));
    if (!p_newxstream) {
        HANDLE_ERROR("ABTU_malloc");
        *newxstream = ABT_XSTREAM_NULL;
        abt_errno = ABT_ERR_MEM;
        goto fn_fail;
    }

    /* Create a wrapper unit */
    h_newxstream = ABTI_xstream_get_handle(p_newxstream);
    p_newxstream->unit = ABTI_unit_create_from_xstream(h_newxstream);

    p_newxstream->rank    = ABTI_xstream_get_new_rank();
    p_newxstream->p_name  = NULL;
    p_newxstream->type    = ABTI_XSTREAM_TYPE_SECONDARY;
    p_newxstream->state   = ABT_XSTREAM_STATE_CREATED;
    p_newxstream->request = 0;

    /* Create a mutex */
    abt_errno = ABT_mutex_create(&p_newxstream->mutex);
    ABTI_CHECK_ERROR(abt_errno);

    /* Set the scheduler */
    if (sched == ABT_SCHED_NULL) {
        /* Default scheduler */
        abt_errno = ABTI_sched_create_default(&p_newxstream->p_sched);
        if (abt_errno != ABT_SUCCESS) {
            HANDLE_ERROR("ABTI_sched_create_default");
            ABTU_free(p_newxstream);
            *newxstream = ABT_XSTREAM_NULL;
            goto fn_fail;
        }
    } else {
        p_newxstream->p_sched = ABTI_sched_get_ptr(sched);
    }

    /* Set this ES as the associated ES in the scheduler */
    p_newxstream->p_sched->p_xstream = p_newxstream;

    /* Create a work unit pool that contains terminated work units */
    abt_errno = ABTI_pool_create(&p_newxstream->deads);
    ABTI_CHECK_ERROR(abt_errno);

    /* Add this xstream to the global ES pool */
    ABTI_global_add_xstream(p_newxstream);

    /* Return value */
    *newxstream = h_newxstream;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_xstream_create", abt_errno);
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
        ABTI_mutex_waitlock(p_xstream->mutex);
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
        if (abt_errno != ABT_SUCCESS) {
            HANDLE_ERROR("ABTI_global_move_xstream");
            goto fn_fail;
        }
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
    if (abt_errno != ABT_SUCCESS) {
        HANDLE_ERROR("ABTD_xstream_context_join");
        goto fn_fail;
    }

  fn_exit:
    return abt_errno;

  fn_fail:
    goto fn_exit;
}

/**
 * @ingroup ES
 * @brief   The calling thread terminates its associated ES.
 *
 * Since the calling thread's ES terminates, this routine never returns.
 *
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_xstream_exit(void)
{
    int abt_errno = ABT_SUCCESS;

    ABTI_xstream *p_xstream = ABTI_local_get_xstream();

    /* Set the exit request */
    ABTD_atomic_fetch_or_uint32(&p_xstream->request, ABTI_XSTREAM_REQ_EXIT);

    /* Wait until the ES terminates */
    do {
        ABT_thread_yield();
    } while (p_xstream->state != ABT_XSTREAM_STATE_TERMINATED);

    return abt_errno;
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
 * @brief   Return the ES handle of the calling thread.
 *
 * @param[out] xstream  ES handle
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_xstream_self(ABT_xstream *xstream)
{
    int abt_errno = ABT_SUCCESS;

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
 * @brief   Return the rank of ES associated with the calling thread.
 *
 * @param[out] rank  ES rank
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_xstream_self_rank(int *rank)
{
    int abt_errno = ABT_SUCCESS;

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
 * @brief   Set sched as the scheduler for xstream.
 *
 * @param[in] xstream  handle to the target ES
 * @param[in] sched    handle to the scheduler used for xstream
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_xstream_set_sched(ABT_xstream xstream, ABT_sched sched)
{
    int abt_errno = ABT_SUCCESS;

    ABTI_xstream *p_xstream = ABTI_xstream_get_ptr(xstream);
    ABTI_CHECK_NULL_XSTREAM_PTR(p_xstream);

    ABTI_sched *p_sched = ABTI_sched_get_ptr(sched);
    ABTI_CHECK_NULL_SCHED_PTR(p_sched);

    /* If the type of current scheduler is DEFAULT, free it */
    ABTI_sched *p_cursched = p_xstream->p_sched;
    if (p_cursched != NULL) {
        if (p_cursched->type == ABTI_SCHED_TYPE_DEFAULT) {
            ABT_sched h_cursched = ABTI_sched_get_handle(p_cursched);
            abt_errno = ABT_sched_free(&h_cursched);
            ABTI_CHECK_ERROR(abt_errno);
        }
    }

    /* Set this ES as the associated ES in the scheduler */
    p_sched->p_xstream = p_xstream;

    /* Set the scheduler */
    p_xstream->p_sched = p_sched;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_xstream_set_sched", abt_errno);
    goto fn_exit;
}

/**
 * @ingroup ES
 * @brief   Get the scheduler for the ES.
 *
 * \c ABT_xstream_get_sched() gets the handle to the ES's scheduler through
 * \c sched.
 *
 * @param[in] xstream  handle to the target ES
 * @param[out] sched   handle to the scheduler
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_xstream_get_sched(ABT_xstream xstream, ABT_sched *sched)
{
    int abt_errno = ABT_SUCCESS;

    ABTI_xstream *p_xstream = ABTI_xstream_get_ptr(xstream);
    ABTI_CHECK_NULL_XSTREAM_PTR(p_xstream);

    /* Return value */
    *sched = ABTI_sched_get_handle(p_xstream->p_sched);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_xstream_get_sched", abt_errno);
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
 * @brief   Return the number of ULTs associated with the ES.
 *
 * \c ABT_xstream_get_num_threads() returns the number of ULTs currently
 * associated with the target ES.
 *
 * @param[in]  xstream      handle to the target ES
 * @param[out] num_threads  the number of ULTs
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_xstream_get_num_threads(ABT_xstream xstream, int *num_threads)
{
    int abt_errno = ABT_SUCCESS;

    ABTI_xstream *p_xstream = ABTI_xstream_get_ptr(xstream);
    ABTI_CHECK_NULL_XSTREAM_PTR(p_xstream);

    /* Return value */
    *num_threads = (int)p_xstream->p_sched->num_threads;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_xstream_get_num_threads", abt_errno);
    goto fn_exit;
}

/**
 * @ingroup ES
 * @brief   Return the number of tasklets associated with the ES.
 *
 * \c ABT_xstream_get_num_tasks() returns the number of tasklets currently
 * associated with the target ES.
 *
 * @param[in]  xstream    handle to the target ES
 * @param[out] num_tasks  the number of tasklets
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_xstream_get_num_tasks(ABT_xstream xstream, int *num_tasks)
{
    int abt_errno = ABT_SUCCESS;

    ABTI_xstream *p_xstream = ABTI_xstream_get_ptr(xstream);
    ABTI_CHECK_NULL_XSTREAM_PTR(p_xstream);

    /* Return value */
    *num_tasks = (int)p_xstream->p_sched->num_tasks;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_xstream_get_num_tasks", abt_errno);
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
    ABTI_mutex_waitlock(p_xstream->mutex);
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
 * If name is NULL, only len is returned.
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

    *len = strlen(p_xstream->p_name);
    if (name != NULL) {
        ABTU_strcpy(name, p_xstream->p_name);
    }

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_xstream_get_name", abt_errno);
    goto fn_exit;
}


/*****************************************************************************/
/* Private APIs                                                              */
/*****************************************************************************/

ABTI_xstream *ABTI_xstream_get_ptr(ABT_xstream xstream)
{
    ABTI_xstream *p_xstream;
    if (xstream == ABT_XSTREAM_NULL) {
        p_xstream = NULL;
    } else {
        p_xstream = (ABTI_xstream *)xstream;
    }
    return p_xstream;
}

ABT_xstream ABTI_xstream_get_handle(ABTI_xstream *p_xstream)
{
    ABT_xstream h_xstream;
    if (p_xstream == NULL) {
        h_xstream = ABT_XSTREAM_NULL;
    } else {
        h_xstream = (ABT_xstream)p_xstream;
    }
    return h_xstream;
}

int ABTI_xstream_free(ABTI_xstream *p_xstream)
{
    int abt_errno = ABT_SUCCESS;

    ABTI_unit_free(&p_xstream->unit);
    if (p_xstream->p_name) ABTU_free(p_xstream->p_name);

    /* Free the scheduler */
    if (p_xstream->p_sched) {
        ABT_sched h_sched = ABTI_sched_get_handle(p_xstream->p_sched);
        abt_errno = ABT_sched_free(&h_sched);
        ABTI_CHECK_ERROR(abt_errno);
    }

    /* Clean up the deads pool */
    ABTI_mutex_waitlock(p_xstream->mutex);
    ABTI_pool_free(&p_xstream->deads);
    ABT_mutex_unlock(p_xstream->mutex);

    /* Free the mutex */
    abt_errno = ABT_mutex_free(&p_xstream->mutex);
    ABTI_CHECK_ERROR(abt_errno);

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

int ABTI_xstream_start(ABTI_xstream *p_xstream)
{
    assert(p_xstream->state == ABT_XSTREAM_STATE_CREATED);

    /* Set the xstream's state as READY */
    ABT_xstream_state old_state;
    old_state = ABTD_atomic_cas_int32((int32_t *)&p_xstream->state,
            ABT_XSTREAM_STATE_CREATED, ABT_XSTREAM_STATE_READY);
    if (old_state != ABT_XSTREAM_STATE_CREATED) goto fn_exit;

    int abt_errno = ABT_SUCCESS;
    if (p_xstream->type == ABTI_XSTREAM_TYPE_PRIMARY) {
        abt_errno = ABTD_xstream_context_self(&p_xstream->ctx);
        if (abt_errno != ABT_SUCCESS) {
            HANDLE_ERROR("ABTD_xstream_context_self");
            goto fn_fail;
        }
    } else {
        abt_errno = ABTD_xstream_context_create(
                ABTI_xstream_loop, (void *)p_xstream, &p_xstream->ctx);
        if (abt_errno != ABT_SUCCESS) {
            HANDLE_ERROR("ABTD_xstream_context_create");
            goto fn_fail;
        }
    }

    /* Move the xstream to the global active ES pool */
    ABTI_global_move_xstream(p_xstream);

  fn_exit:
    return abt_errno;

  fn_fail:
    goto fn_exit;
}

int ABTI_xstream_start_any(void)
{
    int abt_errno = ABT_SUCCESS;

    ABTI_xstream_pool *p_xstreams = gp_ABTI_global->p_xstreams;

    /* If there exist one or more active xstreams or there is no xstream
     * created, we have nothing to do. */
    if (ABTI_pool_get_size(p_xstreams->active) > 0) goto fn_exit;
    if (ABTI_pool_get_size(p_xstreams->created) == 0) goto fn_exit;

    /* Pop one xstream from the global ES pool and start it */
    ABTI_xstream *p_xstream;
    abt_errno = ABTI_global_get_created_xstream(&p_xstream);
    ABTI_CHECK_ERROR(abt_errno);
    if (p_xstream == NULL) goto fn_exit;

    if (p_xstream->state == ABT_XSTREAM_STATE_CREATED) {
        abt_errno = ABTI_xstream_start(p_xstream);
        if (abt_errno != ABT_SUCCESS) {
            HANDLE_ERROR("ABTI_xstream_start");
            goto fn_fail;
        }
    }

  fn_exit:
    return abt_errno;

  fn_fail:
    goto fn_exit;
}

int ABTI_xstream_schedule(ABTI_xstream *p_xstream)
{
    int abt_errno = ABT_SUCCESS;

    ABTI_task_pool *p_tasks = gp_ABTI_global->p_tasks;

    p_xstream->state = ABT_XSTREAM_STATE_RUNNING;

    ABTI_sched *p_sched = p_xstream->p_sched;
    ABT_pool pool = p_sched->pool;
    while (1) {
        /* If there exist tasks in thte global task pool, steal one and
         * execute it. */
        if (ABTI_pool_get_size(p_tasks->pool) > 0) {
            ABTI_task *p_task;
            abt_errno = ABTI_global_pop_task(&p_task);
            ABTI_CHECK_ERROR(abt_errno);
            if (!p_task) continue;

            /* Set the associated ES and execute */
            p_task->p_xstream = p_xstream;
            abt_errno = ABTI_xstream_schedule_task(p_task);
            ABTI_CHECK_ERROR(abt_errno);

            continue;
        }

        /* Execute one work unit from the scheduler's pool */
        if (p_sched->p_get_size(pool) > 0) {
            /* Pop one work unit */
            ABT_unit unit = ABTI_sched_pop(p_sched);

            ABT_unit_type type = p_sched->u_get_type(unit);
            if (type == ABT_UNIT_TYPE_THREAD) {
                ABT_thread thread = p_sched->u_get_thread(unit);
                ABTI_thread *p_thread = ABTI_thread_get_ptr(thread);

                /* Switch the context */
                abt_errno = ABTI_xstream_schedule_thread(p_thread);
                ABTI_CHECK_ERROR(abt_errno);
            } else if (type == ABT_UNIT_TYPE_TASK) {
                ABT_task task = p_sched->u_get_task(unit);
                ABTI_task *p_task = ABTI_task_get_ptr(task);

                /* Execute the task */
                abt_errno = ABTI_xstream_schedule_task(p_task);
                ABTI_CHECK_ERROR(abt_errno);
            } else {
                HANDLE_ERROR("Not supported type!");
                abt_errno = ABT_ERR_INV_UNIT;
                goto fn_fail;
            }
        } else if (p_sched->num_blocked == 0) {
            /* Excape the loop when there is no work unit in the pool and
             * no ULT blocked. */
            break;
        }
    }

  fn_exit:
    p_xstream->state = ABT_XSTREAM_STATE_READY;
    return abt_errno;

  fn_fail:
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

    ABTI_xstream *p_xstream = p_thread->p_xstream;
    ABTI_sched *p_sched = p_xstream->p_sched;

    ABTI_local_set_thread(p_thread);

    /* Change the thread state */
    p_thread->state = ABT_THREAD_STATE_RUNNING;

    /* Switch the context */
    DEBUG_PRINT("[S%lu:TH%lu] START\n", p_xstream->id, p_thread->id);
    abt_errno = ABTD_thread_context_switch(&p_sched->ctx, &p_thread->ctx);

    /* The scheduler continues from here. */
    if (abt_errno != ABT_SUCCESS) {
        HANDLE_ERROR("ABTD_thread_context_switch");
        goto fn_fail;
    }

    /* The previous thread may not be the same as one to which the
     * context has been switched. */
    p_thread = ABTI_local_get_thread();
    p_xstream = p_thread->p_xstream;
    DEBUG_PRINT("[S%lu:TH%lu] END\n", p_xstream->id, p_thread->id);

    if ((p_thread->request & ABTI_THREAD_REQ_TERMINATE) ||
        (p_thread->request & ABTI_THREAD_REQ_CANCEL) ||
        (p_thread->request & ABTI_THREAD_REQ_EXIT)) {
        abt_errno = ABTI_xstream_terminate_thread(p_thread);
    } else if (p_thread->state != ABT_THREAD_STATE_BLOCKED) {
        /* The thread did not finish its execution.
         * Change the state of current running thread and
         * add it to the pool again. */
        abt_errno = ABTI_xstream_add_thread(p_thread);
    }
    ABTI_CHECK_ERROR(abt_errno);

    /* Now, no thread is running */
    ABTI_local_set_thread(NULL);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABTI_xstream_schedule_thread", abt_errno);
    goto fn_exit;
}

int ABTI_xstream_schedule_task(ABTI_task *p_task)
{
    int abt_errno = ABT_SUCCESS;

    if (p_task->request & ABTI_TASK_REQ_CANCEL) {
        abt_errno = ABTI_xstream_terminate_task(p_task);
        ABTI_CHECK_ERROR(abt_errno);
        goto fn_exit;
    }

    /* Set the current running tasklet */
    ABTI_local_set_task(p_task);

    /* Change the task state */
    p_task->state = ABT_TASK_STATE_RUNNING;

    /* Execute the task function */
    DEBUG_PRINT("[S%lu:TK%lu] START\n", p_task->p_xstream->id, p_task->id);
    p_task->f_task(p_task->p_arg);
    DEBUG_PRINT("[S%lu:TK%lu] END\n", p_task->p_xstream->id, p_task->id);

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
    ABTI_xstream *p_xstream;
    ABTI_sched *p_sched;

    /* callback function */
    if (p_thread->attr.f_cb) {
        ABT_thread thread = ABTI_thread_get_handle(p_thread);
        p_thread->attr.f_cb(thread, p_thread->attr.p_cb_arg);
    }

    ABTI_mutex_waitlock(p_thread->mutex);
    {
        /* extracting argument in migration request */
        p_xstream = (ABTI_xstream *)ABTI_thread_extract_req_arg(p_thread,
                ABTI_THREAD_REQ_MIGRATE);
        ABTD_atomic_fetch_and_uint32(&p_thread->request,
                ~ABTI_THREAD_REQ_MIGRATE);

        DEBUG_PRINT("[TH%lu] Migration: S%lu -> S%lu\n",
                p_thread->id, p_thread->p_xstream->id, p_xstream->id);

        /* Change the associated ES */
        p_thread->p_xstream = p_xstream;

        ABTI_mutex_waitlock(p_xstream->mutex);
        {
            p_sched = p_xstream->p_sched;
            ABTD_thread_context_change_link(&p_thread->ctx, &p_sched->ctx);

            /* Add the unit to the scheduler's pool */
            ABTI_sched_push(p_sched, p_thread->unit);
        }
        ABT_mutex_unlock(p_xstream->mutex);
    }
    ABT_mutex_unlock(p_thread->mutex);

    /* checking the state destination xstream */
    if (p_xstream->state == ABT_XSTREAM_STATE_CREATED) {
        abt_errno = ABTI_xstream_start(p_xstream);
        if (abt_errno != ABT_SUCCESS) {
            HANDLE_ERROR("ABTI_xstream_start");
            goto fn_fail;
        }
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
    /* The thread's ES must not be changed during this function.
     * So, its mutex is used to guarantee it. */
    ABTI_mutex_waitlock(p_thread->mutex);

    ABTI_xstream *p_xstream = p_thread->p_xstream;
    ABTI_sched *p_sched = p_xstream->p_sched;

    /* Add the unit to the scheduler's pool */
    ABTI_sched_push(p_sched, p_thread->unit);

    /* Set the thread's state as READY */
    p_thread->state = ABT_THREAD_STATE_READY;

    ABT_mutex_unlock(p_thread->mutex);

    return ABT_SUCCESS;
}

int ABTI_xstream_keep_thread(ABTI_thread *p_thread)
{
    /* The thread's ES must not be changed during this function.
     * So, its mutex is used to guarantee it. */
    ABTI_mutex_waitlock(p_thread->mutex);

    ABTI_xstream *p_xstream = p_thread->p_xstream;

    /* If the scheduler is not a default one, free the unit and create
     * an internal unit to add to the deads pool. */
    ABTI_sched *p_sched = p_xstream->p_sched;
    if (p_sched->type != ABTI_SCHED_TYPE_DEFAULT) {
        p_sched->u_free(&p_thread->unit);

        ABT_thread thread = ABTI_thread_get_handle(p_thread);
        p_thread->unit = ABTI_unit_create_from_thread(thread);
    }

    /* Add the unit to the deads pool */
    ABTI_mutex_waitlock(p_xstream->mutex);
    ABTI_pool_push(p_xstream->deads, p_thread->unit);
    ABT_mutex_unlock(p_xstream->mutex);

    /* Set the thread's state as TERMINATED */
    p_thread->state = ABT_THREAD_STATE_TERMINATED;

    ABT_mutex_unlock(p_thread->mutex);

    return ABT_SUCCESS;
}

int ABTI_xstream_keep_task(ABTI_task *p_task)
{
    ABTI_xstream *p_xstream = p_task->p_xstream;
    ABTI_sched *p_sched = p_xstream->p_sched;

    /* If the scheduler is not a default one, free the unit and create
     * an internal unit to add to the deads pool. */
    if (p_sched->type != ABTI_SCHED_TYPE_DEFAULT) {
        p_sched->u_free(&p_task->unit);

        ABT_task task = ABTI_task_get_handle(p_task);
        p_task->unit = ABTI_unit_create_from_task(task);
    }

    /* Add the unit to the deads pool */
    ABTI_mutex_waitlock(p_xstream->mutex);
    ABTI_pool_push(p_xstream->deads, p_task->unit);
    ABT_mutex_unlock(p_xstream->mutex);

    /* Set the task's state as TERMINATED */
    p_task->state = ABT_TASK_STATE_TERMINATED;

    return ABT_SUCCESS;
}

int ABTI_xstream_print(ABTI_xstream *p_xstream)
{
    int abt_errno = ABT_SUCCESS;
    if (p_xstream == NULL) {
        printf("NULL XSTREAM\n");
        goto fn_exit;
    }

    printf("== XSTREAM (%p) ==\n", p_xstream);
    printf("unit   : ");
    ABTI_unit_print(p_xstream->unit);
    printf("rank   : %lu\n", p_xstream->rank);
    printf("name   : %s\n", p_xstream->p_name);
    printf("type   : ");
    switch (p_xstream->type) {
        case ABTI_XSTREAM_TYPE_PRIMARY:   printf("PRIMARY\n"); break;
        case ABTI_XSTREAM_TYPE_SECONDARY: printf("SECONDARY\n"); break;
        default: printf("UNKNOWN\n"); break;
    }
    printf("state  : ");
    switch (p_xstream->state) {
        case ABT_XSTREAM_STATE_CREATED:    printf("CREATED\n"); break;
        case ABT_XSTREAM_STATE_READY:      printf("READY\n"); break;
        case ABT_XSTREAM_STATE_RUNNING:    printf("RUNNING\n"); break;
        case ABT_XSTREAM_STATE_TERMINATED: printf("TERMINATED\n"); break;
        default: printf("UNKNOWN\n"); break;
    }
    printf("request: %x\n", p_xstream->request);

    printf("sched  : %p\n", p_xstream->p_sched);
    abt_errno = ABTI_sched_print(p_xstream->p_sched);
    ABTI_CHECK_ERROR(abt_errno);

    printf("deads  :");
    abt_errno = ABTI_pool_print(p_xstream->deads);
    ABTI_CHECK_ERROR(abt_errno);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABTI_xstream_print", abt_errno);
    goto fn_exit;
}


/*****************************************************************************/
/* Internal static functions                                                 */
/*****************************************************************************/

static uint64_t ABTI_xstream_get_new_rank(void)
{
    static uint64_t xstream_rank = 0;
    return ABTD_atomic_fetch_add_uint64(&xstream_rank, 1);
}

static void *ABTI_xstream_loop(void *p_arg)
{
    ABTI_xstream *p_xstream = (ABTI_xstream *)p_arg;

    DEBUG_PRINT("[S%lu] START\n", p_xstream->id);

    /* Set this ES as the current ES */
    ABTI_local_init(p_xstream);

    ABTI_task_pool *p_tasks = gp_ABTI_global->p_tasks;
    ABTI_sched *p_sched = p_xstream->p_sched;
    ABT_pool pool = p_sched->pool;

    while (1) {
        /* If there is an exit or a cancel request, the ES terminates
         * regardless of remaining work units. */
        if ((p_xstream->request & ABTI_XSTREAM_REQ_EXIT) ||
            (p_xstream->request & ABTI_XSTREAM_REQ_CANCEL))
            break;

        /* When join is requested, the ES terminates after finishing
         * execution of all work units. */
        if ((p_xstream->request & ABTI_XSTREAM_REQ_JOIN) &&
            (ABTI_pool_get_size(p_tasks->pool) == 0) &&
            (p_sched->p_get_size(pool) == 0) &&
            (p_sched->num_blocked == 0))
            break;

        if (ABTI_xstream_schedule(p_xstream) != ABT_SUCCESS) {
            HANDLE_ERROR("ABTI_xstream_schedule");
            goto fn_exit;
        }
    }

  fn_exit:
    /* Set the xstream's state as TERMINATED */
    p_xstream->state = ABT_XSTREAM_STATE_TERMINATED;

    /* Move the xstream to the deads pool */
    ABTI_global_move_xstream(p_xstream);

    /* Reset the current ES and thread info. */
    ABTI_local_finalize();

    DEBUG_PRINT("[S%lu] END\n", p_xstream->id);

    ABTD_xstream_context_exit();

    return NULL;
}

