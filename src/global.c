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
/* A global lock protecting the initialization/finalization process */
static ABTI_spinlock g_ABTI_init_lock = ABTI_SPINLOCK_STATIC_INITIALIZER();
/* A flag whether Argobots has been initialized or not */
static ABTD_atomic_uint32 g_ABTI_initialized =
    ABTD_ATOMIC_UINT32_STATIC_INITIALIZER(0);

/**
 * @ingroup ENV
 * @brief   Initialize the Argobots execution environment.
 *
 * \c ABT_init() initializes the Argobots library and its execution environment.
 * It internally creates objects for the \a primary ES and the \a primary ULT.
 *
 * \c ABT_init() must be called by the primary ULT before using any other
 * Argobots functions. \c ABT_init() can be called again after
 * \c ABT_finalize() is called.
 *
 * @param[in] argc the number of arguments
 * @param[in] argv the argument vector
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_init(int argc, char **argv)
{
    ABTI_UNUSED(argc);
    ABTI_UNUSED(argv);
    int abt_errno = ABT_SUCCESS;

    /* First, take a global lock protecting the initialization/finalization
     * process. Don't go to fn_exit before taking a lock */
    ABTI_spinlock_acquire(&g_ABTI_init_lock);

    /* If Argobots has already been initialized, just return */
    if (g_ABTI_num_inits++ > 0)
        goto fn_exit;

    gp_ABTI_global = (ABTI_global *)ABTU_malloc(sizeof(ABTI_global));

    /* Initialize the system environment */
    ABTD_env_init(gp_ABTI_global);

    /* Initialize memory pool */
    ABTI_mem_init(gp_ABTI_global);

    /* Initialize IDs */
    ABTI_thread_reset_id();
    ABTI_sched_reset_id();
    ABTI_pool_reset_id();

#ifndef ABT_CONFIG_DISABLE_TOOL_INTERFACE
    /* Initialize the tool interface */
    ABTI_spinlock_clear(&gp_ABTI_global->tool_writer_lock);
    gp_ABTI_global->tool_thread_cb_f = NULL;
    gp_ABTI_global->tool_thread_user_arg = NULL;
    ABTD_atomic_relaxed_store_uint64(&gp_ABTI_global
                                          ->tool_thread_event_mask_tagged,
                                     0);
    gp_ABTI_global->tool_task_cb_f = NULL;
    gp_ABTI_global->tool_task_user_arg = NULL;
    ABTD_atomic_relaxed_store_uint64(&gp_ABTI_global
                                          ->tool_task_event_mask_tagged,
                                     0);
#endif

    /* Initialize the ES array */
    gp_ABTI_global->p_xstreams =
        (ABTI_xstream **)ABTU_calloc(gp_ABTI_global->max_xstreams,
                                     sizeof(ABTI_xstream *));
    gp_ABTI_global->num_xstreams = 0;

    /* Initialize a spinlock */
    ABTI_spinlock_clear(&gp_ABTI_global->xstreams_lock);

    /* Create the primary ES */
    ABTI_xstream *p_local_xstream;
    abt_errno = ABTI_xstream_create_primary(&p_local_xstream);
    ABTI_CHECK_ERROR_MSG(abt_errno, "ABTI_xstream_create_primary");

    /* Init the ES local data */
    ABTI_local_set_xstream(p_local_xstream);

    /* Create the primary ULT, i.e., the main thread */
    ABTI_ythread *p_main_thread;
    abt_errno = ABTI_thread_create_main(p_local_xstream, p_local_xstream,
                                        &p_main_thread);
    /* Set as if p_local_xstream is currently running the main thread. */
    ABTD_atomic_relaxed_store_int(&p_main_thread->thread.state,
                                  ABTI_THREAD_STATE_RUNNING);
    p_main_thread->thread.p_last_xstream = p_local_xstream;
    ABTI_CHECK_ERROR_MSG(abt_errno, "ABTI_thread_create_main");
    gp_ABTI_global->p_thread_main = p_main_thread;
    p_local_xstream->p_thread = &p_main_thread->thread;

    /* Start the primary ES */
    abt_errno = ABTI_xstream_start_primary(&p_local_xstream, p_local_xstream,
                                           p_main_thread);
    ABTI_CHECK_ERROR_MSG(abt_errno, "ABTI_xstream_start_primary");

    if (gp_ABTI_global->print_config == ABT_TRUE) {
        ABTI_info_print_config(stdout);
    }
    ABTD_atomic_release_store_uint32(&g_ABTI_initialized, 1);

