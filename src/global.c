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
 * \c ABT_init() initializes the Argobots library and its execution environment.
 * It internally creates objects for the \a primary ES and the \a primary ULT.
 *
 * \c ABT_init() must be called by the primary ULT before using any other
 * Argobots APIs. \c ABT_init() can be called again after \c ABT_finalize() is
 * called.
 *
 * @param[in] argc the number of arguments
 * @param[in] argv the argument vector
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_init(int argc, char **argv)
{
    ABTI_UNUSED(argc); ABTI_UNUSED(argv);
    int abt_errno = ABT_SUCCESS;
    ABTI_xstream_contn *p_xstreams;

    /* If Argobots has already been initialized, just return. */
    if (gp_ABTI_global != NULL) goto fn_exit;

    gp_ABTI_global = (ABTI_global *)ABTU_malloc(sizeof(ABTI_global));

    /* Initialize the system environment */
    ABTD_env_init(gp_ABTI_global);

    /* Initialize rank and IDs. */
    ABTI_xstream_reset_rank();
    ABTI_thread_reset_id();
    ABTI_task_reset_id();
    ABTI_sched_reset_id();
    ABTI_pool_reset_id();

    /* Initialize the ES container */
    p_xstreams = (ABTI_xstream_contn *)ABTU_malloc(sizeof(ABTI_xstream_contn));
    ABTI_xstream_contn_init(p_xstreams);
    gp_ABTI_global->p_xstreams = p_xstreams;

    /* Init the ES local data */
    abt_errno = ABTI_local_init();
    ABTI_CHECK_ERROR_MSG(abt_errno, "ABTI_local_init");

    /* Create the primary ES */
    ABTI_xstream *p_newxstream;
    abt_errno = ABTI_xstream_create_primary(&p_newxstream);
    ABTI_CHECK_ERROR_MSG(abt_errno, "ABTI_xstream_create_primary");
    ABTI_local_set_xstream(p_newxstream);

    /* Create the primary ULT, i.e., the main thread */
    ABTI_thread *p_main_thread;
    abt_errno = ABTI_thread_create_main(p_newxstream, &p_main_thread);
    ABTI_CHECK_ERROR_MSG(abt_errno, "ABTI_thread_create_main");
    gp_ABTI_global->p_thread_main = p_main_thread;
    ABTI_local_set_thread(p_main_thread);

    /* Start the primary ES */
    abt_errno = ABTI_xstream_start_primary(p_newxstream, p_main_thread);
    ABTI_CHECK_ERROR_MSG(abt_errno, "ABTI_xstream_start_primary");

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

/**
 * @ingroup ENV
 * @brief   Terminate the Argobots execution environment.
 *
 * \c ABT_finalize() terminates the Argobots execution environment and
 * deallocates memory internally used in Argobots. This routine also contains
 * deallocation of objects for the primary ES and the primary ULT.
 *
 * \c ABT_finalize() must be called by the primary ULT. Invoking the Argobots
 * APIs after \c ABT_finalize() is not allowed. To use the Argobots APIs after
 * calling \c ABT_finalize(), \c ABT_init() needs to be called again.
 *
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_finalize(void)
{
    int abt_errno = ABT_SUCCESS;

    /* If Argobots is not initialized, just return. */
    if (gp_ABTI_global == NULL) {
        abt_errno = ABT_ERR_UNINITIALIZED;
        goto fn_exit;
    }

    /* If called by an external thread, return an error. */
    ABTI_CHECK_TRUE(lp_ABTI_local != NULL, ABT_ERR_INV_XSTREAM);

    ABTI_xstream *p_xstream = ABTI_local_get_xstream();
    ABTI_CHECK_TRUE_MSG(p_xstream->type == ABTI_XSTREAM_TYPE_PRIMARY,
                        ABT_ERR_INV_XSTREAM,
                        "ABT_finalize must be called by the primary ES.");

    ABTI_thread *p_thread = ABTI_local_get_thread();
    ABTI_CHECK_TRUE_MSG(p_thread->type == ABTI_THREAD_TYPE_MAIN,
                        ABT_ERR_INV_THREAD,
                        "ABT_finalize must be called by the primary ULT.");

    /* Set the join request */
    ABTI_xstream_set_request(p_xstream, ABTI_XSTREAM_REQ_JOIN);

    /* We wait for the remaining jobs */
    if (p_xstream->state != ABT_XSTREAM_STATE_TERMINATED) {
        /* Set the orphan request for the primary ULT */
        ABTI_thread_set_request(p_thread, ABTI_THREAD_REQ_ORPHAN);

        LOG_EVENT("[U%" PRIu64 ":E%" PRIu64 "] yield to scheduler\n",
                  ABTI_thread_get_id(p_thread), p_thread->p_last_xstream->rank);

        /* Switch to the top scheduler */
        ABTI_sched *p_sched = ABTI_xstream_get_top_sched(p_thread->p_last_xstream);
        ABTI_LOG_SET_SCHED(p_sched);
        ABTD_thread_context_switch(&p_thread->ctx, p_sched->p_ctx);

        /* Back to the original thread */
        LOG_EVENT("[U%" PRIu64 ":E%" PRIu64 "] resume after yield\n",
                  ABTI_thread_get_id(p_thread), p_thread->p_last_xstream->rank);
    }

    /* Remove the primary ES from the global ES container */
    ABTI_global_del_xstream(p_xstream);

    /* Finalize the ES container */
    abt_errno = ABTI_xstream_contn_finalize(gp_ABTI_global->p_xstreams);
    ABTI_CHECK_ERROR_MSG(abt_errno, "ABTI_xstream_contn_finalize");
    ABTU_free(gp_ABTI_global->p_xstreams);

    /* Remove the primary ULT */
    ABTI_thread_free_main(p_thread);

    /* Free the primary ES */
    abt_errno = ABTI_xstream_free(p_xstream);
    ABTI_CHECK_ERROR(abt_errno);

    /* Finalize the ES local data */
    abt_errno = ABTI_local_finalize();
    ABTI_CHECK_ERROR(abt_errno);

    /* Free the ABTI_global structure */
    ABTU_free(gp_ABTI_global);
    gp_ABTI_global = NULL;

    /* Free internal arrays */
    ABTI_xstream_free_ranks();

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

/**
 * @ingroup ENV
 * @brief   Check whether \c ABT_init() has been called.
 *
 * \c ABT_initialized() returns \c ABT_SUCCESS if \c ABT_init() has been called.
 * Otherwise, it returns \c ABT_ERR_UNINITIALIZED.
 *
 * @return Error code
 * @retval ABT_SUCCESS           if \c ABT_init() has been called.
 * @retval ABT_ERR_UNINITIALIZED if \c ABT_init() has not been called.
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

void ABTI_xstream_contn_init(ABTI_xstream_contn *p_xstreams)
{
    /* Create ES containers */
    ABTI_contn_create(&p_xstreams->created);
    ABTI_contn_create(&p_xstreams->active);
    ABTI_contn_create(&p_xstreams->deads);

    /* Initialize the mutex */
    ABTI_mutex_init(&p_xstreams->mutex);
}

