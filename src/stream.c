/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"

static uint64_t ABTI_stream_get_new_id();
static void *ABTI_stream_loop(void *p_arg);


/** @defgroup ES Execution Stream (ES)
 * This group is for Execution Stream.
 */


/**
 * @ingroup ES
 * @brief   Create a new stream and return its handle through newstream.
 *
 * @param[in]  sched  handle to the scheduler used for a new stream. If this is
 *                    ABT_SCHEDULER_NULL, the runtime-provided scheduler is used.
 * @param[out] newstream  handle to a newly created stream. This cannot be NULL
 *                    because unnamed stream is not allowed.
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_stream_create(ABT_scheduler sched, ABT_stream *newstream)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_stream *p_newstream;
    ABT_stream h_newstream;

    p_newstream = (ABTI_stream *)ABTU_malloc(sizeof(ABTI_stream));
    if (!p_newstream) {
        HANDLE_ERROR("ABTU_malloc");
        *newstream = ABT_STREAM_NULL;
        abt_errno = ABT_ERR_MEM;
        goto fn_fail;
    }

    /* Create a wrapper unit */
    h_newstream = ABTI_stream_get_handle(p_newstream);
    p_newstream->unit = ABTI_unit_create_from_stream(h_newstream);

    p_newstream->id      = ABTI_stream_get_new_id();
    p_newstream->p_name  = NULL;
    p_newstream->type    = ABTI_STREAM_TYPE_SECONDARY;
    p_newstream->state   = ABT_STREAM_STATE_CREATED;
    p_newstream->request = 0;

    /* Create a mutex */
    abt_errno = ABT_mutex_create(&p_newstream->mutex);
    ABTI_CHECK_ERROR(abt_errno);

    /* Set the scheduler */
    if (sched == ABT_SCHEDULER_NULL) {
        /* Default scheduler */
        abt_errno = ABTI_scheduler_create_default(&p_newstream->p_sched);
        if (abt_errno != ABT_SUCCESS) {
            HANDLE_ERROR("ABTI_scheduler_create_default");
            ABTU_free(p_newstream);
            *newstream = ABT_STREAM_NULL;
            goto fn_fail;
        }
    } else {
        p_newstream->p_sched = ABTI_scheduler_get_ptr(sched);
    }

    /* Create a work unit pool that contains terminated work units */
    abt_errno = ABTI_pool_create(&p_newstream->deads);
    ABTI_CHECK_ERROR(abt_errno);

    /* Add this stream to the global stream pool */
    ABTI_global_add_stream(p_newstream);

    /* Return value */
    *newstream = h_newstream;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_stream_create", abt_errno);
    goto fn_exit;
}

/**
 * @ingroup ES
 * @brief   Release the stream object associated with stream handle.
 *
 * This routine deallocates memory used for the stream object. If the stream
 * is still running when this routine is called, the deallocation happens
 * after the stream terminates and then this routine returns. If it is
 * successfully processed, stream is set as ABT_STREAM_NULL. The primary
 * stream cannot be freed with this routine.
 *
 * @param[in,out] stream  handle to the target stream
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_stream_free(ABT_stream *stream)
{
    int abt_errno = ABT_SUCCESS;
    ABT_stream h_stream = *stream;

    ABTI_stream *p_stream = ABTI_stream_get_ptr(h_stream);
    if (p_stream == NULL) goto fn_exit;

    if (p_stream == ABTI_local_get_stream()) {
        HANDLE_ERROR("The current stream cannot be freed.");
        abt_errno = ABT_ERR_INV_STREAM;
        goto fn_fail;
    }

    if (p_stream->type == ABTI_STREAM_TYPE_PRIMARY) {
        HANDLE_ERROR("The primary stream cannot be freed explicitly.");
        abt_errno = ABT_ERR_INV_STREAM;
        goto fn_fail;
    }

    /* If the stream is running, wait until it terminates */
    if (p_stream->state == ABT_STREAM_STATE_RUNNING) {
        abt_errno = ABT_stream_join(h_stream);
        ABTI_CHECK_ERROR(abt_errno);
    }

    /* Remove this stream from the global stream pool */
    abt_errno = ABTI_global_del_stream(p_stream);
    ABTI_CHECK_ERROR(abt_errno);

    /* Free the stream object */
    abt_errno = ABTI_stream_free(p_stream);
    ABTI_CHECK_ERROR(abt_errno);

    /* Return value */
    *stream = ABT_STREAM_NULL;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_stream_free", abt_errno);
    goto fn_exit;
}

