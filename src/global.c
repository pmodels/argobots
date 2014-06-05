/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"

/** @defgroup ENV Init & Finalize
 * This group is for initialization and finalization of the Argobots environment.
 */

/* Global Data */
ABTI_global *gp_ABTI_global = NULL;

/**
 * @ingroup ENV
 * @brief   Initialize the Argobots execution environment.
 *
 * This must be called by the main thread before using any other Argobots APIs.
 * The stream object for primary stream is created in this routine.
 *
 * @param[in] argc the number of arguments
 * @param[in] argv the argument vector
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_init(int argc, char **argv)
{
    assert(gp_ABTI_global == NULL);
    int abt_errno = ABT_SUCCESS;
    ABTI_stream_pool *p_streams;
    ABTI_task_pool *p_tasks;

    gp_ABTI_global = (ABTI_global *)ABTU_malloc(sizeof(ABTI_global));
    if (!gp_ABTI_global) {
        HANDLE_ERROR("ABTU_malloc");
        abt_errno = ABT_ERR_MEM;
        goto fn_fail;
    }

    /* Initialize the stream pool */
    p_streams = (ABTI_stream_pool *)ABTU_malloc(sizeof(ABTI_stream_pool));
    if (!p_streams) {
        HANDLE_ERROR("ABTU_malloc");
        abt_errno = ABT_ERR_MEM;
        goto fn_fail;
    }
    abt_errno = ABTI_stream_pool_init(p_streams);
    if (abt_errno != ABT_SUCCESS) {
        HANDLE_ERROR("ABTI_stream_pool_init");
        goto fn_fail;
    }
    gp_ABTI_global->p_streams = p_streams;

    /* Initialize the task pool */
    p_tasks = (ABTI_task_pool *)ABTU_malloc(sizeof(ABTI_task_pool));
    if (!p_tasks) {
        HANDLE_ERROR("ABTU_malloc");
        abt_errno = ABT_ERR_MEM;
        goto fn_fail;
    }
    abt_errno = ABTI_task_pool_init(p_tasks);
    if (abt_errno != ABT_SUCCESS) {
        HANDLE_ERROR("ABTI_task_pool_init");
        goto fn_fail;
    }
    gp_ABTI_global->p_tasks = p_tasks;

    /* Create the primary stream */
    ABT_stream stream;
    abt_errno = ABT_stream_create(ABT_SCHEDULER_NULL, &stream);
    if (abt_errno != ABT_SUCCESS) {
        HANDLE_ERROR("ABT_stream_create");
        goto fn_fail;
    }
    ABTI_stream *p_stream = ABTI_stream_get_ptr(stream);
    p_stream->type = ABTI_STREAM_TYPE_PRIMARY;

    /* Start the primary stream */
    abt_errno = ABTI_stream_start(p_stream);
    if (abt_errno != ABT_SUCCESS) {
        HANDLE_ERROR("ABTI_stream_start");
        goto fn_fail;
    }

    /* Init the ES local data */
    abt_errno = ABTI_local_init(p_stream);
    if (abt_errno != ABT_SUCCESS) {
        HANDLE_ERROR("ABTI_local_init");
        goto fn_fail;
    }

    /* Create the main thread */
    ABTI_thread *p_thread;
    abt_errno = ABTI_thread_create_main(p_stream, &p_thread);
    if (abt_errno != ABT_SUCCESS) {
        HANDLE_ERROR("ABTI_thread_create_main");
        goto fn_fail;
    }
    ABTI_local_set_thread(p_thread);

  fn_exit:
    return abt_errno;

  fn_fail:
    goto fn_exit;
}

