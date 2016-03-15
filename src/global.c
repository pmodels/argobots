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

/* To indicate how many times ABT_init is called. */
static uint32_t g_ABTI_num_inits = 0;

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

    /* If Argobots has already been initialized, just return. */
    uint32_t num_inits = ABTD_atomic_fetch_add_uint32(&g_ABTI_num_inits, 1);
    if (num_inits > 0 || gp_ABTI_global != NULL) goto fn_exit;

    gp_ABTI_global = (ABTI_global *)ABTU_malloc(sizeof(ABTI_global));

    /* Initialize the system environment */
    ABTD_env_init(gp_ABTI_global);

    /* Initialize memory pool */
    ABTI_mem_init(gp_ABTI_global);

    /* Initialize the event environment */
    ABTI_event_init();

    /* Initialize rank and IDs. */
    ABTI_xstream_reset_rank();
    ABTI_thread_reset_id();
    ABTI_task_reset_id();
    ABTI_sched_reset_id();
    ABTI_pool_reset_id();

    /* Initialize the ES array */
    gp_ABTI_global->p_xstreams = (ABTI_xstream **)ABTU_calloc(
            gp_ABTI_global->max_xstreams, sizeof(ABTI_xstream *));
    gp_ABTI_global->num_xstreams = 0;

    /* Create a spinlock */
    ABTI_spinlock_create(&gp_ABTI_global->lock);

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

    if (gp_ABTI_global->print_config == ABT_TRUE) {
        ABT_info_print_config(stdout);
    }

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
    if (gp_ABTI_global == NULL || g_ABTI_num_inits == 0) {
        abt_errno = ABT_ERR_UNINITIALIZED;
        goto fn_exit;
    }

    uint32_t num_inits = ABTD_atomic_fetch_sub_uint32(&g_ABTI_num_inits, 1);
    if (num_inits != 1) goto fn_exit;

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

    /* Remove the primary ES from the global ES array */
    gp_ABTI_global->p_xstreams[p_xstream->rank] = NULL;
    gp_ABTI_global->num_xstreams--;

    /* Remove the primary ULT */
    ABTI_thread_free_main(p_thread);

    /* Free the primary ES */
    abt_errno = ABTI_xstream_free(p_xstream);
    ABTI_CHECK_ERROR(abt_errno);

    /* Finalize the ES local data */
    abt_errno = ABTI_local_finalize();
    ABTI_CHECK_ERROR(abt_errno);

    /* Finalize the event environment */
    ABTI_event_finalize();

    /* Free the ES array */
    ABTU_free(gp_ABTI_global->p_xstreams);

    /* Finalize the memory pool */
    ABTI_mem_finalize(gp_ABTI_global);

    /* Free the spinlock */
    ABTI_spinlock_free(&gp_ABTI_global->lock);

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