/**
 * @ingroup ES
 * @brief   Wait for stream to terminate.
 *
 * The target stream cannot be the same as the stream associated with calling
 * thread. If they are identical, this routine returns immediately without
 * waiting for the stream's termination.
 *
 * @param[in] stream  handle to the target stream
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_stream_join(ABT_stream stream)
{
    int abt_errno = ABT_SUCCESS;
    uint32_t old;
    ABTI_stream *p_stream = ABTI_stream_get_ptr(stream);

    /* The target stream must not be the same as the calling thread's
     * stream */
    if (p_stream == ABTI_local_get_stream()) {
        HANDLE_ERROR("The target stream should be different.");
        abt_errno = ABT_ERR_INV_STREAM;
        goto fn_fail;
    }

    if (p_stream->type == ABTI_STREAM_TYPE_PRIMARY) {
        HANDLE_ERROR("The primary stream cannot be joined.");
        abt_errno = ABT_ERR_INV_STREAM;
        goto fn_fail;
    }

    if (p_stream->state == ABT_STREAM_STATE_CREATED) {
        ABTI_mutex_waitlock(p_stream->mutex);
        /* If stream's state was changed, we cannot terminate it here */
        if (ABTD_atomic_cas_int32((int32_t *)&p_stream->state,
                                  ABT_STREAM_STATE_CREATED,
                                  ABT_STREAM_STATE_TERMINATED)
            != ABT_STREAM_STATE_CREATED) {
            ABT_mutex_unlock(p_stream->mutex);
            goto fn_body;
        }
        abt_errno = ABTI_global_move_stream(p_stream);
        ABT_mutex_unlock(p_stream->mutex);
        if (abt_errno != ABT_SUCCESS) {
            HANDLE_ERROR("ABTI_global_move_stream");
            goto fn_fail;
        }
        goto fn_exit;
    }

  fn_body:
    /* Set the join request */
    old = ABTD_atomic_fetch_or_uint32(&p_stream->request,
                                      ABTI_STREAM_REQ_JOIN);
    if (old & ABTI_STREAM_REQ_JOIN) {
        /* If the join reqeust has aleady been checked, wait until
         * the stream is terminated */
        do {
            ABT_thread_yield();
        } while (p_stream->state != ABT_STREAM_STATE_TERMINATED);
    } else {
        /* Normal join request */
        abt_errno = ABTD_stream_context_join(p_stream->ctx);
        if (abt_errno != ABT_SUCCESS) {
            HANDLE_ERROR("ABTD_stream_context_join");
            goto fn_fail;
        }
    }

  fn_exit:
    return abt_errno;

  fn_fail:
    goto fn_exit;
}

/**
 * @ingroup ES
 * @brief   The calling thread terminates its associated stream.
 *
 * Since the calling thread's stream terminates, this routine never returns.
 *
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_stream_exit()
{
    int abt_errno = ABT_SUCCESS;

    ABTI_stream *p_stream = ABTI_local_get_stream();

    /* Set the exit request */
    ABTD_atomic_fetch_or_uint32(&p_stream->request, ABTI_STREAM_REQ_EXIT);

    /* Wait until the stream terminates */
    do {
        ABT_thread_yield();
    } while (p_stream->state != ABT_STREAM_STATE_TERMINATED);

    return abt_errno;
}

/**
 * @ingroup ES
 * @brief   Request the cancelation of the target stream.
 *
 * @param[in] stream  handle to the target stream
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_stream_cancel(ABT_stream stream)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_stream *p_stream = ABTI_stream_get_ptr(stream);

    if (p_stream->type == ABTI_STREAM_TYPE_PRIMARY) {
        HANDLE_ERROR("The primary stream cannot be canceled.");
        abt_errno = ABT_ERR_INV_STREAM;
        goto fn_fail;
    }

    /* Set the cancel request */
    ABTD_atomic_fetch_or_uint32(&p_stream->request, ABTI_STREAM_REQ_CANCEL);

  fn_exit:
    return abt_errno;

  fn_fail:
    goto fn_exit;
}

