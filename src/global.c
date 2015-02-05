/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"

/** @defgroup ENV Init & Finalize
 * This group is for initialization and finalization of the Argobots
 * environment.
 */

/* Global Data */
ABTI_global *gp_ABTI_global = NULL;

/**
 * @ingroup ENV
 * @brief   Initialize the Argobots execution environment.
 *
 * This must be called by the main thread before using any other Argobots APIs.
 * The ES object for primary ES is created in this routine.
 *
 * @param[in] argc the number of arguments
 * @param[in] argv the argument vector
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_init(int argc, char **argv)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_xstream_pool *p_xstreams;
    ABTI_task_pool *p_tasks;

    /* If Argobots has already been initialized, just return. */
    if (gp_ABTI_global != NULL) goto fn_exit;

    gp_ABTI_global = (ABTI_global *)ABTU_malloc(sizeof(ABTI_global));
    if (!gp_ABTI_global) {
        HANDLE_ERROR("ABTU_malloc");
        abt_errno = ABT_ERR_MEM;
        goto fn_fail;
    }

    /* Initialize the system environment */
    ABTD_env_init(gp_ABTI_global);

    /* Initialize the ES pool */
    p_xstreams = (ABTI_xstream_pool *)ABTU_malloc(sizeof(ABTI_xstream_pool));
    if (!p_xstreams) {
        HANDLE_ERROR("ABTU_malloc");
        abt_errno = ABT_ERR_MEM;
        goto fn_fail;
    }
    abt_errno = ABTI_xstream_pool_init(p_xstreams);
    if (abt_errno != ABT_SUCCESS) {
        HANDLE_ERROR("ABTI_xstream_pool_init");
        goto fn_fail;
    }
    gp_ABTI_global->p_xstreams = p_xstreams;

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

    /* Create the primary ES */
    ABT_xstream xstream;
    abt_errno = ABT_xstream_create(ABT_SCHED_NULL, &xstream);
    if (abt_errno != ABT_SUCCESS) {
        HANDLE_ERROR("ABT_xstream_create");
        goto fn_fail;
    }
    ABTI_xstream *p_xstream = ABTI_xstream_get_ptr(xstream);
    p_xstream->type = ABTI_XSTREAM_TYPE_PRIMARY;

    /* Start the primary ES */
    abt_errno = ABTI_xstream_start(p_xstream);
    if (abt_errno != ABT_SUCCESS) {
        HANDLE_ERROR("ABTI_xstream_start");
        goto fn_fail;
    }

    /* Init the ES local data */
    abt_errno = ABTI_local_init(p_xstream);
    if (abt_errno != ABT_SUCCESS) {
        HANDLE_ERROR("ABTI_local_init");
        goto fn_fail;
    }

    /* Create the main thread */
    ABTI_thread *p_thread;
    abt_errno = ABTI_thread_create_main(p_xstream, &p_thread);
    if (abt_errno != ABT_SUCCESS) {
        HANDLE_ERROR("ABTI_thread_create_main");
        goto fn_fail;
    }
    ABTI_local_set_thread(p_thread);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_init", abt_errno);
    goto fn_exit;
}

/**
 * @ingroup ENV
 * @brief   Terminate the Argobots execution environment.
 *
 * This must be called by the main thread. Invoking the Argobots APIs after
 * ABT_finalize() is not allowed. This routine also contains deallocation of
 * ES object for the primary ES.
 *
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_finalize(void)
{
    int abt_errno = ABT_SUCCESS;

    /* If Argobots is not initialized, just return. */
    if (gp_ABTI_global == NULL) goto fn_exit;

    ABTI_xstream *p_xstream = ABTI_local_get_xstream();
    if (p_xstream->type != ABTI_XSTREAM_TYPE_PRIMARY) {
        HANDLE_ERROR("ABT_finalize must be called by the primary xstream.");
        abt_errno = ABT_ERR_INV_XSTREAM;
        goto fn_fail;
    }

    ABTI_thread *p_thread = ABTI_local_get_thread();
    if (p_thread->type != ABTI_THREAD_TYPE_MAIN) {
        HANDLE_ERROR("ABT_finalize must be called by the main xstream.");
        abt_errno = ABT_ERR_INV_THREAD;
        goto fn_fail;
    }

    /* Remove the main thread */
    abt_errno = ABTI_thread_free_main(p_thread);
    ABTI_CHECK_ERROR(abt_errno);

    /* Remove the primary ES from the global ES pool */
    abt_errno = ABTI_global_del_xstream(p_xstream);
    ABTI_CHECK_ERROR(abt_errno);

    /* Finalize the ES local data */
    abt_errno = ABTI_local_finalize();
    ABTI_CHECK_ERROR(abt_errno);

    /* Free the ES */
    abt_errno = ABTI_xstream_free(p_xstream);
    ABTI_CHECK_ERROR(abt_errno);

    /* Finalize the ES pool */
    abt_errno = ABTI_xstream_pool_finalize(gp_ABTI_global->p_xstreams);
    if (abt_errno != ABT_SUCCESS) {
        HANDLE_ERROR("ABTI_xstream_finalize");
        goto fn_fail;
    }
    ABTU_free(gp_ABTI_global->p_xstreams);

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

/**
 * @ingroup ENV
 * @brief   Check whether \c ABT_init has been called.
 *
 * \c ABT_initialized() returns \c ABT_SUCCESS if \c ABT_init has been called.
 * Otherwise, it returns \c ABT_ERR_UNINITIALIZED.
 *
 * @return Error code
 * @retval ABT_SUCCESS if \c ABT_init has been called.
 * @retval ABT_ERR_UNINITIALIZED if \c ABT_init has not been called.
 */
