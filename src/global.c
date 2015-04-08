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

    /* Initialize the ES container */
    p_xstreams = (ABTI_xstream_contn *)ABTU_malloc(sizeof(ABTI_xstream_contn));
    abt_errno = ABTI_xstream_contn_init(p_xstreams);
    ABTI_CHECK_ERROR_MSG(abt_errno, "ABTI_xstream_contn_init");
    gp_ABTI_global->p_xstreams = p_xstreams;

    /* Init the ES local data */
    abt_errno = ABTI_local_init();
    ABTI_CHECK_ERROR_MSG(abt_errno, "ABTI_local_init");

    /* Create the primary ES */
    ABT_xstream newxstream;
    abt_errno = ABT_xstream_create(ABT_SCHED_NULL, &newxstream);
    ABTI_CHECK_ERROR_MSG(abt_errno, "ABTI_xstream_create");
    ABTI_xstream *p_newxstream = ABTI_xstream_get_ptr(newxstream);
    p_newxstream->type = ABTI_XSTREAM_TYPE_PRIMARY;
    ABTI_local_set_xstream(p_newxstream);

    /* Create the primary ULT, i.e., the main thread */
    ABTI_thread *p_main_thread;
    abt_errno = ABTI_thread_create_main(p_newxstream, &p_main_thread);
    ABTI_CHECK_ERROR_MSG(abt_errno, "ABTI_thread_create_main");
    gp_ABTI_global->p_thread_main = p_main_thread;
    ABTI_local_set_thread(p_main_thread);

    /* Start the primary ES */
    abt_errno = ABT_xstream_start(newxstream);
    ABTI_CHECK_ERROR_MSG(abt_errno, "ABT_xstream_start");

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
    if (lp_ABTI_local == NULL) {
        abt_errno = ABT_ERR_INV_XSTREAM;
        goto fn_fail;
    }

    ABTI_xstream *p_xstream = ABTI_local_get_xstream();
    if (p_xstream->type != ABTI_XSTREAM_TYPE_PRIMARY) {
        HANDLE_ERROR("ABT_finalize must be called by the primary ES.");
        abt_errno = ABT_ERR_INV_XSTREAM;
        goto fn_fail;
    }

    ABTI_thread *p_thread = ABTI_local_get_thread();
    if (p_thread->type != ABTI_THREAD_TYPE_MAIN) {
        HANDLE_ERROR("ABT_finalize must be called by the primary ULT.");
        abt_errno = ABT_ERR_INV_THREAD;
        goto fn_fail;
    }

    /* Set the join request */
    ABTD_atomic_fetch_or_uint32(&p_xstream->request, ABTI_XSTREAM_REQ_JOIN);

    /* We wait for the remaining jobs */
    while (p_xstream->state != ABT_XSTREAM_STATE_TERMINATED) {
        ABT_thread_yield();
    }

    /* Remove the primary ES from the global ES container */
    abt_errno = ABTI_global_del_xstream(p_xstream);
    ABTI_CHECK_ERROR(abt_errno);

    /* Finalize the ES container */
    abt_errno = ABTI_xstream_contn_finalize(gp_ABTI_global->p_xstreams);
    ABTI_CHECK_ERROR_MSG(abt_errno, "ABTI_xstream_contn_finalize");
    ABTU_free(gp_ABTI_global->p_xstreams);

    /* Remove the primary ULT */
    abt_errno = ABTI_thread_free_main(p_thread);
    ABTI_CHECK_ERROR(abt_errno);

    /* Free the primary ES */
    abt_errno = ABTI_xstream_free(p_xstream);
    ABTI_CHECK_ERROR(abt_errno);

    /* Finalize the ES local data */
    abt_errno = ABTI_local_finalize();
    ABTI_CHECK_ERROR(abt_errno);

    /* Free the ABTI_global structure */
    ABTU_free(gp_ABTI_global);
    gp_ABTI_global = NULL;

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