/**
 * @ingroup ES
 * @brief   Return the stream handle of the calling thread.
 *
 * @param[out] stream  stream handle
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_stream_self(ABT_stream *stream)
{
    int abt_errno = ABT_SUCCESS;

    ABTI_stream *p_stream = ABTI_local_get_stream();
    if (p_stream == NULL) {
        HANDLE_ERROR("NULL STREAM");
        abt_errno = ABT_ERR_STREAM;
        goto fn_fail;
    }

    /* Return value */
    *stream = ABTI_stream_get_handle(p_stream);

  fn_exit:
    return abt_errno;

  fn_fail:
    goto fn_exit;
}

/**
 * @ingroup ES
 * @brief   Compare two stream handles for equality.
 *
 * @param[in]  stream1  handle to the stream 1
 * @param[in]  stream2  handle to the stream 2
 * @param[out] result   0: not same, 1: same
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_stream_equal(ABT_stream stream1, ABT_stream stream2, int *result)
{
    ABTI_stream *p_stream1 = ABTI_stream_get_ptr(stream1);
    ABTI_stream *p_stream2 = ABTI_stream_get_ptr(stream2);
    *result = p_stream1 == p_stream2;
    return ABT_SUCCESS;
}

/**
 * @ingroup ES
 * @brief   Set sched as stream’s scheduler.
 *
 * @param[in] stream  handle to the target stream
 * @param[in] sched   handle to the scheduler used for stream
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_stream_set_scheduler(ABT_stream stream, ABT_scheduler sched)
{
    int abt_errno = ABT_SUCCESS;

    ABTI_stream *p_stream = ABTI_stream_get_ptr(stream);
    if (p_stream == NULL) {
        abt_errno = ABT_ERR_INV_STREAM;
        goto fn_fail;
    }

    ABTI_scheduler *p_sched = ABTI_scheduler_get_ptr(sched);
    if (p_sched == NULL) {
        abt_errno = ABT_ERR_INV_SCHEDULER;
        goto fn_fail;
    }

    /* Set the scheduler */
    p_stream->p_sched = p_sched;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_stream_set_scheduler", abt_errno);
    goto fn_exit;
}

/**
 * @ingroup ES
 * @brief   Return the state of stream.
 *
 * @param[in]  stream  handle to the target stream
 * @param[out] state   the stream's state
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_stream_get_state(ABT_stream stream, ABT_stream_state *state)
{
    int abt_errno = ABT_SUCCESS;

    ABTI_stream *p_stream = ABTI_stream_get_ptr(stream);
    if (p_stream == NULL) {
        abt_errno = ABT_ERR_INV_STREAM;
        goto fn_fail;
    }

    /* Return value */
    *state = p_stream->state;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_stream_get_state", abt_errno);
    goto fn_exit;
}

/**
 * @ingroup ES
 * @brief   Set the stream's name.
 *
 * @param[in] stream  handle to the target stream
 * @param[in] name    stream name
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_stream_set_name(ABT_stream stream, const char *name)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_stream *p_stream = ABTI_stream_get_ptr(stream);
    if (p_stream == NULL) {
        abt_errno = ABT_ERR_INV_STREAM;
        goto fn_fail;
    }

    size_t len = strlen(name);
    ABTI_mutex_waitlock(p_stream->mutex);
    if (p_stream->p_name) ABTU_free(p_stream->p_name);
    p_stream->p_name = (char *)ABTU_malloc(len + 1);
    if (!p_stream->p_name) {
        ABT_mutex_unlock(p_stream->mutex);
        abt_errno = ABT_ERR_MEM;
        goto fn_fail;
    }
    ABTU_strcpy(p_stream->p_name, name);
    ABT_mutex_unlock(p_stream->mutex);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_stream_set_name", abt_errno);
    goto fn_exit;
}

/**
 * @ingroup ES
 * @brief   Return the stream's name and its length.
 *
 * If name is NULL, only len is returned.
 *
 * @param[in]  stream  handle to the target stream
 * @param[out] name    stream name
 * @param[out] len     the length of name in bytes
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_stream_get_name(ABT_stream stream, char *name, size_t *len)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_stream *p_stream = ABTI_stream_get_ptr(stream);
    if (p_stream == NULL) {
        abt_errno = ABT_ERR_INV_STREAM;
        goto fn_fail;
    }

    *len = strlen(p_stream->p_name);
    if (name != NULL) {
        ABTU_strcpy(name, p_stream->p_name);
    }

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_stream_get_name", abt_errno);
    goto fn_exit;
}


/* Private APIs */
ABTI_stream *ABTI_stream_get_ptr(ABT_stream stream)
{
    ABTI_stream *p_stream;
    if (stream == ABT_STREAM_NULL) {
        p_stream = NULL;
    } else {
        p_stream = (ABTI_stream *)stream;
    }
    return p_stream;
}

