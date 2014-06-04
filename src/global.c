/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"

/* Global Data */
ABTI_Global *gp_ABTI_Global = NULL;

int ABT_Init(int argc, char **argv)
{
    assert(gp_ABTI_Global == NULL);
    int abt_errno = ABT_SUCCESS;
    ABTI_Stream_pool *p_streams;
    ABTI_Task_pool *p_tasks;

    gp_ABTI_Global = (ABTI_Global *)ABTU_Malloc(sizeof(ABTI_Global));
    if (!gp_ABTI_Global) {
        HANDLE_ERROR("ABTU_Malloc");
        abt_errno = ABT_ERR_MEM;
        goto fn_fail;
    }

    /* Initialize the stream pool */
    p_streams = (ABTI_Stream_pool *)ABTU_Malloc(sizeof(ABTI_Stream_pool));
    if (!p_streams) {
        HANDLE_ERROR("ABTU_Malloc");
        abt_errno = ABT_ERR_MEM;
        goto fn_fail;
    }
    abt_errno = ABTI_Stream_pool_init(p_streams);
    if (abt_errno != ABT_SUCCESS) {
        HANDLE_ERROR("ABTI_Stream_pool_init");
        goto fn_fail;
    }
    gp_ABTI_Global->p_streams = p_streams;

    /* Initialize the task pool */
    p_tasks = (ABTI_Task_pool *)ABTU_Malloc(sizeof(ABTI_Task_pool));
    if (!p_tasks) {
        HANDLE_ERROR("ABTU_Malloc");
        abt_errno = ABT_ERR_MEM;
        goto fn_fail;
    }
    abt_errno = ABTI_Task_pool_init(p_tasks);
    if (abt_errno != ABT_SUCCESS) {
        HANDLE_ERROR("ABTI_Task_pool_init");
        goto fn_fail;
    }
    gp_ABTI_Global->p_tasks = p_tasks;

    /* Create the primary stream */
    ABT_Stream stream;
    abt_errno = ABT_Stream_create(ABT_SCHEDULER_NULL, &stream);
    if (abt_errno != ABT_SUCCESS) {
        HANDLE_ERROR("ABT_Stream_create");
        goto fn_fail;
    }
    ABTI_Stream *p_stream = ABTI_Stream_get_ptr(stream);
    p_stream->type = ABTI_STREAM_TYPE_PRIMARY;

    /* Start the primary stream */
    abt_errno = ABTI_Stream_start(p_stream);
    if (abt_errno != ABT_SUCCESS) {
        HANDLE_ERROR("ABTI_Stream_start");
        goto fn_fail;
    }

    /* Init the ES local data */
    abt_errno = ABTI_Local_init(p_stream);
    if (abt_errno != ABT_SUCCESS) {
        HANDLE_ERROR("ABTI_Local_init");
        goto fn_fail;
    }

    /* Create the main thread */
    ABTI_Thread *p_thread;
    abt_errno = ABTI_Thread_create_main(p_stream, &p_thread);
    if (abt_errno != ABT_SUCCESS) {
        HANDLE_ERROR("ABTI_Thread_create_main");
        goto fn_fail;
    }
    ABTI_Local_set_thread(p_thread);

  fn_exit:
    return abt_errno;

  fn_fail:
    goto fn_exit;
}

int ABT_Finalize()
{
    assert(gp_ABTI_Global != NULL);
    int abt_errno = ABT_SUCCESS;

    ABTI_Stream *p_stream = ABTI_Local_get_stream();
    if (p_stream->type != ABTI_STREAM_TYPE_PRIMARY) {
        HANDLE_ERROR("ABT_Finalize must be called by the primary stream.");
        abt_errno = ABT_ERR_INV_STREAM;
        goto fn_fail;
    }

    ABTI_Thread *p_thread = ABTI_Local_get_thread();
    if (p_thread->type != ABTI_THREAD_TYPE_MAIN) {
        HANDLE_ERROR("ABT_Finalize must be called by the main stream.");
        abt_errno = ABT_ERR_INV_THREAD;
        goto fn_fail;
    }

    /* Remove the main thread */
    abt_errno = ABTI_Thread_free_main(p_thread);
    ABTI_CHECK_ERROR(abt_errno);

    /* Remove the primary stream from the global stream pool */
    abt_errno = ABTI_Global_del_stream(p_stream);
    ABTI_CHECK_ERROR(abt_errno);

    /* Finalize the ES local data */
    abt_errno = ABTI_Local_finalize();
    ABTI_CHECK_ERROR(abt_errno);

    /* Free the stream */
    abt_errno = ABTI_Stream_free(p_stream);
    ABTI_CHECK_ERROR(abt_errno);

    /* Finalize the stream pool */
    abt_errno = ABTI_Stream_pool_finalize(gp_ABTI_Global->p_streams);
    if (abt_errno != ABT_SUCCESS) {
        HANDLE_ERROR("ABTI_Stream_finalize");
        goto fn_fail;
    }
    ABTU_Free(gp_ABTI_Global->p_streams);

    /* Finalize the task pool */
    abt_errno = ABTI_Task_pool_finalize(gp_ABTI_Global->p_tasks);
    if (abt_errno != ABT_SUCCESS) {
        HANDLE_ERROR("ABTI_Task_pool_finalize");
        goto fn_fail;
    }
    ABTU_Free(gp_ABTI_Global->p_tasks);

    /* Free the ABTI_Global structure */
    ABTU_Free(gp_ABTI_Global);
    gp_ABTI_Global = NULL;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_Finalize", abt_errno);
    goto fn_exit;
}


