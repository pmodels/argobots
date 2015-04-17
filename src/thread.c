/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"

static inline ABT_thread_id ABTI_thread_get_new_id(void);


/** @defgroup ULT User-level Thread (ULT)
 * This group is for User-level Thread (ULT).
 */

/**
 * @ingroup ULT
 * @brief   Create a new thread and return its handle through newthread.
 *
 * \c ABT_thread_create() creates a new ULT that is pushed into \c pool. The
 * insertion is done from the ES where this call is made. Therefore, the access
 * type of \c pool should comply with that. Only a \a secondary ULT can be
 * created explicitly, and the \a primary ULT is created automatically.
 *
 * If newthread is NULL, the thread object will be automatically released when
 * this \a unnamed thread completes the execution of thread_func. Otherwise,
 * ABT_thread_free() can be used to explicitly release the thread object.
 *
 * @param[in]  pool         handle to the associated pool
 * @param[in]  thread_func  function to be executed by a new thread
 * @param[in]  arg          argument for thread_func
 * @param[in]  attr         thread attribute. If it is ABT_THREAD_ATTR_NULL,
 *                          the default attribute is used.
 * @param[out] newthread    handle to a newly created thread
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_thread_create(ABT_pool pool, void(*thread_func)(void *),
                      void *arg, ABT_thread_attr attr,
                      ABT_thread *newthread)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_thread *p_newthread;
    ABT_thread h_newthread;
    size_t stacksize;

    ABTI_pool *p_pool = ABTI_pool_get_ptr(pool);
    ABTI_CHECK_NULL_POOL_PTR(p_pool);

    /* If the pool is directly associated to a ES and this ES is not running,
     * then start it */
    ABTI_xstream *p_xstream = p_pool->consumer;
    if (p_xstream && p_xstream->state == ABT_XSTREAM_STATE_CREATED) {
        ABT_xstream xstream = ABTI_xstream_get_handle(p_xstream);
        abt_errno = ABT_xstream_start(xstream);
        ABTI_CHECK_ERROR_MSG(abt_errno, "ABT_xstream_start");
    }

    p_newthread = (ABTI_thread *)ABTU_malloc(sizeof(ABTI_thread));

    /* Set attributes */
    ABTI_thread_set_attr(p_newthread, attr);

    /* Create a stack for this thread */
    stacksize = p_newthread->attr.stacksize;
    p_newthread->p_stack = ABTU_malloc(stacksize);

    /* Create a thread context */
    abt_errno = ABTD_thread_context_create(NULL,
            thread_func, arg, stacksize, p_newthread->p_stack,
            &p_newthread->ctx);
    ABTI_CHECK_ERROR(abt_errno);

    /* Initialize the mutex */
    ABTI_mutex_init(&p_newthread->mutex);

    p_newthread->p_last_xstream = NULL;
    p_newthread->is_sched       = NULL;
    p_newthread->p_pool         = p_pool;
    p_newthread->type           = ABTI_THREAD_TYPE_USER;
    p_newthread->state          = ABT_THREAD_STATE_READY;
    p_newthread->refcount       = (newthread != NULL) ? 1 : 0;
    p_newthread->request        = 0;
    p_newthread->p_req_arg      = NULL;
    p_newthread->id             = ABTI_THREAD_INIT_ID;
    p_newthread->p_name         = NULL;

    /* Create a wrapper unit */
    h_newthread = ABTI_thread_get_handle(p_newthread);
    p_newthread->unit = p_pool->u_create_from_thread(h_newthread);

    /* Add this thread to the pool */
    abt_errno = ABTI_pool_push(p_pool, p_newthread->unit, ABTI_xstream_self());
    if (abt_errno != ABT_SUCCESS) {
        p_newthread->state = ABT_THREAD_STATE_CREATED;
        int ret = ABT_thread_free(&h_newthread);
        ABTI_CHECK_TRUE(ret == ABT_SUCCESS, ret);
        goto fn_fail;
    }

    /* Return value */
    if (newthread) *newthread = h_newthread;

  fn_exit:
    return abt_errno;

  fn_fail:
    if (newthread) *newthread = ABT_THREAD_NULL;
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

/**
 * @ingroup ULT
 * @brief   Create a new ULT associated with the target ES (\c xstream).
 *
 * \c ABT_thread_create_on_xstream() creates a new ULT associated with the
 * target ES and returns its handle through \c newthread. The new ULT will be
 * inserted into a proper pool associated with the main scheduler of the target
 * ES.
 *
 * This routine is only for convenience. If the user wants to focus on the
 * performance, we recommend to use \c ABT_thread_create() with directly
 * dealing with pools. Pools are a right way to manage work units in Argobots.
 * ES is just an abstract, and it is not a mechanism for execution and
 * performance tuning.
 *
 * If \c attr is \c ABT_THREAD_ATTR_NULL, a new ULT is created with default
 * attributes. For example, the stack size of default attribute is 16KB.
 * If the attribute is specified, attribute values are saved in the ULT object.
 * After creating the ULT object, changes in the attribute object will not
 * affect attributes of the ULT object. A new attribute object can be created
 * with \c ABT_thread_attr_create().
 *
 * If \c newthread is \c NULL, this routine creates an unnamed ULT. The object
 * for unnamed ULT will be automatically freed when the unnamed ULT completes
 * its execution. Otherwise, this routine creates a named ULT and
 * \c ABT_thread_free() can be used to explicitly free the object for
 * the named ULT.
 *
 * If \c newthread is not \c NULL and an error occurs in this routine,
 * a non-zero error code will be returned and \c newthread will be set to
 * \c ABT_THREAD_NULL.
 *
 * @param[in]  xstream      handle to the target ES
 * @param[in]  thread_func  function to be executed by a new ULT
 * @param[in]  arg          argument for <tt>thread_func</tt>
 * @param[in]  attr         ULT attribute
 * @param[out] newthread    handle to a newly created ULT
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_thread_create_on_xstream(ABT_xstream xstream,
                      void (*thread_func)(void *), void *arg,
                      ABT_thread_attr attr, ABT_thread *newthread)
{
    int abt_errno = ABT_SUCCESS;
    ABT_pool pool;

    /* TODO: need to consider the access type of target pool */
    abt_errno = ABT_xstream_get_main_pools(xstream, 1, &pool);
    ABTI_CHECK_ERROR(abt_errno);

    abt_errno = ABT_thread_create(pool, thread_func, arg, attr, newthread);
    ABTI_CHECK_ERROR(abt_errno);

  fn_exit:
    return abt_errno;

  fn_fail:
    if (newthread) *newthread = ABT_THREAD_NULL;
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