ABT_stream ABTI_stream_get_handle(ABTI_stream *p_stream)
{
    ABT_stream h_stream;
    if (p_stream == NULL) {
        h_stream = ABT_STREAM_NULL;
    } else {
        h_stream = (ABT_stream)p_stream;
    }
    return h_stream;
}

int ABTI_stream_free(ABTI_stream *p_stream)
{
    int abt_errno = ABT_SUCCESS;

    ABTI_unit_free(&p_stream->unit);
    if (p_stream->p_name) ABTU_free(p_stream->p_name);

    /* Free the scheduler */
    ABT_scheduler h_sched = ABTI_scheduler_get_handle(p_stream->p_sched);
    abt_errno = ABT_scheduler_free(&h_sched);
    ABTI_CHECK_ERROR(abt_errno);

    /* Clean up the deads pool */
    ABTI_mutex_waitlock(p_stream->mutex);
    ABTI_pool_free(&p_stream->deads);
    ABT_mutex_unlock(p_stream->mutex);

    /* Free the mutex */
    abt_errno = ABT_mutex_free(&p_stream->mutex);
    ABTI_CHECK_ERROR(abt_errno);

    /* Free the context */
    abt_errno = ABTD_stream_context_free(&p_stream->ctx);
    ABTI_CHECK_ERROR(abt_errno);

    ABTU_free(p_stream);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABTI_stream_free", abt_errno);
    goto fn_exit;
}

int ABTI_stream_start(ABTI_stream *p_stream)
{
    assert(p_stream->state == ABT_STREAM_STATE_CREATED);

    /* Set the stream's state as READY */
    ABT_stream_state old_state;
    old_state = ABTD_atomic_cas_int32((int32_t *)&p_stream->state,
            ABT_STREAM_STATE_CREATED, ABT_STREAM_STATE_READY);
    if (old_state != ABT_STREAM_STATE_CREATED) goto fn_exit;

    int abt_errno = ABT_SUCCESS;
    if (p_stream->type == ABTI_STREAM_TYPE_PRIMARY) {
        abt_errno = ABTD_stream_context_self(&p_stream->ctx);
        if (abt_errno != ABT_SUCCESS) {
            HANDLE_ERROR("ABTD_stream_context_self");
            goto fn_fail;
        }
    } else {
        abt_errno = ABTD_stream_context_create(
                ABTI_stream_loop, (void *)p_stream, &p_stream->ctx);
        if (abt_errno != ABT_SUCCESS) {
            HANDLE_ERROR("ABTD_stream_context_create");
            goto fn_fail;
        }
    }

    /* Move the stream to the global active stream pool */
    ABTI_global_move_stream(p_stream);

  fn_exit:
    return abt_errno;

  fn_fail:
    goto fn_exit;
}

int ABTI_stream_start_any()
{
    int abt_errno = ABT_SUCCESS;

    ABTI_stream_pool *p_streams = gp_ABTI_global->p_streams;

    /* If there exist one or more active streams or there is no stream
     * created, we have nothing to do. */
    if (ABTI_pool_get_size(p_streams->active) > 0) goto fn_exit;
    if (ABTI_pool_get_size(p_streams->created) == 0) goto fn_exit;

    /* Pop one stream from the global stream pool and start it */
    ABTI_stream *p_stream;
    abt_errno = ABTI_global_get_created_stream(&p_stream);
    ABTI_CHECK_ERROR(abt_errno);
    if (p_stream == NULL) goto fn_exit;

    if (p_stream->state == ABT_STREAM_STATE_CREATED) {
        abt_errno = ABTI_stream_start(p_stream);
        if (abt_errno != ABT_SUCCESS) {
            HANDLE_ERROR("ABTI_stream_start");
            goto fn_fail;
        }
    }

  fn_exit:
    return abt_errno;

  fn_fail:
    goto fn_exit;
}