fn_exit:
    /* Unlock a global lock */
    ABTI_spinlock_release(&g_ABTI_init_lock);
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
 * deallocates memory internally used in Argobots. This function also contains
 * deallocation of objects for the primary ES and the primary ULT.
 *
 * \c ABT_finalize() must be called by the primary ULT. Invoking the Argobots
 * functions after \c ABT_finalize() is not allowed. To use the Argobots
 * functions after calling \c ABT_finalize(), \c ABT_init() needs to be called
 * again.
 *
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_finalize(void)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_xstream *p_local_xstream = ABTI_local_get_xstream();

    /* First, take a global lock protecting the initialization/finalization
     * process. Don't go to fn_exit before taking a lock */
    ABTI_spinlock_acquire(&g_ABTI_init_lock);

    /* If Argobots is not initialized, just return */
    if (g_ABTI_num_inits == 0) {
        abt_errno = ABT_ERR_UNINITIALIZED;
        goto fn_exit;
    }
    /* If Argobots is still referenced by others, just return */
    if (--g_ABTI_num_inits != 0)
        goto fn_exit;

    /* If called by an external thread, return an error. */
    ABTI_CHECK_TRUE(p_local_xstream != NULL, ABT_ERR_INV_XSTREAM);

    ABTI_CHECK_TRUE_MSG(p_local_xstream->type == ABTI_XSTREAM_TYPE_PRIMARY,
                        ABT_ERR_INV_XSTREAM,
                        "ABT_finalize must be called by the primary ES.");

    ABTI_thread *p_self = p_local_xstream->p_thread;
    ABTI_CHECK_TRUE_MSG(ABTI_thread_type_is_thread_main(p_self->type),
                        ABT_ERR_INV_THREAD,
                        "ABT_finalize must be called by the primary ULT.");
    ABTI_ythread *p_thread = ABTI_thread_get_ythread(p_self);

#ifndef ABT_CONFIG_DISABLE_TOOL_INTERFACE
    /* Turns off the tool interface */
    ABTI_tool_event_thread_update_callback(NULL, ABT_TOOL_EVENT_THREAD_NONE,
                                           NULL);
    ABTI_tool_event_task_update_callback(NULL, ABT_TOOL_EVENT_TASK_NONE, NULL);
#endif

    /* Set the join request */
    ABTI_xstream_set_request(p_local_xstream, ABTI_XSTREAM_REQ_JOIN);

    /* We wait for the remaining jobs */
    if (ABTD_atomic_acquire_load_int(&p_local_xstream->state) !=
        ABT_XSTREAM_STATE_TERMINATED) {
        /* Set the orphan request for the primary ULT */
        ABTI_thread_set_request(p_self, ABTI_THREAD_REQ_ORPHAN);

        LOG_DEBUG("[U%" PRIu64 ":E%d] yield to scheduler\n",
                  ABTI_thread_get_id(p_self),
                  p_thread->thread.p_last_xstream->rank);

        /* Switch to the parent */
        ABTI_thread_context_switch_to_parent(&p_local_xstream, p_thread,
                                             ABT_SYNC_EVENT_TYPE_XSTREAM_JOIN,
                                             (void *)p_local_xstream);

        /* Back to the original thread */
        LOG_DEBUG("[U%" PRIu64 ":E%d] resume after yield\n",
                  ABTI_thread_get_id(p_self),
                  p_thread->thread.p_last_xstream->rank);
    }

    /* Remove the primary ULT */
    ABTI_ASSERT(p_local_xstream->p_thread == p_self);
    p_local_xstream->p_thread = NULL;
    ABTI_thread_free_main(p_local_xstream, p_thread);

    /* Free the primary ES */
    abt_errno = ABTI_xstream_free(p_local_xstream, p_local_xstream, ABT_TRUE);
    ABTI_CHECK_ERROR(abt_errno);

    /* Finalize the ES local data */
    ABTI_local_set_xstream(NULL);

    /* Free the ES array */
    ABTU_free(gp_ABTI_global->p_xstreams);

    /* Finalize the memory pool */
    ABTI_mem_finalize(gp_ABTI_global);

    /* Restore the affinity */
    if (gp_ABTI_global->set_affinity == ABT_TRUE) {
        ABTD_affinity_finalize();
    }

    /* Free the ABTI_global structure */
    ABTU_free(gp_ABTI_global);
    gp_ABTI_global = NULL;
    ABTD_atomic_release_store_uint32(&g_ABTI_initialized, 0);