/**
 * @ingroup ULT
 * @brief   Release the thread object associated with thread handle.
 *
 * This routine deallocates memory used for the thread object. If the thread
 * is still running when this routine is called, the deallocation happens
 * after the thread terminates and then this routine returns. If it is
 * successfully processed, thread is set as ABT_THREAD_NULL.
 *
 * @param[in,out] thread  handle to the target thread
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_thread_free(ABT_thread *thread)
{
    int abt_errno = ABT_SUCCESS;
    ABT_thread h_thread = *thread;
    if (h_thread == ABT_THREAD_NULL) goto fn_exit;

    ABTI_thread *p_thread = ABTI_thread_get_ptr(h_thread);
    ABTI_CHECK_NULL_THREAD_PTR(p_thread);

    /* We first need to check whether lp_ABTI_local is NULL because external
     * threads might call this routine. */
    ABTI_CHECK_TRUE_MSG(lp_ABTI_local == NULL ||
                          p_thread != ABTI_local_get_thread(),
                        ABT_ERR_INV_THREAD,
                        "The current thread cannot be freed.");

    ABTI_CHECK_TRUE_MSG(p_thread->type != ABTI_THREAD_TYPE_MAIN &&
                          p_thread->type != ABTI_THREAD_TYPE_MAIN_SCHED,
                        ABT_ERR_INV_THREAD,
                        "The main thread cannot be freed explicitly.");

    /* Wait until the thread terminates */
    while (p_thread->state != ABT_THREAD_STATE_TERMINATED &&
           p_thread->state != ABT_THREAD_STATE_CREATED) {
        ABT_thread_yield();
    }

    if (p_thread->refcount > 0 &&
        p_thread->state != ABT_THREAD_STATE_CREATED) {
        /* The thread has finished but it is still referenced.
         * Thus it exists in the xstream's deads pool. */
        ABTI_xstream *p_xstream = p_thread->p_last_xstream;
        ABTI_mutex_spinlock(&p_xstream->mutex);
        ABTI_contn_remove(p_xstream->deads, p_thread->unit);
        ABTI_mutex_unlock(&p_xstream->mutex);
    }

    /* Free the ABTI_thread structure */
    abt_errno = ABTI_thread_free(p_thread);
    ABTI_CHECK_ERROR(abt_errno);

    /* Return value */
    *thread = ABT_THREAD_NULL;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

/**
 * @ingroup ULT
 * @brief   Wait for thread to terminate.
 *
 * The target thread cannot be the same as the calling thread.
 *
 * @param[in] thread  handle to the target thread
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_thread_join(ABT_thread thread)
{
    int abt_errno = ABT_SUCCESS;
    ABT_unit_type type;
    ABTI_thread *p_thread = ABTI_thread_get_ptr(thread);
    ABTI_CHECK_NULL_THREAD_PTR(p_thread);

    ABTI_CHECK_TRUE_MSG(p_thread->type != ABTI_THREAD_TYPE_MAIN &&
                          p_thread->type != ABTI_THREAD_TYPE_MAIN_SCHED,
                        ABT_ERR_INV_THREAD,
                        "The main ULT cannot be joined.");

    ABT_self_get_type(&type);

    /* If the caller is ULT, we can use yield_to-based implementation. */
    if (type == ABT_UNIT_TYPE_THREAD) {
        ABTI_thread *p_caller = ABTI_local_get_thread();

        ABTI_CHECK_TRUE_MSG(p_thread != p_caller,
                            ABT_ERR_INV_THREAD,
                            "The target ULT should be different.");

        while (p_thread->state != ABT_THREAD_STATE_TERMINATED) {
            /* FIXME: instead of same pool, runnable by the same ES */
            if (p_thread->p_pool == p_caller->p_pool &&
                ABTI_thread_is_ready(p_thread) == ABT_TRUE) {
                ABT_thread_yield_to(thread);
            }
            else
                ABT_thread_yield();
        }
    } else {
        while (p_thread->state != ABT_THREAD_STATE_TERMINATED) {
            ABT_thread_yield();
        }
    }

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

/**
 * @ingroup ULT
 * @brief   The calling ULT terminates its execution.
 *
 * Since the calling ULT terminates, this routine never returns.
 *
 * @return Error code
 * @retval ABT_SUCCESS           on success
 * @retval ABT_ERR_UNINITIALIZED Argobots has not been initialized
 * @retval ABT_ERR_INV_XSTREAM   called by an external thread, e.g., pthread
 */
int ABT_thread_exit(void)
{
    int abt_errno = ABT_SUCCESS;

    /* In case that Argobots has not been initialized or this routine is called
     * by an external thread, e.g., pthread, return an error code instead of
     * making the call fail. */
    if (gp_ABTI_global == NULL) {
        abt_errno = ABT_ERR_UNINITIALIZED;
        goto fn_exit;
    }
    if (lp_ABTI_local == NULL) {
        abt_errno = ABT_ERR_INV_XSTREAM;
        goto fn_exit;
    }

    ABTI_thread *p_thread = ABTI_local_get_thread();
    ABTI_CHECK_NULL_THREAD_PTR(p_thread);

    /* Set the exit request */
    ABTD_atomic_fetch_or_uint32(&p_thread->request, ABTI_THREAD_REQ_EXIT);

    /* Switch the context to the scheduler */
    ABT_thread_yield();

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

/**
 * @ingroup ULT
 * @brief   Request the cancelation of the target thread.
 *
 * @param[in] thread  handle to the target thread
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_thread_cancel(ABT_thread thread)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_thread *p_thread = ABTI_thread_get_ptr(thread);
    ABTI_CHECK_NULL_THREAD_PTR(p_thread);

    ABTI_CHECK_TRUE_MSG(p_thread->type != ABTI_THREAD_TYPE_MAIN &&
                          p_thread->type != ABTI_THREAD_TYPE_MAIN_SCHED,
                        ABT_ERR_INV_THREAD,
                        "The main thread cannot be canceled.");

    /* Set the cancel request */
    ABTD_atomic_fetch_or_uint32(&p_thread->request, ABTI_THREAD_REQ_CANCEL);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

/**
 * @ingroup ULT
 * @brief   Return the handle of the calling ULT.
 *
 * \c ABT_thread_self() returns the handle of the calling ULT. Both the primary
 * ULT and secondary ULTs can get their handle through this routine.
 * If tasklets call this routine, \c ABT_THREAD_NULL will be returned to
 * \c thread.
 *
 * @param[out] thread  ULT handle
 * @return Error code
 * @retval ABT_SUCCESS           on success
 * @retval ABT_ERR_UNINITIALIZED Argobots has not been initialized
 * @retval ABT_ERR_INV_XSTREAM   called by an external thread, e.g., pthread
 * @retval ABT_ERR_INV_THREAD    called by a tasklet
 */
int ABT_thread_self(ABT_thread *thread)
{
    int abt_errno = ABT_SUCCESS;

    /* In case that Argobots has not been initialized or this routine is called
     * by an external thread, e.g., pthread, return an error code instead of
     * making the call fail. */
    if (gp_ABTI_global == NULL) {
        abt_errno = ABT_ERR_UNINITIALIZED;
        *thread = ABT_THREAD_NULL;
        goto fn_exit;
    }
    if (lp_ABTI_local == NULL) {
        abt_errno = ABT_ERR_INV_XSTREAM;
        *thread = ABT_THREAD_NULL;
        goto fn_exit;
    }

    ABTI_thread *p_thread = ABTI_local_get_thread();
    if (p_thread != NULL) {
        ABTI_thread_retain(p_thread);
        *thread = ABTI_thread_get_handle(p_thread);
    } else {
        abt_errno = ABT_ERR_INV_THREAD;
        *thread = ABT_THREAD_NULL;
    }

  fn_exit:
    return abt_errno;
}

/**
 * @ingroup ULT
 * @brief   Return the state of thread.
 *
 * @param[in]  thread  handle to the target thread
 * @param[out] state   the thread's state
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_thread_get_state(ABT_thread thread, ABT_thread_state *state)
{
    int abt_errno = ABT_SUCCESS;

    ABTI_thread *p_thread = ABTI_thread_get_ptr(thread);
    ABTI_CHECK_NULL_THREAD_PTR(p_thread);

    /* Return value */
    *state = p_thread->state;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

/**
 * @ingroup ULT
 * @brief   Return the last pool of ULT.
 *
 * If the ULT is not running, we get the pool where it is, else we get the
 * last pool where it was (the pool from the thread was poped).
 *
 * @param[in]  thread handle to the target ULT
 * @param[out] pool   the last pool of the ULT
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_thread_get_last_pool(ABT_thread thread, ABT_pool *pool)
{
    int abt_errno = ABT_SUCCESS;

    ABTI_thread *p_thread = ABTI_thread_get_ptr(thread);
    ABTI_CHECK_NULL_THREAD_PTR(p_thread);

    /* Return value */
    *pool = ABTI_pool_get_handle(p_thread->p_pool);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}