int ABTI_stream_schedule(ABTI_stream *p_stream)
{
    int abt_errno = ABT_SUCCESS;

    ABTI_task_pool *p_tasks = gp_ABTI_global->p_tasks;

    p_stream->state = ABT_STREAM_STATE_RUNNING;

    ABTI_scheduler *p_sched = p_stream->p_sched;
    ABT_pool pool = p_sched->pool;
    while (p_sched->p_get_size(pool) > 0) {
        /* If there exist tasks in thte global task pool, steal one and
         * execute it. */
        if (ABTI_pool_get_size(p_tasks->pool) > 0) {
            ABTI_task *p_task;
            abt_errno = ABTI_global_pop_task(&p_task);
            ABTI_CHECK_ERROR(abt_errno);
            if (!p_task) continue;

            /* Set the associated stream and execute */
            p_task->p_stream = p_stream;
            abt_errno = ABTI_stream_schedule_task(p_task);
            ABTI_CHECK_ERROR(abt_errno);
        }

        /* Pop one work unit */
        ABT_unit unit = ABTI_scheduler_pop(p_sched);

        ABT_unit_type type = p_sched->u_get_type(unit);
        if (type == ABT_UNIT_TYPE_THREAD) {
            ABT_thread thread = p_sched->u_get_thread(unit);
            ABTI_thread *p_thread = ABTI_thread_get_ptr(thread);

            /* Switch the context */
            abt_errno = ABTI_stream_schedule_thread(p_thread);
            ABTI_CHECK_ERROR(abt_errno);
        } else if (type == ABT_UNIT_TYPE_TASK) {
            ABT_task task = p_sched->u_get_task(unit);
            ABTI_task *p_task = ABTI_task_get_ptr(task);

            /* Execute the task */
            abt_errno = ABTI_stream_schedule_task(p_task);
            ABTI_CHECK_ERROR(abt_errno);
        } else {
            HANDLE_ERROR("Not supported type!");
            abt_errno = ABT_ERR_INV_UNIT;
            goto fn_fail;
        }
    }

  fn_exit:
    p_stream->state = ABT_STREAM_STATE_READY;
    return abt_errno;

  fn_fail:
    goto fn_exit;
}

int ABTI_stream_schedule_thread(ABTI_thread *p_thread)
{
    int abt_errno = ABT_SUCCESS;

    if ((p_thread->request & ABTI_THREAD_REQ_CANCEL) ||
        (p_thread->request & ABTI_THREAD_REQ_EXIT)) {
        abt_errno = ABTI_stream_terminate_thread(p_thread);
        ABTI_CHECK_ERROR(abt_errno);
        goto fn_exit;
    }

    ABTI_stream *p_stream = p_thread->p_stream;
    ABTI_scheduler *p_sched = p_stream->p_sched;

    ABTI_local_set_thread(p_thread);

    /* Change the thread state */
    p_thread->state = ABT_THREAD_STATE_RUNNING;

    /* Switch the context */
    DEBUG_PRINT("[S%lu:TH%lu] START\n", p_stream->id, p_thread->id);
    abt_errno = ABTD_thread_context_switch(&p_sched->ctx, &p_thread->ctx);

    /* The scheduler continues from here. */
    if (abt_errno != ABT_SUCCESS) {
        HANDLE_ERROR("ABTD_thread_context_switch");
        goto fn_fail;
    }

    /* The previous thread may not be the same as one to which the
     * context has been switched. */
    p_thread = ABTI_local_get_thread();
    p_stream = p_thread->p_stream;
    DEBUG_PRINT("[S%lu:TH%lu] END\n", p_stream->id, p_thread->id);

    if ((p_thread->state == ABT_THREAD_STATE_COMPLETED) ||
        (p_thread->request & ABTI_THREAD_REQ_CANCEL) ||
        (p_thread->request & ABTI_THREAD_REQ_EXIT)) {
        abt_errno = ABTI_stream_terminate_thread(p_thread);
    } else {
        /* The thread did not finish its execution.
         * Change the state of current running thread and
         * add it to the pool again. */
        abt_errno = ABTI_stream_add_thread(p_thread);
    }
    ABTI_CHECK_ERROR(abt_errno);

    /* Now, no thread is running */
    ABTI_local_set_thread(NULL);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABTI_stream_schedule_thread", abt_errno);
    goto fn_exit;
}