int ABTI_xstream_contn_finalize(ABTI_xstream_contn *p_xstreams)
{
    int abt_errno = ABT_SUCCESS;

    /* Check there is no running ES anymore */
    ABTI_CHECK_TRUE(ABTI_contn_get_size(p_xstreams->active) == 0,
                    ABT_ERR_MISSING_JOIN);

    /* Free all containers */
    ABTI_mutex_spinlock(&p_xstreams->mutex);
    ABTI_contn_free(&p_xstreams->created);
    ABTI_contn_free(&p_xstreams->active);
    ABTI_contn_free(&p_xstreams->deads);
    ABTI_mutex_unlock(&p_xstreams->mutex);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

void ABTI_global_add_xstream(ABTI_xstream *p_xstream)
{
    ABTI_xstream_contn *p_gxstreams = gp_ABTI_global->p_xstreams;

    ABTI_mutex_spinlock(&p_gxstreams->mutex);
    switch (p_xstream->state) {
        case ABT_XSTREAM_STATE_CREATED:
            ABTI_contn_push(p_gxstreams->created, &p_xstream->elem);
            break;
        case ABT_XSTREAM_STATE_READY:
        case ABT_XSTREAM_STATE_RUNNING:
            ABTI_contn_push(p_gxstreams->active, &p_xstream->elem);
            break;
        case ABT_XSTREAM_STATE_TERMINATED:
            ABTI_contn_push(p_gxstreams->deads, &p_xstream->elem);
            break;
        default:
            HANDLE_ERROR("UNKNOWN ES STATE");
            break;
    }
    ABTI_mutex_unlock(&p_gxstreams->mutex);
}

void ABTI_global_move_xstream(ABTI_xstream *p_xstream)
{
    ABTI_xstream_contn *p_gxstreams = gp_ABTI_global->p_xstreams;

    ABTI_elem *p_elem = &p_xstream->elem;
    ABTI_contn *prev_contn = p_elem->p_contn;

    /* Remove from the previous container and add to the new container */
    ABTI_mutex_spinlock(&p_gxstreams->mutex);
    ABTI_contn_remove(prev_contn, &p_xstream->elem);
    switch (p_xstream->state) {
        case ABT_XSTREAM_STATE_CREATED:
            HANDLE_ERROR("SHOULD NOT REACH HERE");
            break;
        case ABT_XSTREAM_STATE_READY:
        case ABT_XSTREAM_STATE_RUNNING:
            ABTI_contn_push(p_gxstreams->active, &p_xstream->elem);
            break;
        case ABT_XSTREAM_STATE_TERMINATED:
            ABTI_contn_push(p_gxstreams->deads, &p_xstream->elem);
            break;
        default:
            HANDLE_ERROR("UNKNOWN ES STATE");
            break;
    }
    ABTI_mutex_unlock(&p_gxstreams->mutex);
}

void ABTI_global_del_xstream(ABTI_xstream *p_xstream)
{
    ABTI_xstream_contn *p_gxstreams = gp_ABTI_global->p_xstreams;

    ABTI_elem *p_elem = &p_xstream->elem;
    ABTI_contn *prev_contn = p_elem->p_contn;

    ABTI_mutex_spinlock(&p_gxstreams->mutex);
    ABTI_contn_remove(prev_contn, &p_xstream->elem);
    ABTI_mutex_unlock(&p_gxstreams->mutex);
}

void ABTI_global_get_created_xstream(ABTI_xstream **p_xstream)
{
    ABTI_xstream_contn *p_gxstreams = gp_ABTI_global->p_xstreams;

    /* Pop one ES */
    ABTI_mutex_spinlock(&p_gxstreams->mutex);
    ABTI_elem *elem = ABTI_contn_pop(p_gxstreams->created);
    ABTI_mutex_unlock(&p_gxstreams->mutex);

    /* If the list is empty */
    if (!elem)
        *p_xstream = NULL;
    else
        *p_xstream = ABTI_elem_get_xstream(elem);
}