/**
 * @ingroup ULT
 * @brief   Yield the processor from the current running thread to the
 *          specific thread.
 *
 * This function can be used for users to explicitly schedule the next thread
 * to execute.
 *
 * @param[in] thread  handle to the target thread
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_thread_yield_to(ABT_thread thread)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_thread *p_cur_thread = NULL;

    /* If this routine is called by non-ULT, just return. */
    if (lp_ABTI_local != NULL) {
        p_cur_thread = ABTI_local_get_thread();
    }
    if (p_cur_thread == NULL) goto fn_exit;

    ABTI_xstream *p_xstream = ABTI_local_get_xstream();
    ABTI_thread *p_tar_thread = ABTI_thread_get_ptr(thread);
    ABTI_CHECK_NULL_THREAD_PTR(p_tar_thread);
    DEBUG_PRINT("YIELD_TO: TH%" PRIu64 " -> TH%" PRIu64 "\n",
                ABTI_thread_get_id(p_cur_thread),
                ABTI_thread_get_id(p_tar_thread));

    /* If the target thread is the same as the running thread, just keep
     * its execution. */
    if (p_cur_thread == p_tar_thread) goto fn_exit;

    ABTI_CHECK_TRUE_MSG(p_tar_thread->state != ABT_THREAD_STATE_TERMINATED,
                        ABT_ERR_INV_THREAD,
                        "Cannot yield to the terminated thread");

    /* Both threads must be associated with the same pool. */
    /* FIXME: instead of same pool, runnable by the same ES */
    ABTI_CHECK_TRUE_MSG(p_cur_thread->p_pool == p_tar_thread->p_pool,
                        ABT_ERR_INV_THREAD,
                        "The target thread's pool is not the same as mine.");

    /* If the target thread is not in READY, we don't yield. */
    if (ABTI_thread_is_ready(p_tar_thread) == ABT_FALSE) {
        goto fn_exit;
    }

    if (p_cur_thread->type == ABTI_THREAD_TYPE_MAIN ||
            p_cur_thread->type == ABTI_THREAD_TYPE_MAIN_SCHED) {
        /* Remove the target ULT from the pool */
        ABTI_pool_remove(p_tar_thread->p_pool, p_tar_thread->unit, p_xstream);

        abt_errno = ABTI_xstream_schedule_thread(p_xstream, p_tar_thread);
        ABTI_CHECK_ERROR(abt_errno);

        /* Yield to another thread to execute all threads */
        if (p_cur_thread->type == ABTI_THREAD_TYPE_MAIN)
            ABT_thread_yield();

    } else {
        p_cur_thread->state = ABT_THREAD_STATE_READY;

        /* Add the current thread to the pool again */
        abt_errno = ABTI_pool_push(p_cur_thread->p_pool, p_cur_thread->unit,
                                   p_xstream);
        ABTI_CHECK_ERROR(abt_errno);

        /* Delete the last context if the ULT is a scheduler */
        if (p_cur_thread->is_sched != NULL) {
            ABTI_xstream_pop_sched(p_xstream);
            p_cur_thread->is_sched->state = ABT_SCHED_STATE_STOPPED;
        }

        /* Remove the target ULT from the pool */
        ABTI_pool_remove(p_tar_thread->p_pool, p_tar_thread->unit, p_xstream);

        /* Add a new scheduler if the ULT is a scheduler */
        if (p_tar_thread->is_sched != NULL) {
            p_tar_thread->is_sched->p_ctx = ABTI_xstream_get_sched_ctx(p_xstream);
            ABTI_xstream_push_sched(p_xstream, p_tar_thread->is_sched);
            p_tar_thread->is_sched->state = ABT_SCHED_STATE_RUNNING;
        }

        /* We set the link in the context for the target thread */
        ABTD_thread_context *p_ctx = ABTI_xstream_get_sched_ctx(p_xstream);
        ABTD_thread_context_change_link(&p_tar_thread->ctx, p_ctx);

        /* We set the last ES */
        p_tar_thread->p_last_xstream = p_xstream;

        /* Switch the context */
        ABTI_local_set_thread(p_tar_thread);
        p_tar_thread->state = ABT_THREAD_STATE_RUNNING;
        ABTD_thread_context_switch(&p_cur_thread->ctx, &p_tar_thread->ctx);

        ABTI_local_set_thread(p_cur_thread);
    }

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