int ABTI_stream_schedule_task(ABTI_task *p_task)
{
    int abt_errno = ABT_SUCCESS;

    if (p_task->request & ABTI_TASK_REQ_CANCEL) {
        abt_errno = ABTI_stream_terminate_task(p_task);
        ABTI_CHECK_ERROR(abt_errno);
        goto fn_exit;
    }

    /* Change the task state */
    p_task->state = ABT_TASK_STATE_RUNNING;

    /* Execute the task function */
    DEBUG_PRINT("[S%lu:TK%lu] START\n", p_task->p_stream->id, p_task->id);
    p_task->f_task(p_task->p_arg);
    p_task->state = ABT_TASK_STATE_COMPLETED;
    DEBUG_PRINT("[S%lu:TK%lu] END\n", p_task->p_stream->id, p_task->id);

    abt_errno = ABTI_stream_terminate_task(p_task);
    ABTI_CHECK_ERROR(abt_errno);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABTI_stream_schedule_task", abt_errno);
    goto fn_exit;
}

int ABTI_stream_terminate_thread(ABTI_thread *p_thread)
{
    int abt_errno;
    if (p_thread->refcount == 0) {
        p_thread->state = ABT_THREAD_STATE_TERMINATED;
        abt_errno = ABTI_thread_free(p_thread);
    } else {
        abt_errno = ABTI_stream_keep_thread(p_thread);
    }
    return abt_errno;
}

int ABTI_stream_terminate_task(ABTI_task *p_task)
{
    int abt_errno;
    if (p_task->refcount == 0) {
        p_task->state = ABT_TASK_STATE_TERMINATED;
        abt_errno = ABTI_task_free(p_task);
    } else {
        abt_errno = ABTI_stream_keep_task(p_task);
    }
    return abt_errno;
}

int ABTI_stream_add_thread(ABTI_thread *p_thread)
{
    /* The thread's stream must not be changed during this function.
     * So, its mutex is used to guarantee it. */
    ABTI_mutex_waitlock(p_thread->mutex);

    ABTI_stream *p_stream = p_thread->p_stream;
    ABTI_scheduler *p_sched = p_stream->p_sched;

    /* Add the unit to the scheduler's pool */
    ABTI_scheduler_push(p_sched, p_thread->unit);

    /* Set the thread's state as READY */
    p_thread->state = ABT_THREAD_STATE_READY;

    ABT_mutex_unlock(p_thread->mutex);

    return ABT_SUCCESS;
}

int ABTI_stream_keep_thread(ABTI_thread *p_thread)
{
    /* The thread's stream must not be changed during this function.
     * So, its mutex is used to guarantee it. */
    ABTI_mutex_waitlock(p_thread->mutex);

    ABTI_stream *p_stream = p_thread->p_stream;

    /* If the scheduler is not a default one, free the unit and create
     * an internal unit to add to the deads pool. */
    ABTI_scheduler *p_sched = p_stream->p_sched;
    if (p_sched->type != ABTI_SCHEDULER_TYPE_DEFAULT) {
        p_sched->u_free(&p_thread->unit);

        ABT_thread thread = ABTI_thread_get_handle(p_thread);
        p_thread->unit = ABTI_unit_create_from_thread(thread);
    }

    /* Add the unit to the deads pool */
    ABTI_mutex_waitlock(p_stream->mutex);
    ABTI_pool_push(p_stream->deads, p_thread->unit);
    ABT_mutex_unlock(p_stream->mutex);

    /* Set the thread's state as TERMINATED */
    p_thread->state = ABT_THREAD_STATE_TERMINATED;

    ABT_mutex_unlock(p_thread->mutex);

    return ABT_SUCCESS;
}