fn_exit:
    /* Unlock a global lock */
    ABTI_spinlock_release(&g_ABTI_init_lock);
    return abt_errno;

fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

/**
 * @ingroup ENV
 * @brief   Check whether \c ABT_init() has been called.
 *
 * \c ABT_initialized() returns \c ABT_SUCCESS if the Argobots execution
 * environment has been initialized. Otherwise, it returns
 * \c ABT_ERR_UNINITIALIZED.
 *
 * @return Error code
 * @retval ABT_SUCCESS           if the environment has been initialized.
 * @retval ABT_ERR_UNINITIALIZED if the environment has not been initialized.
 */
int ABT_initialized(void)
{
    int abt_errno = ABT_SUCCESS;

    if (ABTD_atomic_acquire_load_uint32(&g_ABTI_initialized) == 0) {
        abt_errno = ABT_ERR_UNINITIALIZED;
    }

    return abt_errno;
}

/* If new_size is equal to zero, we double max_xstreams.
 * NOTE: This function currently cannot decrease max_xstreams.
 */
void ABTI_global_update_max_xstreams(int new_size)
{
    int i, cur_size;

    if (new_size != 0 && new_size < gp_ABTI_global->max_xstreams)
        return;

    ABTI_spinlock_acquire(&gp_ABTI_global->xstreams_lock);

    static int max_xstreams_warning_once = 0;
    if (max_xstreams_warning_once == 0) {
        /* Because some Argobots functionalities depend on the runtime value
         * ABT_MAX_NUM_XSTREAMS (or gp_ABTI_global->max_xstreams), changing this
         * value at run-time can cause an error.  For example, using ABT_mutex
         * created before updating max_xstreams causes an error since
         * ABTI_thread_htable's array size depends on ABT_MAX_NUM_XSTREAMS.
         * To fix this issue, please set a larger number to ABT_MAX_NUM_XSTREAMS
         * in advance. */
        char *warning_message = (char *)malloc(sizeof(char) * 1024);
        snprintf(warning_message, 1024,
                 "Warning: the number of execution streams exceeds "
                 "ABT_MAX_NUM_XSTREAMS (=%d), which may cause an unexpected "
                 "error.",
                 gp_ABTI_global->max_xstreams);
        HANDLE_WARNING(warning_message);
        free(warning_message);
        max_xstreams_warning_once = 1;
    }

    cur_size = gp_ABTI_global->max_xstreams;
    new_size = (new_size > 0) ? new_size : cur_size * 2;
    gp_ABTI_global->max_xstreams = new_size;
    gp_ABTI_global->p_xstreams =
        (ABTI_xstream **)ABTU_realloc(gp_ABTI_global->p_xstreams,
                                      cur_size * sizeof(ABTI_xstream *),
                                      new_size * sizeof(ABTI_xstream *));

    for (i = gp_ABTI_global->num_xstreams; i < new_size; i++) {
        gp_ABTI_global->p_xstreams[i] = NULL;
    }

    ABTI_spinlock_release(&gp_ABTI_global->xstreams_lock);
}