/**
 * @ingroup ENV
 * @brief   Terminate the Argobots execution environment.
 *
 * This must be called by the main thread. Invoking the Argobots APIs after
 * ABT_finalize() is not allowed. This routine also contains deallocation of
 * stream object for the primary stream.
 *
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_finalize()
{
    assert(gp_ABTI_global != NULL);
    int abt_errno = ABT_SUCCESS;

    ABTI_stream *p_stream = ABTI_local_get_stream();
    if (p_stream->type != ABTI_STREAM_TYPE_PRIMARY) {
        HANDLE_ERROR("ABT_finalize must be called by the primary stream.");
        abt_errno = ABT_ERR_INV_STREAM;
        goto fn_fail;
    }

    ABTI_thread *p_thread = ABTI_local_get_thread();
    if (p_thread->type != ABTI_THREAD_TYPE_MAIN) {
        HANDLE_ERROR("ABT_finalize must be called by the main stream.");
        abt_errno = ABT_ERR_INV_THREAD;
        goto fn_fail;
    }

    /* Remove the main thread */
    abt_errno = ABTI_thread_free_main(p_thread);
    ABTI_CHECK_ERROR(abt_errno);

    /* Remove the primary stream from the global stream pool */
    abt_errno = ABTI_global_del_stream(p_stream);
    ABTI_CHECK_ERROR(abt_errno);

    /* Finalize the ES local data */
    abt_errno = ABTI_local_finalize();
    ABTI_CHECK_ERROR(abt_errno);

    /* Free the stream */
    abt_errno = ABTI_stream_free(p_stream);
    ABTI_CHECK_ERROR(abt_errno);

    /* Finalize the stream pool */
    abt_errno = ABTI_stream_pool_finalize(gp_ABTI_global->p_streams);
    if (abt_errno != ABT_SUCCESS) {
        HANDLE_ERROR("ABTI_stream_finalize");
        goto fn_fail;
    }
    ABTU_free(gp_ABTI_global->p_streams);

    /* Finalize the task pool */
    abt_errno = ABTI_task_pool_finalize(gp_ABTI_global->p_tasks);
    if (abt_errno != ABT_SUCCESS) {
        HANDLE_ERROR("ABTI_task_pool_finalize");
        goto fn_fail;
    }
    ABTU_free(gp_ABTI_global->p_tasks);

    /* Free the ABTI_global structure */
    ABTU_free(gp_ABTI_global);
    gp_ABTI_global = NULL;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_finalize", abt_errno);
    goto fn_exit;
}


/* Private APIs */
int ABTI_stream_pool_init(ABTI_stream_pool *p_streams)
{
    int abt_errno = ABT_SUCCESS;

    /* Create stream pools */
    abt_errno = ABTI_pool_create(&p_streams->created);
    ABTI_CHECK_ERROR(abt_errno);

    abt_errno = ABTI_pool_create(&p_streams->active);
    ABTI_CHECK_ERROR(abt_errno);

    abt_errno = ABTI_pool_create(&p_streams->deads);
    ABTI_CHECK_ERROR(abt_errno);

    /* Initialize the mutex */
    abt_errno = ABT_mutex_create(&p_streams->mutex);
    ABTI_CHECK_ERROR(abt_errno);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABTI_stream_pool_init", abt_errno);
    goto fn_exit;
}

int ABTI_stream_pool_finalize(ABTI_stream_pool *p_streams)
{
    int abt_errno = ABT_SUCCESS;

    /* Wait until all running streams are terminated */
    while (ABTI_pool_get_size(p_streams->active) > 0) {
        ABT_thread_yield();
    }

    /* Free all pools */
    ABTI_mutex_waitlock(p_streams->mutex);
    ABTI_pool_free(&p_streams->created);
    ABTI_pool_free(&p_streams->active);
    ABTI_pool_free(&p_streams->deads);
    ABT_mutex_unlock(p_streams->mutex);

    /* Destroy the mutex */
    abt_errno = ABT_mutex_free(&p_streams->mutex);
    ABTI_CHECK_ERROR(abt_errno);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABTI_stream_pool_finalize", abt_errno);
    goto fn_exit;
}

int ABTI_task_pool_init(ABTI_task_pool *p_tasks)
{
    int abt_errno = ABT_SUCCESS;

    /* Create a task pool */
    abt_errno = ABTI_pool_create(&p_tasks->pool);
    ABTI_CHECK_ERROR(abt_errno);

    /* Initialize the mutex */
    abt_errno = ABT_mutex_create(&p_tasks->mutex);
    ABTI_CHECK_ERROR(abt_errno);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABTI_task_pool_init", abt_errno);
    goto fn_exit;
}

int ABTI_task_pool_finalize(ABTI_task_pool *p_tasks)
{
    int abt_errno = ABT_SUCCESS;

    /* Clean up the pool */
    ABTI_mutex_waitlock(p_tasks->mutex);
    abt_errno = ABTI_pool_free(&p_tasks->pool);
    ABT_mutex_unlock(p_tasks->mutex);
    ABTI_CHECK_ERROR(abt_errno);

    /* Destroy the mutex */
    abt_errno = ABT_mutex_free(&p_tasks->mutex);
    ABTI_CHECK_ERROR(abt_errno);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABTI_task_pool_finalize", abt_errno);
    goto fn_exit;
}