/**
 * @ingroup ULT
 * @brief   Yield the processor from the current running ULT back to the
 *          scheduler.
 *
 * The ULT that yields, goes back to its pool, and eventually will be
 * resumed automatically later.
 *
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_thread_yield(void)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_thread *p_thread = NULL;

    /* If this routine is called by non-ULT, just return. */
    if (lp_ABTI_local != NULL) {
        p_thread = ABTI_local_get_thread();
    }
    if (p_thread == NULL) goto fn_exit;

    ABTI_CHECK_TRUE(p_thread->p_last_xstream == ABTI_local_get_xstream(),
                    ABT_ERR_THREAD);

    /* Change the state of current running thread */
    p_thread->state = ABT_THREAD_STATE_READY;

    /* Switch to the top scheduler */
    ABTD_thread_context_switch(&p_thread->ctx,
        ABTI_xstream_get_sched_ctx(p_thread->p_last_xstream));

    /* Back to the original thread */
    p_thread->state = ABT_THREAD_STATE_RUNNING;
    ABTI_local_set_thread(p_thread);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

/**
 * @ingroup ULT
 * @brief   Migrate a thread to a specific ES.
 *
 * The actual migration occurs asynchronously with this function call.
 * In other words, this function may return immediately without the thread
 * being migrated. The migration request will be posted on the thread, such that
 * next time a scheduler picks it up, migration will happen.
 * The target pool is chosen by the running scheduler of the target ES.
 * The migration will fail if the running scheduler has no pool available for
 * migration.
 *
 *
 * @param[in] thread   handle to the thread to migrate
 * @param[in] xstream  handle to the ES to migrate the thread to
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_thread_migrate_to_xstream(ABT_thread thread, ABT_xstream xstream)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_thread *p_thread = ABTI_thread_get_ptr(thread);
    ABTI_CHECK_NULL_THREAD_PTR(p_thread);
    ABTI_xstream *p_xstream = ABTI_xstream_get_ptr(xstream);
    ABTI_CHECK_NULL_XSTREAM_PTR(p_xstream);

    /* checking for cases when migration is not allowed */
    ABTI_CHECK_TRUE(p_xstream->state != ABT_XSTREAM_STATE_TERMINATED,
                    ABT_ERR_INV_XSTREAM);
    ABTI_CHECK_TRUE(p_thread->type != ABTI_THREAD_TYPE_MAIN &&
                      p_thread->type != ABTI_THREAD_TYPE_MAIN_SCHED,
                    ABT_ERR_INV_THREAD);
    ABTI_CHECK_TRUE(p_thread->state != ABT_THREAD_STATE_TERMINATED,
                    ABT_ERR_INV_THREAD);

    /* We need to find the target scheduler */
    ABTI_pool *p_pool = NULL;
    ABTI_sched *p_sched = NULL;
    do {
        ABTI_mutex_spinlock(&p_xstream->top_sched_mutex);

        /* We check the state of the ES */
        if (p_xstream->state == ABT_XSTREAM_STATE_TERMINATED) {
            abt_errno = ABT_ERR_INV_XSTREAM;
            ABTI_mutex_unlock(&p_xstream->top_sched_mutex);
            goto fn_fail;

        } else if (p_xstream->state == ABT_XSTREAM_STATE_RUNNING) {
            p_sched = ABTI_xstream_get_top_sched(p_xstream);

        } else {
            p_sched = p_xstream->p_main_sched;
        }

        /* We check the state of the sched */
        if (p_sched->state == ABT_SCHED_STATE_TERMINATED) {
            abt_errno = ABT_ERR_INV_XSTREAM;
            ABTI_mutex_unlock(&p_xstream->top_sched_mutex);
            goto fn_fail;
        } else {
            /* Find a pool */
            ABTI_sched_get_migration_pool(p_sched, p_thread->p_pool, &p_pool);
            if (p_pool == NULL) {
                abt_errno = ABT_ERR_INV_POOL;
                ABTI_mutex_unlock(&p_xstream->top_sched_mutex);
                goto fn_fail;
            }
            /* We set the migration counter to prevent the scheduler from
             * stopping */
            ABTI_pool_inc_num_migrations(p_pool);
        }
        ABTI_mutex_unlock(&p_xstream->top_sched_mutex);
    } while (p_pool == NULL);

    abt_errno = ABTI_thread_migrate_to_pool(p_thread, p_pool);
    if (abt_errno != ABT_SUCCESS) {
        ABTI_pool_dec_num_migrations(p_pool);
        goto fn_fail;
    }

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

/**
 * @ingroup ULT
 * @brief   Migrate a thread to a specific scheduler.
 *
 * The actual migration occurs asynchronously with this function call.
 * In other words, this function may return immediately without the thread
 * being migrated. The migration request will be posted on the thread, such that
 * next time a scheduler picks it up, migration will happen.
 * The target pool is chosen by the scheduler itself.
 * The migration will fail if the target scheduler has no pool available for
 * migration.
 *
 *
 * @param[in] thread handle to the thread to migrate
 * @param[in] sched  handle to the sched to migrate the thread to
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_thread_migrate_to_sched(ABT_thread thread, ABT_sched sched)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_thread *p_thread = ABTI_thread_get_ptr(thread);
    ABTI_CHECK_NULL_THREAD_PTR(p_thread);
    ABTI_sched *p_sched = ABTI_sched_get_ptr(sched);
    ABTI_CHECK_NULL_SCHED_PTR(p_sched);

    /* checking for cases when migration is not allowed */
    ABTI_CHECK_TRUE(p_sched->state == ABT_SCHED_STATE_RUNNING,
                    ABT_ERR_INV_XSTREAM);
    ABTI_CHECK_TRUE(p_thread->type != ABTI_THREAD_TYPE_MAIN &&
                      p_thread->type != ABTI_THREAD_TYPE_MAIN_SCHED,
                    ABT_ERR_INV_THREAD);
    ABTI_CHECK_TRUE(p_thread->state != ABT_THREAD_STATE_TERMINATED,
                    ABT_ERR_INV_THREAD);

    /* Find a pool */
    ABTI_pool *p_pool;
    ABTI_sched_get_migration_pool(p_sched, p_thread->p_pool, &p_pool);
    ABTI_CHECK_NULL_POOL_PTR(p_pool);

    abt_errno = ABTI_thread_migrate_to_pool(p_thread, p_pool);
    ABTI_CHECK_ERROR(abt_errno);

    ABTI_pool_inc_num_migrations(p_pool);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

/**
 * @ingroup ULT
 * @brief   Migrate a thread to a specific pool.
 *
 * The actual migration occurs asynchronously with this function call.
 * In other words, this function may return immediately without the thread
 * being migrated. The migration request will be posted on the thread, such that
 * next time a scheduler picks it up, migration will happen.
 *
 * @param[in] thread handle to the thread to migrate
 * @param[in] pool   handle to the pool to migrate the thread to
 * @return Error code
 * @retval ABT_SUCCESS              on success
 * @retval ABT_ERR_MIGRATION_TARGET the same pool is used
 */