int ABT_initialized(void)
{
    int abt_errno = ABT_SUCCESS;

    if (gp_ABTI_global == NULL) {
        abt_errno = ABT_ERR_UNINITIALIZED;
    }

    return abt_errno;
}


/*****************************************************************************/
/* Private APIs                                                              */
/*****************************************************************************/

int ABTI_xstream_pool_init(ABTI_xstream_pool *p_xstreams)
{
    int abt_errno = ABT_SUCCESS;

    /* Create ES pools */
    abt_errno = ABTI_pool_create(&p_xstreams->created);
    ABTI_CHECK_ERROR(abt_errno);

    abt_errno = ABTI_pool_create(&p_xstreams->active);
    ABTI_CHECK_ERROR(abt_errno);

    abt_errno = ABTI_pool_create(&p_xstreams->deads);
    ABTI_CHECK_ERROR(abt_errno);

    /* Initialize the mutex */
    abt_errno = ABT_mutex_create(&p_xstreams->mutex);
    ABTI_CHECK_ERROR(abt_errno);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABTI_xstream_pool_init", abt_errno);
    goto fn_exit;
}

int ABTI_xstream_pool_finalize(ABTI_xstream_pool *p_xstreams)
{
    int abt_errno = ABT_SUCCESS;

    /* Wait until all running xstreams are terminated */
    while (ABTI_pool_get_size(p_xstreams->active) > 0) {
        ABT_thread_yield();
    }

    /* Free all pools */
    ABTI_mutex_waitlock(p_xstreams->mutex);
    ABTI_pool_free(&p_xstreams->created);
    ABTI_pool_free(&p_xstreams->active);
    ABTI_pool_free(&p_xstreams->deads);
    ABT_mutex_unlock(p_xstreams->mutex);

    /* Destroy the mutex */
    abt_errno = ABT_mutex_free(&p_xstreams->mutex);
    ABTI_CHECK_ERROR(abt_errno);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABTI_xstream_pool_finalize", abt_errno);
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

int ABTI_global_add_xstream(ABTI_xstream *p_xstream)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_xstream_pool *p_gxstreams = gp_ABTI_global->p_xstreams;

    ABTI_mutex_waitlock(p_gxstreams->mutex);
    switch (p_xstream->state) {
        case ABT_XSTREAM_STATE_CREATED:
            ABTI_pool_push(p_gxstreams->created, p_xstream->unit);
            break;
        case ABT_XSTREAM_STATE_READY:
        case ABT_XSTREAM_STATE_RUNNING:
            ABTI_pool_push(p_gxstreams->active, p_xstream->unit);
            break;
        case ABT_XSTREAM_STATE_TERMINATED:
            ABTI_pool_push(p_gxstreams->deads, p_xstream->unit);
            break;
        default:
            HANDLE_ERROR("Unknown xstream type");
            abt_errno = ABT_ERR_INV_XSTREAM;
            break;
    }
    ABT_mutex_unlock(p_gxstreams->mutex);

    return abt_errno;
}

int ABTI_global_move_xstream(ABTI_xstream *p_xstream)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_xstream_pool *p_gxstreams = gp_ABTI_global->p_xstreams;

    ABTI_unit *p_unit = ABTI_unit_get_ptr(p_xstream->unit);
    ABT_pool prev_pool = ABTI_pool_get_handle(p_unit->p_pool);

    /* Remove from the previous pool and add to the new pool */
    ABTI_mutex_waitlock(p_gxstreams->mutex);
    ABTI_pool_remove(prev_pool, p_xstream->unit);
    switch (p_xstream->state) {
        case ABT_XSTREAM_STATE_CREATED:
            HANDLE_ERROR("SHOULD NOT REACH HERE");
            abt_errno = ABT_ERR_XSTREAM;
            break;
        case ABT_XSTREAM_STATE_READY:
        case ABT_XSTREAM_STATE_RUNNING:
            ABTI_pool_push(p_gxstreams->active, p_xstream->unit);
            break;
        case ABT_XSTREAM_STATE_TERMINATED:
            ABTI_pool_push(p_gxstreams->deads, p_xstream->unit);
            break;
        default:
            HANDLE_ERROR("UNKNOWN XSTREAM TYPE");
            abt_errno = ABT_ERR_INV_XSTREAM;
            break;
    }
    ABT_mutex_unlock(p_gxstreams->mutex);

    return abt_errno;
}

int ABTI_global_del_xstream(ABTI_xstream *p_xstream)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_xstream_pool *p_gxstreams = gp_ABTI_global->p_xstreams;

    ABTI_unit *p_unit = ABTI_unit_get_ptr(p_xstream->unit);
    ABT_pool prev_pool = ABTI_pool_get_handle(p_unit->p_pool);

    ABTI_mutex_waitlock(p_gxstreams->mutex);
    ABTI_pool_remove(prev_pool, p_xstream->unit);
    ABT_mutex_unlock(p_gxstreams->mutex);

    return abt_errno;
}

int ABTI_global_get_created_xstream(ABTI_xstream **p_xstream)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_xstream_pool *p_gxstreams = gp_ABTI_global->p_xstreams;

    /* Pop one ES */
    ABTI_mutex_waitlock(p_gxstreams->mutex);
    ABT_unit unit = ABTI_pool_pop(p_gxstreams->created);
    ABT_mutex_unlock(p_gxstreams->mutex);

    /* Return value */
    ABT_xstream xstream = ABTI_unit_get_xstream(unit);
    *p_xstream = ABTI_xstream_get_ptr(xstream);

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