int ABTI_global_add_stream(ABTI_stream *p_stream)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_stream_pool *p_gstreams = gp_ABTI_global->p_streams;

    ABTI_mutex_waitlock(p_gstreams->mutex);
    switch (p_stream->state) {
        case ABT_STREAM_STATE_CREATED:
            ABTI_pool_push(p_gstreams->created, p_stream->unit);
            break;
        case ABT_STREAM_STATE_READY:
        case ABT_STREAM_STATE_RUNNING:
            ABTI_pool_push(p_gstreams->active, p_stream->unit);
            break;
        case ABT_STREAM_STATE_TERMINATED:
            ABTI_pool_push(p_gstreams->deads, p_stream->unit);
            break;
        default:
            HANDLE_ERROR("Unknown stream type");
            abt_errno = ABT_ERR_INV_STREAM;
            break;
    }
    ABT_mutex_unlock(p_gstreams->mutex);

    return abt_errno;
}

int ABTI_global_move_stream(ABTI_stream *p_stream)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_stream_pool *p_gstreams = gp_ABTI_global->p_streams;

    ABTI_unit *p_unit = ABTI_unit_get_ptr(p_stream->unit);
    ABT_pool prev_pool = ABTI_pool_get_handle(p_unit->p_pool);

    /* Remove from the previous pool and add to the new pool */
    ABTI_mutex_waitlock(p_gstreams->mutex);
    ABTI_pool_remove(prev_pool, p_stream->unit);
    switch (p_stream->state) {
        case ABT_STREAM_STATE_CREATED:
            HANDLE_ERROR("SHOULD NOT REACH HERE");
            abt_errno = ABT_ERR_STREAM;
            break;
        case ABT_STREAM_STATE_READY:
        case ABT_STREAM_STATE_RUNNING:
            ABTI_pool_push(p_gstreams->active, p_stream->unit);
            break;
        case ABT_STREAM_STATE_TERMINATED:
            ABTI_pool_push(p_gstreams->deads, p_stream->unit);
            break;
        default:
            HANDLE_ERROR("UNKNOWN STREAM TYPE");
            abt_errno = ABT_ERR_INV_STREAM;
            break;
    }
    ABT_mutex_unlock(p_gstreams->mutex);

    return abt_errno;
}

int ABTI_global_del_stream(ABTI_stream *p_stream)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_stream_pool *p_gstreams = gp_ABTI_global->p_streams;

    ABTI_unit *p_unit = ABTI_unit_get_ptr(p_stream->unit);
    ABT_pool prev_pool = ABTI_pool_get_handle(p_unit->p_pool);

    ABTI_mutex_waitlock(p_gstreams->mutex);
    ABTI_pool_remove(prev_pool, p_stream->unit);
    ABT_mutex_unlock(p_gstreams->mutex);

    return abt_errno;
}

int ABTI_global_get_created_stream(ABTI_stream **p_stream)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_stream_pool *p_gstreams = gp_ABTI_global->p_streams;

    /* Pop one stream */
    ABTI_mutex_waitlock(p_gstreams->mutex);
    ABT_unit unit = ABTI_pool_pop(p_gstreams->created);
    ABT_mutex_unlock(p_gstreams->mutex);

    /* Return value */
    ABT_stream stream = ABTI_unit_get_stream(unit);
    *p_stream = ABTI_stream_get_ptr(stream);

    return abt_errno;
}

int ABTI_global_add_task(ABTI_task *p_task)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_task_pool *p_tasks = gp_ABTI_global->p_tasks;

    ABTI_mutex_waitlock(p_tasks->mutex);
    ABTI_pool_push(p_tasks->pool, p_task->unit);
    ABT_mutex_unlock(p_tasks->mutex);

    return abt_errno;
}

int ABTI_global_del_task(ABTI_task *p_task)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_task_pool *p_tasks = gp_ABTI_global->p_tasks;

    ABTI_mutex_waitlock(p_tasks->mutex);
    ABTI_pool_remove(p_tasks->pool, p_task->unit);
    ABT_mutex_unlock(p_tasks->mutex);

    return abt_errno;
}

int ABTI_global_pop_task(ABTI_task **p_task)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_task_pool *p_tasks = gp_ABTI_global->p_tasks;
    ABT_unit unit;
    ABT_task h_task;

    ABTI_mutex_waitlock(p_tasks->mutex);
    unit = ABTI_pool_pop(p_tasks->pool);
    ABT_mutex_unlock(p_tasks->mutex);

    h_task = ABTI_unit_get_task(unit);
    *p_task = ABTI_task_get_ptr(h_task);

    return abt_errno;
}

int ABTI_global_has_task(int *result)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_task_pool *p_tasks = gp_ABTI_global->p_tasks;
    *result = ABTI_pool_get_size(p_tasks->pool) > 0;
    return abt_errno;
}