int ABT_thread_migrate_to_pool(ABT_thread thread, ABT_pool pool)
{
    int abt_errno;
    ABTI_thread *p_thread = ABTI_thread_get_ptr(thread);
    ABTI_CHECK_NULL_THREAD_PTR(p_thread);
    ABTI_pool *p_pool = ABTI_pool_get_ptr(pool);
    ABTI_CHECK_NULL_POOL_PTR(p_pool);

    abt_errno = ABTI_thread_migrate_to_pool(p_thread, p_pool);
    ABTI_CHECK_ERROR(abt_errno);

    ABTI_pool_inc_num_migrations(p_pool);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

/**
 * @ingroup ULT
 * @brief   Request migration of the thread to an any available ES.
 *
 * ABT_thread_migrate requests migration of the thread but does not specify
 * the target ES. The target ES will be determined among available ESs by the
 * runtime. Other semantics of this routine are the same as those of
 * \c ABT_thread_migrate_to_xstream()
 *
 * NOTE: This function may have some bugs.
 *
 * @param[in] thread  handle to the thread
 * @return Error code
 * @retval ABT_SUCCESS          on success
 * @retval ABT_ERR_MIGRATION_NA no other available ES for migration
 */
int ABT_thread_migrate(ABT_thread thread)
{
    /* TODO: fix the bug(s) */
    int abt_errno = ABT_SUCCESS;
    ABT_xstream xstream;

    ABTI_xstream_contn *p_xstreams = gp_ABTI_global->p_xstreams;

    /* Choose the destination xstream */
    /* FIXME: Currenlty, the target xstream is randomly chosen. We need a
     * better selection strategy. */
    /* TODO: handle better when no pool accepts migration */
    /* TODO: choose a pool also when (p_thread->p_pool->consumer == NULL) */
    while (1) {
        /* Only one ES */
        if (ABTI_contn_get_size(p_xstreams->created) +
            ABTI_contn_get_size(p_xstreams->active) == 1) {
            abt_errno = ABT_ERR_MIGRATION_NA;
            break;
        }
        if (ABTI_contn_get_size(p_xstreams->created) > 0) {
            ABTI_xstream *p_xstream;
            abt_errno = ABTI_global_get_created_xstream(&p_xstream);
            ABTI_CHECK_ERROR(abt_errno);
            if (p_xstream == NULL) continue;
            xstream = ABTI_xstream_get_handle(p_xstream);
        } else {
            ABTI_xstream *p_xstream;
            ABTI_thread *p_thread = ABTI_thread_get_ptr(thread);
            /* If the pool is not associated with an ES */
            ABTI_CHECK_TRUE(p_thread->p_pool->consumer != NULL,
                            ABT_ERR_INV_POOL_ACCESS);
            ABTI_elem *p_next =
                ABTI_elem_get_next(&p_thread->p_pool->consumer->elem);
            p_xstream = ABTI_elem_get_xstream(p_next);
            xstream = ABTI_xstream_get_handle(p_xstream);
        }

        abt_errno = ABT_thread_migrate_to_xstream(thread, xstream);
        if (abt_errno != ABT_ERR_INV_XSTREAM &&
            abt_errno != ABT_ERR_MIGRATION_TARGET) {
            ABTI_CHECK_ERROR(abt_errno);
            break;
        }
    }

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

/**
 * @ingroup ULT
 * @brief   Set the callback function.
 *
 * \c ABT_thread_set_callback sets the callback function to be used when the
 * ULT is migrated.
 *
 * @param[in] thread   handle to the target ULT
 * @param[in] cb_func  callback function pointer
 * @param[in] cb_arg   argument for the callback function
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_thread_set_callback(ABT_thread thread,
                            void(*cb_func)(ABT_thread thread, void *cb_arg),
                            void *cb_arg)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_thread *p_thread = ABTI_thread_get_ptr(thread);
    ABTI_CHECK_NULL_THREAD_PTR(p_thread);

    p_thread->attr.f_cb     = cb_func;
    p_thread->attr.p_cb_arg = cb_arg;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

/**
 * @ingroup ULT
 * @brief   Set the ULT's migratability.
 *
 * \c ABT_thread_set_migratable sets the secondary ULT's migratability. This
 * routine cannot be used for the primary ULT. If \c flag is \c ABT_TRUE, the
 * target ULT becomes migratable. On the other hand, if \c flag is \c
 * ABT_FALSE, the target ULT becomes unmigratable.
 *
 * @param[in] thread  handle to the target ULT
 * @param[in] flag    migratability flag (<tt>ABT_TRUE</tt>: migratable,
 *                    <tt>ABT_FALSE</tt>: not)
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_thread_set_migratable(ABT_thread thread, ABT_bool flag)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_thread *p_thread = ABTI_thread_get_ptr(thread);
    ABTI_CHECK_NULL_THREAD_PTR(p_thread);

    if (p_thread->type == ABTI_THREAD_TYPE_USER) {
        p_thread->attr.migratable = flag;
    }

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

/**
 * @ingroup ULT
 * @brief   Get the ULT's migratability.
 *
 * \c ABT_thread_is_migratable returns the ULT's migratability through
 * \c flag. If the target ULT is migratable, \c ABT_TRUE is returned to
 * \c flag. Otherwise, \c flag is set to \c ABT_FALSE.
 *
 * @param[in]  thread  handle to the target ULT
 * @param[out] flag    migratability flag (<tt>ABT_TRUE</tt>: migratable,
 *                     <tt>ABT_FALSE</tt>: not)
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_thread_is_migratable(ABT_thread thread, ABT_bool *flag)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_thread *p_thread = ABTI_thread_get_ptr(thread);
    ABTI_CHECK_NULL_THREAD_PTR(p_thread);

    *flag = p_thread->attr.migratable;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

/**
 * @ingroup ULT
 * @brief   Check if the target ULT is the primary ULT.
 *
 * \c ABT_thread_is_primary confirms whether the target ULT, \c thread,
 * is the primary ULT and returns the result through \c flag.
 * If \c thread is a handle to the primary ULT, \c flag is set to \c ABT_TRUE.
 * Otherwise, \c flag is set to \c ABT_FALSE.
 *
 * @param[in]  thread  handle to the target ULT
 * @param[out] flag    result (<tt>ABT_TRUE</tt>: primary ULT,
 *                     <tt>ABT_FALSE</tt>: not)
 * @return Error code
 * @retval ABT_SUCCESS        on success
 * @retval ABT_ERR_INV_THREAD invalid ULT handle
 */
int ABT_thread_is_primary(ABT_thread thread, ABT_bool *flag)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_thread *p_thread;

    p_thread = ABTI_thread_get_ptr(thread);
    ABTI_CHECK_NULL_THREAD_PTR(p_thread);

    *flag = (p_thread->type == ABTI_THREAD_TYPE_MAIN) ? ABT_TRUE : ABT_FALSE;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

/**
 * @ingroup ULT
 * @brief   Compare two ULT handles for equality.
 *
 * \c ABT_thread_equal() compares two ULT handles for equality. If two handles
 * are associated with the same ULT object, \c result will be set to
 * \c ABT_TRUE. Otherwise, \c result will be set to \c ABT_FALSE.
 *
 * @param[in]  thread1  handle to the ULT 1
 * @param[in]  thread2  handle to the ULT 2
 * @param[out] result   comparison result (<tt>ABT_TRUE</tt>: same,
 *                      <tt>ABT_FALSE</tt>: not same)
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_thread_equal(ABT_thread thread1, ABT_thread thread2, ABT_bool *result)
{
    ABTI_thread *p_thread1 = ABTI_thread_get_ptr(thread1);
    ABTI_thread *p_thread2 = ABTI_thread_get_ptr(thread2);
    *result = (p_thread1 == p_thread2) ? ABT_TRUE : ABT_FALSE;
    return ABT_SUCCESS;
}

/**
 * @ingroup ULT
 * @brief   Increment the ULT's reference count.
 *
 * \c ABT_thread_retain() increments the ULT's reference count by one. If the
 * user obtains a ULT handle through \c ABT_thread_create() or
 * \c ABT_thread_self(), those routines perform an implicit retain.
 *
 * @param[in] thread  handle to the ULT
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_thread_retain(ABT_thread thread)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_thread *p_thread = ABTI_thread_get_ptr(thread);
    ABTI_CHECK_NULL_THREAD_PTR(p_thread);

    ABTI_thread_retain(p_thread);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

/**
 * @ingroup ULT
 * @brief   Decrement the ULT's reference count.
 *
 * \c ABT_thread_release() decrements the ULT's reference count by one.
 * After the ULT's reference count becomes zero, the ULT object will be freed.
 *
 * @param[in] thread  handle to the ULT
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_thread_release(ABT_thread thread)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_thread *p_thread = ABTI_thread_get_ptr(thread);
    ABTI_CHECK_NULL_THREAD_PTR(p_thread);

    ABTI_thread_release(p_thread);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

/**
 * @ingroup ULT
 * @brief   Get the ULT's stack size.
 *
 * \c ABT_thread_get_stacksize() returns the stack size of \c thread in bytes.
 *
 * @param[in]  thread     handle to the target thread
 * @param[out] stacksize  stack size in bytes
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_thread_get_stacksize(ABT_thread thread, size_t *stacksize)
{
    int abt_errno = ABT_SUCCESS;

    ABTI_thread *p_thread = ABTI_thread_get_ptr(thread);
    ABTI_CHECK_NULL_THREAD_PTR(p_thread);

    /* Return value */
    *stacksize = p_thread->attr.stacksize;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

/**
 * @ingroup ULT
 * @brief   Set the thread's name.
 *
 * @param[in] thread  handle to the target thread
 * @param[in] name    thread name
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_thread_set_name(ABT_thread thread, const char *name)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_thread *p_thread = ABTI_thread_get_ptr(thread);
    ABTI_CHECK_NULL_THREAD_PTR(p_thread);

    size_t len = strlen(name);
    ABTI_mutex_spinlock(&p_thread->mutex);
    if (p_thread->p_name) ABTU_free(p_thread->p_name);
    p_thread->p_name = (char *)ABTU_malloc(len + 1);
    ABTU_strcpy(p_thread->p_name, name);
    ABTI_mutex_unlock(&p_thread->mutex);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

/**
 * @ingroup ULT
 * @brief   Return the name of ULT and its length.
 *
 * \c ABT_thread_get_name() gets the string name of target ULT and the length
 * of name in bytes. If \c name is NULL, only \c len is returned.
 * If \c name is not NULL, it should have enough space to save \c len bytes of
 * characters. If \c len is NULL, \c len is ignored.
 *
 * @param[in]  thread  handle to the target ULT
 * @param[out] name    ULT name
 * @param[out] len     the length of name in bytes
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_thread_get_name(ABT_thread thread, char *name, size_t *len)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_thread *p_thread = ABTI_thread_get_ptr(thread);
    ABTI_CHECK_NULL_THREAD_PTR(p_thread);

    if (name != NULL) {
        ABTU_strcpy(name, p_thread->p_name);
    }
    if (len != NULL) {
        *len = strlen(p_thread->p_name);
    }

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

/**
 * @ingroup ULT
 * @brief   Get the ULT's id
 *
 * \c ABT_thread_get_id() returns the id of \c a thread.
 *
 * @param[in]  thread     handle to the target thread
 * @param[out] thread_id  thread id
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_thread_get_id(ABT_thread thread, ABT_thread_id *thread_id)
{
    int abt_errno = ABT_SUCCESS;

    ABTI_thread *p_thread = ABTI_thread_get_ptr(thread);
    ABTI_CHECK_NULL_THREAD_PTR(p_thread);

    *thread_id = ABTI_thread_get_id(p_thread);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}


/*****************************************************************************/
/* Private APIs                                                              */
/*****************************************************************************/

int ABTI_thread_migrate_to_pool(ABTI_thread *p_thread, ABTI_pool *p_pool)
{
    int abt_errno = ABT_SUCCESS;

    /* checking for cases when migration is not allowed */
    ABTI_CHECK_TRUE(ABTI_pool_accept_migration(p_pool, p_thread->p_pool)
                      == ABT_TRUE,
                    ABT_ERR_INV_POOL);
    ABTI_CHECK_TRUE(p_thread->type != ABTI_THREAD_TYPE_MAIN &&
                      p_thread->type != ABTI_THREAD_TYPE_MAIN_SCHED,
                    ABT_ERR_INV_THREAD);
    ABTI_CHECK_TRUE(p_thread->state != ABT_THREAD_STATE_TERMINATED,
                    ABT_ERR_INV_THREAD);

    /* checking for migration to the same pool */
    ABTI_CHECK_TRUE(p_thread->p_pool != p_pool, ABT_ERR_MIGRATION_TARGET);

    /* adding request to the thread */
    ABTI_mutex_spinlock(&p_thread->mutex);
    ABTI_thread_add_req_arg(p_thread, ABTI_THREAD_REQ_MIGRATE, p_pool);
    ABTI_mutex_unlock(&p_thread->mutex);
    ABTD_atomic_fetch_or_uint32(&p_thread->request, ABTI_THREAD_REQ_MIGRATE);

    /* yielding if it is the same thread */
    if (lp_ABTI_local != NULL && p_thread == ABTI_local_get_thread()) {
        ABT_thread_yield();
    }
    goto fn_exit;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

int ABTI_thread_create_main(ABTI_xstream *p_xstream, ABTI_thread **p_thread)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_thread *p_newthread;

    p_newthread = (ABTI_thread *)ABTU_malloc(sizeof(ABTI_thread));

    /* Set attributes */
    ABTI_thread_set_attr(p_newthread, ABT_THREAD_ATTR_NULL);
    p_newthread->attr.migratable = ABT_FALSE;

    /* Create a context */
    p_newthread->p_stack = NULL;
    abt_errno = ABTD_thread_context_create(NULL, NULL, NULL, 0, NULL,
                                           &p_newthread->ctx);
    ABTI_CHECK_ERROR(abt_errno);

    /* Initialize the mutex */
    ABTI_mutex_init(&p_newthread->mutex);

    p_newthread->unit            = ABT_UNIT_NULL;
    p_newthread->p_last_xstream  = p_xstream;
    p_newthread->is_sched        = NULL;
    p_newthread->p_pool          = NULL;
    p_newthread->type            = ABTI_THREAD_TYPE_MAIN;
    p_newthread->state           = ABT_THREAD_STATE_RUNNING;
    p_newthread->refcount        = 0;
    p_newthread->request         = 0;
    p_newthread->p_req_arg       = NULL;
    p_newthread->id              = ABTI_THREAD_INIT_ID;
    p_newthread->p_name          = NULL;

    *p_thread = p_newthread;

  fn_exit:
    return abt_errno;

  fn_fail:
    *p_thread = NULL;
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

int ABTI_thread_create_main_sched(ABTI_sched *p_sched, ABT_thread *newthread)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_thread *p_newthread;
    size_t stacksize;

    p_newthread = (ABTI_thread *)ABTU_malloc(sizeof(ABTI_thread));

    /* Set attributes */
    ABTI_thread_set_attr(p_newthread, ABT_THREAD_ATTR_NULL);

    /* Create a stack for this thread */
    stacksize = p_newthread->attr.stacksize;
    p_newthread->p_stack = ABTU_malloc(stacksize);
    ABTI_VALGRIND_STACK_REGISTER(p_newthread->p_stack, stacksize);

    /* Create a thread context */
    ABTI_xstream *p_xstream = ABTI_local_get_xstream();
    ABTD_thread_context *p_ctx = ABTI_xstream_get_sched_ctx(p_xstream);
    abt_errno = ABTD_thread_context_create(p_ctx,
            ABTI_xstream_loop, (void *)p_xstream,
            stacksize, p_newthread->p_stack, &p_newthread->ctx);
    ABTI_CHECK_ERROR(abt_errno);

    p_sched->p_ctx = &p_newthread->ctx;
    if (p_xstream->type == ABTI_XSTREAM_TYPE_PRIMARY) {
        ABTI_thread *p_main_thread = ABTI_global_get_main();
        ABTD_thread_context_change_link(p_sched->p_ctx, &p_main_thread->ctx);
    }

    /* Initialize the mutex */
    ABTI_mutex_init(&p_newthread->mutex);

    p_newthread->unit           = ABT_UNIT_NULL;
    p_newthread->p_last_xstream = NULL;
    p_newthread->is_sched       = p_sched;
    p_newthread->p_pool         = NULL;
    p_newthread->type           = ABTI_THREAD_TYPE_MAIN_SCHED;
    p_newthread->state          = ABT_THREAD_STATE_READY;
    p_newthread->refcount       = (newthread != NULL) ? 1 : 0;
    p_newthread->request        = 0;
    p_newthread->p_req_arg      = NULL;
    p_newthread->id             = ABTI_THREAD_INIT_ID;
    p_newthread->p_name         = NULL;

    /* Return value */
    if (newthread) *newthread = ABTI_thread_get_handle(p_newthread);

  fn_exit:
    return abt_errno;

  fn_fail:
    if (newthread) *newthread = ABT_THREAD_NULL;
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

int ABTI_thread_free_main(ABTI_thread *p_thread)
{
    int abt_errno = ABT_SUCCESS;

    /* Free the name */
    if (p_thread->p_name) ABTU_free(p_thread->p_name);

    ABTU_free(p_thread);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

int ABTI_thread_free(ABTI_thread *p_thread)
{
    int abt_errno = ABT_SUCCESS;

    /* Mutex of p_thread may have been locked somewhere. We free p_thread when
       mutex can be locked here. Since p_thread and its mutex will be freed,
       we don't need to unlock the mutex. */
    ABTI_mutex_spinlock(&p_thread->mutex);

    /* Free the unit */
    if (p_thread->refcount == 0) {
        p_thread->p_pool->u_free(&p_thread->unit);
    }

    /* Free the name */
    if (p_thread->p_name) ABTU_free(p_thread->p_name);

    /* Free the stack and the context */
    ABTU_free(p_thread->p_stack);
    ABTD_thread_context_free(&p_thread->ctx);

    ABTU_free(p_thread);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

int ABTI_thread_set_blocked(ABTI_thread *p_thread)
{
    int abt_errno = ABT_SUCCESS;

    /* The main sched cannot be blocked */
    ABTI_CHECK_TRUE(p_thread->type != ABTI_THREAD_TYPE_MAIN_SCHED,
                    ABT_ERR_THREAD);

    /* The main ULT */
    if (p_thread->type == ABTI_THREAD_TYPE_MAIN) {
        /* Change the ULT's state to BLOCKED */
        p_thread->state = ABT_THREAD_STATE_BLOCKED;

    } else {
        /* To prevent the scheduler from adding the ULT to the pool */
        ABTD_atomic_fetch_or_uint32(&p_thread->request, ABTI_THREAD_REQ_BLOCK);

        /* Change the ULT's state to BLOCKED */
        p_thread->state = ABT_THREAD_STATE_BLOCKED;

        /* Increase the number of blocked ULTs */
        ABTI_pool *p_pool = p_thread->p_pool;
        ABTI_pool_inc_num_blocked(p_pool);
    }

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

/* NOTE: This routine should be called after ABTI_thread_set_blocked. */
void ABTI_thread_suspend(ABTI_thread *p_thread)
{
    ABTI_ASSERT(p_thread == ABTI_local_get_thread());
    ABTI_ASSERT(p_thread->p_last_xstream == ABTI_local_get_xstream());
    ABTI_ASSERT(p_thread->type == ABTI_THREAD_TYPE_MAIN
             || p_thread->request & ABTI_THREAD_REQ_BLOCK);

    /* Switch to the scheduler, i.e., suspend p_thread  */
    ABTI_xstream *p_xstream = ABTI_local_get_xstream();
    ABTD_thread_context_switch(&p_thread->ctx, ABTI_xstream_get_sched_ctx(p_xstream));

    /* The suspended ULT resumes its execution from here. */
    ABTI_local_set_thread(p_thread);

    if (p_thread->type == ABTI_THREAD_TYPE_MAIN) {
        while (p_thread->state == ABT_THREAD_STATE_BLOCKED) {
            /* Switch to the top scheduler */
            ABTD_thread_context_switch(&p_thread->ctx,
                ABTI_xstream_get_sched_ctx(p_thread->p_last_xstream));
        }
        ABTI_local_set_thread(p_thread);
        p_thread->state = ABT_THREAD_STATE_RUNNING;
    }
}

int ABTI_thread_set_ready(ABTI_thread *p_thread)
{
    int abt_errno = ABT_SUCCESS;

    /* The ULT should be in BLOCKED state. */
    ABTI_CHECK_TRUE(p_thread->state == ABT_THREAD_STATE_BLOCKED, ABT_ERR_THREAD);

    if (p_thread->type == ABTI_THREAD_TYPE_MAIN) {
        p_thread->state = ABT_THREAD_STATE_READY;

    } else {
        /* Add the ULT to its associated pool */
        ABTI_xstream *p_xstream = ABTI_xstream_self();
        abt_errno = ABTI_pool_add_thread(p_thread, p_xstream);
        ABTI_CHECK_ERROR(abt_errno);

        /* Decrease the number of blocked threads */
        ABTI_pool_dec_num_blocked(p_thread->p_pool);
    }

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

ABT_bool ABTI_thread_is_ready(ABTI_thread *p_thread)
{
    /* ULT can be regarded as 'ready' only if its state is READY and it has been
     * pushed into a pool. Since we set ULT's state to READY and then push it
     * into a pool, we check them in the reverse order, i.e., check if the ULT
     * is inside a pool and the its state. */
    ABTI_pool *p_pool = p_thread->p_pool;
    if (p_pool->u_is_in_pool(p_thread->unit) == ABT_TRUE &&
        p_thread->state == ABT_THREAD_STATE_READY) {
        return ABT_TRUE;
    }

    return ABT_FALSE;
}

void ABTI_thread_set_attr(ABTI_thread *p_thread, ABT_thread_attr attr)
{
    ABTI_thread_attr *my_attr = &p_thread->attr;
    if (attr != ABT_THREAD_ATTR_NULL) {
        ABTI_thread_attr *p_attr = ABTI_thread_attr_get_ptr(attr);
        my_attr->stacksize  = p_attr->stacksize;
        my_attr->migratable = p_attr->migratable;
        my_attr->f_cb       = p_attr->f_cb;
        my_attr->p_cb_arg   = p_attr->p_cb_arg;
    } else {
        my_attr->stacksize  = gp_ABTI_global->default_stacksize;
        my_attr->migratable = ABT_TRUE;
        my_attr->f_cb       = NULL;
        my_attr->p_cb_arg   = NULL;
    }
}

int ABTI_thread_print(ABTI_thread *p_thread)
{
    int abt_errno = ABT_SUCCESS;
    if (p_thread == NULL) {
        printf("[NULL THREAD]");
        goto fn_exit;
    }

    printf("[");
    printf("id:%" PRIu64 " ", ABTI_thread_get_id(p_thread));
    if (p_thread->p_last_xstream == NULL)
        printf("last xstream: NULL");
    else
        printf("last xstream:%" PRIu64 " ", p_thread->p_last_xstream->rank);
    printf("name:%s ", p_thread->p_name);
    printf("type:");
    switch (p_thread->type) {
        case ABTI_THREAD_TYPE_MAIN:       printf("MAIN "); break;
        case ABTI_THREAD_TYPE_MAIN_SCHED: printf("MAIN_SCHED "); break;
        case ABTI_THREAD_TYPE_USER:       printf("USER "); break;
        default: printf("UNKNOWN "); break;
    }
    printf("state:");
    switch (p_thread->state) {
        case ABT_THREAD_STATE_READY:      printf("READY "); break;
        case ABT_THREAD_STATE_RUNNING:    printf("RUNNING "); break;
        case ABT_THREAD_STATE_BLOCKED:    printf("BLOCKED "); break;
        case ABT_THREAD_STATE_TERMINATED: printf("TERMINATED "); break;
        default: printf("UNKNOWN ");
    }
    printf("attr:");
    ABTI_thread_attr_print(&p_thread->attr);
    printf("refcount:%u ", p_thread->refcount);
    printf("request:%x ", p_thread->request);
    printf("req_arg:%p ", p_thread->p_req_arg);
    printf("stack:%p", p_thread->p_stack);
    printf("]");

  fn_exit:
    return abt_errno;
}

void ABTI_thread_add_req_arg(ABTI_thread *p_thread, uint32_t req, void *arg)
{
    ABTI_thread_req_arg *new;
    ABTI_thread_req_arg *p_head = p_thread->p_req_arg;

    /* Overwrite the previous same request if exists */
    while (p_head != NULL) {
        if (p_head->request == req) {
            p_head->p_arg = arg;
            return;
        }
    }

    new = (ABTI_thread_req_arg *)ABTU_malloc(sizeof(ABTI_thread_req_arg));

    /* filling the new argument data structure */
    new->request = req;
    new->p_arg = arg;
    new->next = NULL;

    if (p_head == NULL) {
        p_thread->p_req_arg = new;
    } else {
        while (p_head->next != NULL)
            p_head = p_head->next;
        p_head->next = new;
    }
}

void *ABTI_thread_extract_req_arg(ABTI_thread *p_thread, uint32_t req)
{
    void *result = NULL;
    ABTI_thread_req_arg *p_last = NULL, *p_head = p_thread->p_req_arg;

    while (p_head != NULL) {
        if (p_head->request == req) {
            result = p_head->p_arg;
            if (p_last == NULL)
                p_thread->p_req_arg = p_head->next;
            else
                p_last->next = p_head->next;
            ABTU_free(p_head);
            break;
        }
        p_last = p_head;
        p_head = p_head->next;
    }

    return result;
}

void ABTI_thread_retain(ABTI_thread *p_thread)
{
    ABTD_atomic_fetch_add_uint32(&p_thread->refcount, 1);
}

void ABTI_thread_release(ABTI_thread *p_thread)
{
    uint32_t refcount;
    while ((refcount = p_thread->refcount) > 0) {
        if (ABTD_atomic_cas_uint32(&p_thread->refcount, refcount,
            refcount - 1) == refcount) {
            break;
        }
    }
}

static uint64_t g_thread_id = 0;
void ABTI_thread_reset_id(void)
{
    g_thread_id = 0;
}

ABT_thread_id ABTI_thread_get_id(ABTI_thread *p_thread)
{
    if (p_thread->id == ABTI_THREAD_INIT_ID) {
        p_thread->id = ABTI_thread_get_new_id();
    }
    return p_thread->id;
}

/*****************************************************************************/
/* Internal static functions                                                 */
/*****************************************************************************/

static inline ABT_thread_id ABTI_thread_get_new_id(void)
{
    return (ABT_thread_id)ABTD_atomic_fetch_add_uint64(&g_thread_id, 1);
}