/* Private APIs */
int ABTI_Stream_pool_init(ABTI_Stream_pool *p_streams)
{
    int abt_errno = ABT_SUCCESS;

    /* Create stream pools */
    abt_errno = ABTI_Pool_create(&p_streams->created);
    ABTI_CHECK_ERROR(abt_errno);

    abt_errno = ABTI_Pool_create(&p_streams->active);
    ABTI_CHECK_ERROR(abt_errno);

    abt_errno = ABTI_Pool_create(&p_streams->deads);
    ABTI_CHECK_ERROR(abt_errno);

    /* Initialize the mutex */
    abt_errno = ABT_Mutex_create(&p_streams->mutex);
    ABTI_CHECK_ERROR(abt_errno);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABTI_Stream_pool_init", abt_errno);
    goto fn_exit;
}

int ABTI_Stream_pool_finalize(ABTI_Stream_pool *p_streams)
{
    int abt_errno = ABT_SUCCESS;

    /* Wait until all running streams are terminated */
    while (ABTI_Pool_get_size(p_streams->active) > 0) {
        ABT_Thread_yield();
    }

    /* Free all pools */
    ABTI_Mutex_waitlock(p_streams->mutex);
    ABTI_Pool_free(&p_streams->created);
    ABTI_Pool_free(&p_streams->active);
    ABTI_Pool_free(&p_streams->deads);
    ABT_Mutex_unlock(p_streams->mutex);

    /* Destroy the mutex */
    abt_errno = ABT_Mutex_free(&p_streams->mutex);
    ABTI_CHECK_ERROR(abt_errno);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABTI_Stream_pool_finalize", abt_errno);
    goto fn_exit;
}

int ABTI_Task_pool_init(ABTI_Task_pool *p_tasks)
{
    int abt_errno = ABT_SUCCESS;

    /* Create a task pool */
    abt_errno = ABTI_Pool_create(&p_tasks->pool);
    ABTI_CHECK_ERROR(abt_errno);

    /* Initialize the mutex */
    abt_errno = ABT_Mutex_create(&p_tasks->mutex);
    ABTI_CHECK_ERROR(abt_errno);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABTI_Task_pool_init", abt_errno);
    goto fn_exit;
}

int ABTI_Task_pool_finalize(ABTI_Task_pool *p_tasks)
{
    int abt_errno = ABT_SUCCESS;

    /* Clean up the pool */
    ABTI_Mutex_waitlock(p_tasks->mutex);
    abt_errno = ABTI_Pool_free(&p_tasks->pool);
    ABT_Mutex_unlock(p_tasks->mutex);
    ABTI_CHECK_ERROR(abt_errno);

    /* Destroy the mutex */
    abt_errno = ABT_Mutex_free(&p_tasks->mutex);
    ABTI_CHECK_ERROR(abt_errno);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABTI_Task_pool_finalize", abt_errno);
    goto fn_exit;
}

int ABTI_Global_add_stream(ABTI_Stream *p_stream)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_Stream_pool *p_gstreams = gp_ABTI_Global->p_streams;

    ABTI_Mutex_waitlock(p_gstreams->mutex);
    switch (p_stream->state) {
        case ABT_STREAM_STATE_CREATED:
            ABTI_Pool_push(p_gstreams->created, p_stream->unit);
            break;
        case ABT_STREAM_STATE_READY:
        case ABT_STREAM_STATE_RUNNING:
            ABTI_Pool_push(p_gstreams->active, p_stream->unit);
            break;
        case ABT_STREAM_STATE_TERMINATED:
            ABTI_Pool_push(p_gstreams->deads, p_stream->unit);
            break;
        default:
            HANDLE_ERROR("Unknown stream type");
            abt_errno = ABT_ERR_INV_STREAM;
            break;
    }
    ABT_Mutex_unlock(p_gstreams->mutex);

    return abt_errno;
}