int ABTI_xstream_contn_init(ABTI_xstream_contn *p_xstreams)
{
    int abt_errno = ABT_SUCCESS;

    /* Create ES containers */
    abt_errno = ABTI_contn_create(&p_xstreams->created);
    ABTI_CHECK_ERROR(abt_errno);

    abt_errno = ABTI_contn_create(&p_xstreams->active);
    ABTI_CHECK_ERROR(abt_errno);

    abt_errno = ABTI_contn_create(&p_xstreams->deads);
    ABTI_CHECK_ERROR(abt_errno);

    /* Initialize the mutex */
    ABTI_mutex_init(&p_xstreams->mutex);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

int ABTI_xstream_contn_finalize(ABTI_xstream_contn *p_xstreams)
{
    int abt_errno = ABT_SUCCESS;

    /* Check there is no running ES anymore */
    if (ABTI_contn_get_size(p_xstreams->active) != 0) {
        abt_errno = ABT_ERR_MISSING_JOIN;
        goto fn_fail;
    }

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

int ABTI_global_add_xstream(ABTI_xstream *p_xstream)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_xstream_contn *p_gxstreams = gp_ABTI_global->p_xstreams;

    ABTI_mutex_spinlock(&p_gxstreams->mutex);
    switch (p_xstream->state) {
        case ABT_XSTREAM_STATE_CREATED:
            ABTI_contn_push(p_gxstreams->created, p_xstream->elem);
            break;
        case ABT_XSTREAM_STATE_READY:
        case ABT_XSTREAM_STATE_RUNNING:
            ABTI_contn_push(p_gxstreams->active, p_xstream->elem);
            break;
        case ABT_XSTREAM_STATE_TERMINATED:
            ABTI_contn_push(p_gxstreams->deads, p_xstream->elem);
            break;
        default:
            HANDLE_ERROR("Unknown xstream state");
            abt_errno = ABT_ERR_INV_XSTREAM;
            break;
    }
    ABTI_mutex_unlock(&p_gxstreams->mutex);

    return abt_errno;
}

int ABTI_global_move_xstream(ABTI_xstream *p_xstream)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_xstream_contn *p_gxstreams = gp_ABTI_global->p_xstreams;

    ABTI_elem *p_elem = p_xstream->elem;
    ABTI_contn *prev_contn = p_elem->p_contn;

    /* Remove from the previous container and add to the new container */
    ABTI_mutex_spinlock(&p_gxstreams->mutex);
    ABTI_contn_remove(prev_contn, p_xstream->elem);
    switch (p_xstream->state) {
        case ABT_XSTREAM_STATE_CREATED:
            HANDLE_ERROR("SHOULD NOT REACH HERE");
            abt_errno = ABT_ERR_XSTREAM;
            break;
        case ABT_XSTREAM_STATE_READY:
        case ABT_XSTREAM_STATE_RUNNING:
            ABTI_contn_push(p_gxstreams->active, p_xstream->elem);
            break;
        case ABT_XSTREAM_STATE_TERMINATED:
            ABTI_contn_push(p_gxstreams->deads, p_xstream->elem);
            break;
        default:
            HANDLE_ERROR("UNKNOWN XSTREAM STATE");
            abt_errno = ABT_ERR_INV_XSTREAM;
            break;
    }
    ABTI_mutex_unlock(&p_gxstreams->mutex);

    return abt_errno;
}

int ABTI_global_del_xstream(ABTI_xstream *p_xstream)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_xstream_contn *p_gxstreams = gp_ABTI_global->p_xstreams;

    ABTI_elem *p_elem = p_xstream->elem;
    ABTI_contn *prev_contn = p_elem->p_contn;

    ABTI_mutex_spinlock(&p_gxstreams->mutex);
    ABTI_contn_remove(prev_contn, p_xstream->elem);
    ABTI_mutex_unlock(&p_gxstreams->mutex);

    return abt_errno;
}

int ABTI_global_get_created_xstream(ABTI_xstream **p_xstream)
{
    int abt_errno = ABT_SUCCESS;
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

    return abt_errno;
}