int ABTI_stream_keep_task(ABTI_task *p_task)
{
    ABTI_stream *p_stream = p_task->p_stream;
    ABTI_scheduler *p_sched = p_stream->p_sched;

    /* If the scheduler is not a default one, free the unit and create
     * an internal unit to add to the deads pool. */
    if (p_sched->type != ABTI_SCHEDULER_TYPE_DEFAULT) {
        p_sched->u_free(&p_task->unit);

        ABT_task task = ABTI_task_get_handle(p_task);
        p_task->unit = ABTI_unit_create_from_task(task);
    }

    /* Add the unit to the deads pool */
    ABTI_mutex_waitlock(p_stream->mutex);
    ABTI_pool_push(p_stream->deads, p_task->unit);
    ABT_mutex_unlock(p_stream->mutex);

    /* Set the task's state as TERMINATED */
    p_task->state = ABT_TASK_STATE_TERMINATED;

    return ABT_SUCCESS;
}

int ABTI_stream_print(ABTI_stream *p_stream)
{
    int abt_errno = ABT_SUCCESS;
    if (p_stream == NULL) {
        printf("NULL STREAM\n");
        goto fn_exit;
    }

    printf("== STREAM (%p) ==\n", p_stream);
    printf("unit   : ");
    ABTI_unit_print(p_stream->unit);
    printf("id     : %lu\n", p_stream->id);
    printf("name   : %s\n", p_stream->p_name);
    printf("type   : ");
    switch (p_stream->type) {
        case ABTI_STREAM_TYPE_PRIMARY:   printf("PRIMARY\n"); break;
        case ABTI_STREAM_TYPE_SECONDARY: printf("SECONDARY\n"); break;
        default: printf("UNKNOWN\n"); break;
    }
    printf("state  : ");
    switch (p_stream->state) {
        case ABT_STREAM_STATE_CREATED:    printf("CREATED\n"); break;
        case ABT_STREAM_STATE_READY:      printf("READY\n"); break;
        case ABT_STREAM_STATE_RUNNING:    printf("RUNNING\n"); break;
        case ABT_STREAM_STATE_TERMINATED: printf("TERMINATED\n"); break;
        default: printf("UNKNOWN\n"); break;
    }
    printf("request: %x\n", p_stream->request);

    printf("sched  : %p\n", p_stream->p_sched);
    abt_errno = ABTI_scheduler_print(p_stream->p_sched);
    ABTI_CHECK_ERROR(abt_errno);

    printf("deads  :");
    abt_errno = ABTI_pool_print(p_stream->deads);
    ABTI_CHECK_ERROR(abt_errno);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABTI_stream_print", abt_errno);
    goto fn_exit;
}


/* Internal static functions */
static uint64_t ABTI_stream_get_new_id()
{
    static uint64_t stream_id = 0;
    return ABTD_atomic_fetch_add_uint64(&stream_id, 1);
}

static void *ABTI_stream_loop(void *p_arg)
{
    ABTI_stream *p_stream = (ABTI_stream *)p_arg;

    DEBUG_PRINT("[S%lu] START\n", p_stream->id);

    /* Set this stream as the current global stream */
    ABTI_local_init(p_stream);

    ABTI_scheduler *p_sched = p_stream->p_sched;
    ABT_pool pool = p_sched->pool;

    while (1) {
        /* If there is an exit or a cancel request, the stream terminates
         * regardless of remaining work units. */
        if ((p_stream->request & ABTI_STREAM_REQ_EXIT) ||
            (p_stream->request & ABTI_STREAM_REQ_CANCEL))
            break;

        /* When join is requested, the stream terminates after finishing
         * execution of all work units. */
        if ((p_stream->request & ABTI_STREAM_REQ_JOIN) &&
            (p_sched->p_get_size(pool) == 0))
            break;

        if (ABTI_stream_schedule(p_stream) != ABT_SUCCESS) {
            HANDLE_ERROR("ABTI_stream_schedule");
            goto fn_exit;
        }
    }

  fn_exit:
    /* Set the stream's state as TERMINATED */
    p_stream->state = ABT_STREAM_STATE_TERMINATED;

    /* Move the stream to the deads pool */
    ABTI_global_move_stream(p_stream);

    /* Reset the current stream and thread info. */
    ABTI_local_finalize();

    DEBUG_PRINT("[S%lu] END\n", p_stream->id);

    ABTD_stream_context_exit();

    return NULL;
}