int ABTI_Global_move_stream(ABTI_Stream *p_stream)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_Stream_pool *p_gstreams = gp_ABTI_Global->p_streams;

    ABTI_Unit *p_unit = ABTI_Unit_get_ptr(p_stream->unit);
    ABT_Pool prev_pool = ABTI_Pool_get_handle(p_unit->p_pool);

    /* Remove from the previous pool and add to the new pool */
    ABTI_Mutex_waitlock(p_gstreams->mutex);
    ABTI_Pool_remove(prev_pool, p_stream->unit);
    switch (p_stream->state) {
        case ABT_STREAM_STATE_CREATED:
            HANDLE_ERROR("SHOULD NOT REACH HERE");
            abt_errno = ABT_ERR_STREAM;
            break;
        case ABT_STREAM_STATE_READY:
        case ABT_STREAM_STATE_RUNNING:
            ABTI_Pool_push(p_gstreams->active, p_stream->unit);
            break;
        case ABT_STREAM_STATE_TERMINATED:
            ABTI_Pool_push(p_gstreams->deads, p_stream->unit);
            break;
        default:
            HANDLE_ERROR("UNKNOWN STREAM TYPE");
            abt_errno = ABT_ERR_INV_STREAM;
            break;
    }
    ABT_Mutex_unlock(p_gstreams->mutex);

    return abt_errno;
}

int ABTI_Global_del_stream(ABTI_Stream *p_stream)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_Stream_pool *p_gstreams = gp_ABTI_Global->p_streams;

    ABTI_Unit *p_unit = ABTI_Unit_get_ptr(p_stream->unit);
    ABT_Pool prev_pool = ABTI_Pool_get_handle(p_unit->p_pool);

    ABTI_Mutex_waitlock(p_gstreams->mutex);
    ABTI_Pool_remove(prev_pool, p_stream->unit);
    ABT_Mutex_unlock(p_gstreams->mutex);

    return abt_errno;
}

int ABTI_Global_get_created_stream(ABTI_Stream **p_stream)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_Stream_pool *p_gstreams = gp_ABTI_Global->p_streams;

    /* Pop one stream */
    ABTI_Mutex_waitlock(p_gstreams->mutex);
    ABT_Unit unit = ABTI_Pool_pop(p_gstreams->created);
    ABT_Mutex_unlock(p_gstreams->mutex);

    /* Return value */
    ABT_Stream stream = ABTI_Unit_get_stream(unit);
    *p_stream = ABTI_Stream_get_ptr(stream);

    return abt_errno;
}

int ABTI_Global_add_task(ABTI_Task *p_task)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_Task_pool *p_tasks = gp_ABTI_Global->p_tasks;

    ABTI_Mutex_waitlock(p_tasks->mutex);
    ABTI_Pool_push(p_tasks->pool, p_task->unit);
    ABT_Mutex_unlock(p_tasks->mutex);

    return abt_errno;
}

int ABTI_Global_del_task(ABTI_Task *p_task)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_Task_pool *p_tasks = gp_ABTI_Global->p_tasks;

    ABTI_Mutex_waitlock(p_tasks->mutex);
    ABTI_Pool_remove(p_tasks->pool, p_task->unit);
    ABT_Mutex_unlock(p_tasks->mutex);

    return abt_errno;
}

int ABTI_Global_pop_task(ABTI_Task **p_task)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_Task_pool *p_tasks = gp_ABTI_Global->p_tasks;
    ABT_Unit unit;
    ABT_Task h_task;

    ABTI_Mutex_waitlock(p_tasks->mutex);
    unit = ABTI_Pool_pop(p_tasks->pool);
    ABT_Mutex_unlock(p_tasks->mutex);

    h_task = ABTI_Unit_get_task(unit);
    *p_task = ABTI_Task_get_ptr(h_task);

    return abt_errno;
}

int ABTI_Global_has_task(int *result)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_Task_pool *p_tasks = gp_ABTI_Global->p_tasks;
    *result = ABTI_Pool_get_size(p_tasks->pool) > 0;
    return abt_errno;
}

