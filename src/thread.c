/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"

static inline ABT_bool ABTI_thread_is_ready(ABTI_thread *p_thread);
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

    /* Allocate a ULT object and its stack */
    p_newthread = ABTI_mem_alloc_thread(attr, &stacksize);

    /* Create a thread context */
    abt_errno = ABTD_thread_context_create(NULL,
            thread_func, arg, stacksize, p_newthread->attr.p_stack,
            &p_newthread->ctx);
    ABTI_CHECK_ERROR(abt_errno);

    p_newthread->state          = ABT_THREAD_STATE_READY;
    p_newthread->request        = 0;
    p_newthread->p_last_xstream = NULL;
#ifndef ABT_CONFIG_DISABLE_STACKABLE_SCHED
    p_newthread->is_sched       = NULL;
#endif
    p_newthread->p_pool         = p_pool;
    p_newthread->refcount       = (newthread != NULL) ? 1 : 0;
    p_newthread->type           = ABTI_THREAD_TYPE_USER;
    p_newthread->p_req_arg      = NULL;
    p_newthread->p_keytable     = NULL;
    p_newthread->id             = ABTI_THREAD_INIT_ID;

    /* Create a spinlock */
    ABTI_spinlock_create(&p_newthread->lock);

    /* Create a wrapper unit */
    h_newthread = ABTI_thread_get_handle(p_newthread);
    p_newthread->unit = p_pool->u_create_from_thread(h_newthread);

    LOG_EVENT("[U%" PRIu64 "] created\n", ABTI_thread_get_id(p_newthread));

    /* Add this thread to the pool */
#ifdef ABT_CONFIG_DISABLE_POOL_PRODUCER_CHECK
    ABTI_pool_push(p_pool, p_newthread->unit);
#else
    abt_errno = ABTI_pool_push(p_pool, p_newthread->unit, ABTI_xstream_self());
    if (abt_errno != ABT_SUCCESS) {
        ABTI_thread_free(p_newthread);
        goto fn_fail;
    }
#endif

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
 * @brief   Revive the ULT.
 *
 * \c ABT_thread_revive() revives the ULT, \c thread, with \c thread_func and
 * \arg while it does not change the attributes used in creating \c thread.
 * The revived ULT is pushed into \c pool.
 *
 * This function must be called with a valid ULT handle, which has not been
 * freed by \c ABT_thread_free().  However, the ULT should have been joined by
 * \c ABT_thread_join() before its handle is used in this routine.
 *
 * @param[in]     pool         handle to the associated pool
 * @param[in]     thread_func  function to be executed by the ULT
 * @param[in]     arg          argument for thread_func
 * @param[in,out] thread       handle to the ULT
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_thread_revive(ABT_pool pool, void(*thread_func)(void *), void *arg,
                      ABT_thread *thread)
{
    int abt_errno = ABT_SUCCESS;
    size_t stacksize;

    ABTI_thread *p_thread = ABTI_thread_get_ptr(*thread);
    ABTI_CHECK_NULL_THREAD_PTR(p_thread);
    ABTI_CHECK_TRUE(p_thread->state == ABT_THREAD_STATE_TERMINATED,
                    ABT_ERR_INV_THREAD);

    ABTI_pool *p_pool = ABTI_pool_get_ptr(pool);
    ABTI_CHECK_NULL_POOL_PTR(p_pool);

    /* Create a ULT context */
    stacksize = p_thread->attr.stacksize;
    abt_errno = ABTD_thread_context_create(NULL, thread_func, arg,
                                           stacksize, p_thread->attr.p_stack,
                                           &p_thread->ctx);
    ABTI_CHECK_ERROR(abt_errno);

    p_thread->state          = ABT_THREAD_STATE_READY;
    p_thread->request        = 0;
    p_thread->p_last_xstream = NULL;
    p_thread->refcount       = 1;
    p_thread->type           = ABTI_THREAD_TYPE_USER;

    if (p_thread->p_pool != p_pool) {
        /* Free the unit for the old pool */
        p_thread->p_pool->u_free(&p_thread->unit);

        /* Set the new pool */
        p_thread->p_pool = p_pool;

        /* Create a wrapper unit */
        p_thread->unit = p_pool->u_create_from_thread(*thread);
    }

    LOG_EVENT("[U%" PRIu64 "] revived\n", ABTI_thread_get_id(p_thread));

    /* Add this thread to the pool */
#ifdef ABT_CONFIG_DISABLE_POOL_PRODUCER_CHECK
    ABTI_pool_push(p_pool, p_thread->unit);
#else
    abt_errno = ABTI_pool_push(p_pool, p_thread->unit, ABTI_xstream_self());
    ABTI_CHECK_ERROR(abt_errno);
#endif

  fn_exit:
    return abt_errno;

  fn_fail:
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
    if (p_thread->state != ABT_THREAD_STATE_TERMINATED) {
        ABT_thread_join(h_thread);
    }

    /* Free the ABTI_thread structure */
    ABTI_thread_free(p_thread);

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
    ABTI_thread *p_thread = ABTI_thread_get_ptr(thread);
    ABTI_CHECK_NULL_THREAD_PTR(p_thread);

    if (p_thread->state == ABT_THREAD_STATE_TERMINATED) return abt_errno;

    ABTI_CHECK_TRUE_MSG(p_thread->type != ABTI_THREAD_TYPE_MAIN &&
                          p_thread->type != ABTI_THREAD_TYPE_MAIN_SCHED,
                        ABT_ERR_INV_THREAD,
                        "The main ULT cannot be joined.");

#ifndef ABT_CONFIG_DISABLE_EXT_THREAD
    ABT_unit_type type;
    ABT_self_get_type(&type);
    if (type != ABT_UNIT_TYPE_THREAD) goto yield_based;
#endif

    ABTI_CHECK_TRUE_MSG(p_thread != ABTI_local_get_thread(), ABT_ERR_INV_THREAD,
                        "The target ULT should be different.");

    ABTI_thread *p_self = ABTI_local_get_thread();
    ABT_pool_access access = p_self->p_pool->access;

    if ((p_self->p_pool == p_thread->p_pool) &&
        (access == ABT_POOL_ACCESS_PRIV ||
         access == ABT_POOL_ACCESS_MPSC ||
         access == ABT_POOL_ACCESS_SPSC) &&
        (p_thread->state == ABT_THREAD_STATE_READY)) {

        ABTI_xstream *p_xstream = p_self->p_last_xstream;

        /* If other ES is calling ABTI_thread_set_ready(), p_thread may not
         * have been added to the pool yet because ABTI_thread_set_ready()
         * changes the state first followed by pushing p_thread to the pool.
         * Therefore, we have to check whether p_thread is in the pool, and if
         * not, we need to wait until it is added. */
        while (p_thread->p_pool->u_is_in_pool(p_thread->unit) != ABT_TRUE) {}

        /* Remove the target ULT from the pool */
        ABTI_POOL_REMOVE(p_thread->p_pool, p_thread->unit, p_xstream);

        /* Set the link in the context for the target ULT */
        ABTD_thread_context_change_link(&p_thread->ctx, &p_self->ctx);

        /* Set the last ES */
        p_thread->p_last_xstream = p_xstream;
        p_thread->state = ABT_THREAD_STATE_RUNNING;

        /* Make the current ULT BLOCKED */
        p_self->state = ABT_THREAD_STATE_BLOCKED;
        ABTI_pool_inc_num_blocked(p_self->p_pool);

        LOG_EVENT("[U%" PRIu64 ":E%" PRIu64 "] blocked to join U%" PRIu64 "\n",
                  ABTI_thread_get_id(p_self), p_self->p_last_xstream->rank,
                  ABTI_thread_get_id(p_thread));
        LOG_EVENT("[U%" PRIu64 ":E%" PRIu64 "] start running\n",
                  ABTI_thread_get_id(p_thread), p_thread->p_last_xstream->rank);

        /* Switch the context */
        ABTI_local_set_thread(p_thread);
        ABTD_thread_context_switch(&p_self->ctx, &p_thread->ctx);

    } else if ((p_self->p_pool != p_thread->p_pool) &&
               (access == ABT_POOL_ACCESS_PRIV ||
                access == ABT_POOL_ACCESS_SPSC)) {
        /* FIXME: once we change the suspend/resume mechanism (i.e., asking the
         * scheduler to wake up the blocked ULT), we will be able to handle all
         * access modes. */
        goto yield_based;

    } else {
        /* Tell p_thread that there has been a join request. */
        /* If request already has ABTI_THREAD_REQ_JOIN, p_thread is terminating.
         * We can't block p_self in this case. */
        uint32_t req = ABTD_atomic_fetch_or_uint32(&p_thread->request,
                                                   ABTI_THREAD_REQ_JOIN);
        if (req & ABTI_THREAD_REQ_JOIN) goto yield_based;

        ABTI_thread_set_blocked(p_self);
        LOG_EVENT("[U%" PRIu64 ":E%" PRIu64 "] blocked to join U%" PRIu64 "\n",
                  ABTI_thread_get_id(p_self), p_self->p_last_xstream->rank,
                  ABTI_thread_get_id(p_thread));

        /* Set the link in the context of the target ULT */
        ABTD_thread_context_change_link(&p_thread->ctx, &p_self->ctx);

        /* Suspend the current ULT */
        ABTI_thread_suspend(p_self);
    }

    /* Resume */
    /* If p_self's state is BLOCKED, the target ULT has terminated on the same
     * ES as p_self's ES and the control has come from the target ULT.
     * Otherwise, the target ULT had been migrated to a different ES, p_self
     * has been resumed by p_self's scheduler.  In the latter case, we don't
     * need to change p_self's state and the local ULT information. */
    if (p_self->state == ABT_THREAD_STATE_BLOCKED) {
        p_self->state = ABT_THREAD_STATE_RUNNING;
        ABTI_pool_dec_num_blocked(p_self->p_pool);
        LOG_EVENT("[U%" PRIu64 ":E%" PRIu64 "] resume after join\n",
                  ABTI_thread_get_id(p_self), p_self->p_last_xstream->rank);
        ABTI_local_set_thread(p_self);
        return abt_errno;
    }

  yield_based:
    while (p_thread->state != ABT_THREAD_STATE_TERMINATED) {
        ABT_thread_yield();
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
    ABTI_thread_set_request(p_thread, ABTI_THREAD_REQ_EXIT);

    /* Terminate this ULT */
    ABTD_thread_exit(p_thread);

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
#ifdef ABT_CONFIG_DISABLE_THREAD_CANCEL
    return ABT_ERR_FEATURE_NA;
#else
    int abt_errno = ABT_SUCCESS;
    ABTI_thread *p_thread = ABTI_thread_get_ptr(thread);
    ABTI_CHECK_NULL_THREAD_PTR(p_thread);

    ABTI_CHECK_TRUE_MSG(p_thread->type != ABTI_THREAD_TYPE_MAIN &&
                          p_thread->type != ABTI_THREAD_TYPE_MAIN_SCHED,
                        ABT_ERR_INV_THREAD,
                        "The main thread cannot be canceled.");

    /* Set the cancel request */
    ABTI_thread_set_request(p_thread, ABTI_THREAD_REQ_CANCEL);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
#endif
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

#ifndef ABT_CONFIG_DISABLE_EXT_THREAD
    /* In case that Argobots has not been initialized or this routine is called
     * by an external thread, e.g., pthread, return an error code instead of
     * making the call fail. */
    if (gp_ABTI_global == NULL) {
        abt_errno = ABT_ERR_UNINITIALIZED;
        *thread = ABT_THREAD_NULL;
        return abt_errno;
    }
    if (lp_ABTI_local == NULL) {
        abt_errno = ABT_ERR_INV_XSTREAM;
        *thread = ABT_THREAD_NULL;
        return abt_errno;
    }
#endif

    ABTI_thread *p_thread = ABTI_local_get_thread();
    if (p_thread != NULL) {
        *thread = ABTI_thread_get_handle(p_thread);
    } else {
        abt_errno = ABT_ERR_INV_THREAD;
        *thread = ABT_THREAD_NULL;
    }

    return abt_errno;
}

/**
 * @ingroup ULT
 * @brief   Return the calling ULT's ID.
 *
 * \c ABT_thread_self_id() returns the ID of the calling ULT.
 *
 * @param[out] id  ULT id
 * @return Error code
 * @retval ABT_SUCCESS           on success
 * @retval ABT_ERR_UNINITIALIZED Argobots has not been initialized
 * @retval ABT_ERR_INV_XSTREAM   called by an external thread, e.g., pthread
 * @retval ABT_ERR_INV_THREAD    called by a tasklet
 */
int ABT_thread_self_id(ABT_thread_id *id)
{
    int abt_errno = ABT_SUCCESS;

#ifndef ABT_CONFIG_DISABLE_EXT_THREAD
    /* In case that Argobots has not been initialized or this routine is called
     * by an external thread, e.g., pthread, return an error code instead of
     * making the call fail. */
    if (gp_ABTI_global == NULL) {
        abt_errno = ABT_ERR_UNINITIALIZED;
        return abt_errno;
    }
    if (lp_ABTI_local == NULL) {
        abt_errno = ABT_ERR_INV_XSTREAM;
        return abt_errno;
    }
#endif

    ABTI_thread *p_thread = ABTI_local_get_thread();
    if (p_thread != NULL) {
        *id = ABTI_thread_get_id(p_thread);
    } else {
        abt_errno = ABT_ERR_INV_THREAD;
    }

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
 * @brief   Get the last pool's ID of the ULT
 *
 * \c ABT_thread_get_last_pool_id() returns the last pool's ID of \c thread.
 * If the ULT is not running, this routine returns the ID of the pool where it
 * is residing.  Otherwise, it returns the ID of the last pool where the ULT
 * was (i.e., the pool from which the ULT was popped).
 *
 * @param[in]  thread  handle to the target ULT
 * @param[out] id      pool id
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_thread_get_last_pool_id(ABT_thread thread, int *id)
{
    int abt_errno = ABT_SUCCESS;

    ABTI_thread *p_thread = ABTI_thread_get_ptr(thread);
    ABTI_CHECK_NULL_THREAD_PTR(p_thread);

    ABTI_ASSERT(p_thread->p_pool);
    *id = (int)(p_thread->p_pool->id);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

/**
 * @ingroup ULT
 * @brief   Set the associated pool for the target ULT.
 *
 * \c ABT_thread_set_associated_pool() changes the associated pool of the target
 * ULT \c thread to \c pool.  This routine must be called after \c thread is
 * popped from its original associated pool (i.e., \c thread must not be inside
 * any pool), which is the pool where \c thread was residing in.
 *
 * NOTE: \c ABT_thread_migrate_to_pool() can be used to change the associated
 * pool of \c thread regardless of its location.
 *
 * @param[in] thread  handle to the target ULT
 * @param[in] pool    handle to the pool
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_thread_set_associated_pool(ABT_thread thread, ABT_pool pool)
{
    int abt_errno = ABT_SUCCESS;

    ABTI_thread *p_thread = ABTI_thread_get_ptr(thread);
    ABTI_CHECK_NULL_THREAD_PTR(p_thread);
    ABTI_pool *p_pool = ABTI_pool_get_ptr(pool);
    ABTI_CHECK_NULL_POOL_PTR(p_pool);

    p_thread->p_pool = p_pool;

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

#ifdef ABT_CONFIG_DISABLE_EXT_THREAD
    p_cur_thread = ABTI_local_get_thread();
#else
    /* If this routine is called by non-ULT, just return. */
    if (lp_ABTI_local != NULL) {
        p_cur_thread = ABTI_local_get_thread();
    }
    if (p_cur_thread == NULL) goto fn_exit;
#endif

    ABTI_xstream *p_xstream = ABTI_local_get_xstream();
    ABTI_thread *p_tar_thread = ABTI_thread_get_ptr(thread);
    ABTI_CHECK_NULL_THREAD_PTR(p_tar_thread);
    LOG_EVENT("[U%" PRIu64 ":E%" PRIu64 "] yield_to -> U%" PRIu64 "\n",
              ABTI_thread_get_id(p_cur_thread),
              p_cur_thread->p_last_xstream->rank,
              ABTI_thread_get_id(p_tar_thread));

    /* The target ULT must be different from the caller ULT. */
    ABTI_CHECK_TRUE_MSG(p_cur_thread != p_tar_thread, ABT_ERR_INV_THREAD,
                        "The caller and target ULTs are the same.");

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

    p_cur_thread->state = ABT_THREAD_STATE_READY;

    /* Add the current thread to the pool again */
    ABTI_POOL_PUSH(p_cur_thread->p_pool, p_cur_thread->unit, p_xstream);

#ifndef ABT_CONFIG_DISABLE_STACKABLE_SCHED
    /* Delete the last context if the ULT is a scheduler */
    if (p_cur_thread->is_sched != NULL) {
        ABTI_xstream_pop_sched(p_xstream);
        p_cur_thread->is_sched->state = ABT_SCHED_STATE_STOPPED;
    }
#endif

    /* Remove the target ULT from the pool */
    ABTI_POOL_REMOVE(p_tar_thread->p_pool, p_tar_thread->unit, p_xstream);

#ifndef ABT_CONFIG_DISABLE_STACKABLE_SCHED
    /* Add a new scheduler if the ULT is a scheduler */
    if (p_tar_thread->is_sched != NULL) {
        p_tar_thread->is_sched->p_ctx = ABTI_xstream_get_sched_ctx(p_xstream);
        ABTI_xstream_push_sched(p_xstream, p_tar_thread->is_sched);
        p_tar_thread->is_sched->state = ABT_SCHED_STATE_RUNNING;
    }
#endif

    /* We set the last ES */
    p_tar_thread->p_last_xstream = p_xstream;

    /* Switch the context */
    ABTI_local_set_thread(p_tar_thread);
    p_tar_thread->state = ABT_THREAD_STATE_RUNNING;
    ABTD_thread_context_switch(&p_cur_thread->ctx, &p_tar_thread->ctx);

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

#ifdef ABT_CONFIG_DISABLE_EXT_THREAD
    p_thread = ABTI_local_get_thread();
#else
    /* If this routine is called by non-ULT, just return. */
    if (lp_ABTI_local != NULL) {
        p_thread = ABTI_local_get_thread();
    }
    if (p_thread == NULL) goto fn_exit;
#endif

    ABTI_CHECK_TRUE(p_thread->p_last_xstream == ABTI_local_get_xstream(),
                    ABT_ERR_THREAD);

    ABTI_thread_yield(p_thread);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

/**
 * @ingroup ULT
 * @brief   Resume the target ULT.
 *
 * \c ABT_thread_resume() makes the blocked ULT schedulable by changing the
 * state of the target ULT to READY and pushing it to its associated pool.
 * The ULT will resume its execution when the scheduler schedules it.
 *
 * The ULT should have been blocked by \c ABT_self_suspend() or
 * \c ABT_thread_suspend().  Otherwise, the behavior of this routine is
 * undefined.
 *
 * @param[in] thread   handle to the target ULT
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_thread_resume(ABT_thread thread)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_thread *p_thread;

    p_thread = ABTI_thread_get_ptr(thread);
    ABTI_CHECK_NULL_THREAD_PTR(p_thread);

    abt_errno = ABTI_thread_set_ready(p_thread);
    ABTI_CHECK_ERROR(abt_errno);

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
#ifndef ABT_CONFIG_DISABLE_MIGRATION
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
        ABTI_spinlock_acquire(&p_xstream->sched_lock);

        /* We check the state of the ES */
        if (p_xstream->state == ABT_XSTREAM_STATE_TERMINATED) {
            abt_errno = ABT_ERR_INV_XSTREAM;
            ABTI_spinlock_release(&p_xstream->sched_lock);
            goto fn_fail;

        } else if (p_xstream->state == ABT_XSTREAM_STATE_RUNNING) {
            p_sched = ABTI_xstream_get_top_sched(p_xstream);

        } else {
            p_sched = p_xstream->p_main_sched;
        }

        /* We check the state of the sched */
        if (p_sched->state == ABT_SCHED_STATE_TERMINATED) {
            abt_errno = ABT_ERR_INV_XSTREAM;
            ABTI_spinlock_release(&p_xstream->sched_lock);
            goto fn_fail;
        } else {
            /* Find a pool */
            ABTI_sched_get_migration_pool(p_sched, p_thread->p_pool, &p_pool);
            if (p_pool == NULL) {
                abt_errno = ABT_ERR_INV_POOL;
                ABTI_spinlock_release(&p_xstream->sched_lock);
                goto fn_fail;
            }
            /* We set the migration counter to prevent the scheduler from
             * stopping */
            ABTI_pool_inc_num_migrations(p_pool);
        }
        ABTI_spinlock_release(&p_xstream->sched_lock);
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
#else
    return ABT_ERR_MIGRATION_NA;
#endif
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
#ifndef ABT_CONFIG_DISABLE_MIGRATION
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
#else
    return ABT_ERR_MIGRATION_NA;
#endif
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
#ifndef ABT_CONFIG_DISABLE_MIGRATION
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
#else
    return ABT_ERR_MIGRATION_NA;
#endif
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
#ifndef ABT_CONFIG_DISABLE_MIGRATION
    /* TODO: fix the bug(s) */
    int abt_errno = ABT_SUCCESS;
    ABT_xstream xstream;
    ABTI_xstream *p_xstream;

    ABTI_thread *p_thread = ABTI_thread_get_ptr(thread);
    ABTI_CHECK_NULL_THREAD_PTR(p_thread);

    ABTI_xstream **p_xstreams = gp_ABTI_global->p_xstreams;

    /* Choose the destination xstream */
    /* FIXME: Currenlty, the target xstream is randomly chosen. We need a
     * better selection strategy. */
    /* TODO: handle better when no pool accepts migration */
    /* TODO: choose a pool also when (p_thread->p_pool->consumer == NULL) */
    while (1) {
        /* Only one ES */
        if (gp_ABTI_global->num_xstreams == 1) {
            abt_errno = ABT_ERR_MIGRATION_NA;
            break;
        }

        p_xstream = p_xstreams[rand() % gp_ABTI_global->num_xstreams];
        if (p_xstream && p_xstream != p_thread->p_last_xstream) {
            if (p_xstream->state == ABT_XSTREAM_STATE_RUNNING) {
                xstream = ABTI_xstream_get_handle(p_xstream);
                abt_errno = ABT_thread_migrate_to_xstream(thread, xstream);
                if (abt_errno != ABT_ERR_INV_XSTREAM &&
                        abt_errno != ABT_ERR_MIGRATION_TARGET) {
                    ABTI_CHECK_ERROR(abt_errno);
                    break;
                }
            }
        }
    }

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
#else
    return ABT_ERR_MIGRATION_NA;
#endif
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
#ifndef ABT_CONFIG_DISABLE_MIGRATION
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
#else
    return ABT_ERR_FEATURE_NA;
#endif
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
#ifndef ABT_CONFIG_DISABLE_MIGRATION
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
#else
    return ABT_ERR_FEATURE_NA;
#endif
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
#ifndef ABT_CONFIG_DISABLE_MIGRATION
    int abt_errno = ABT_SUCCESS;
    ABTI_thread *p_thread = ABTI_thread_get_ptr(thread);
    ABTI_CHECK_NULL_THREAD_PTR(p_thread);

    *flag = p_thread->attr.migratable;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
#else
    return ABT_ERR_FEATURE_NA;
#endif
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
 * user obtains a ULT handle through \c ABT_thread_create(), the creation
 * routine performs an implicit retain.
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

/**
 * @ingroup ULT
 * @brief   Set the argument for the ULT function
 *
 * \c ABT_thread_set_arg() sets the argument for the ULT function.
 *
 * @param[in] thread  handle to the target ULT
 * @param[in] arg     argument for the ULT function
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_thread_set_arg(ABT_thread thread, void *arg)
{
    int abt_errno = ABT_SUCCESS;

    ABTI_thread *p_thread = ABTI_thread_get_ptr(thread);
    ABTI_CHECK_NULL_THREAD_PTR(p_thread);

    ABTD_thread_context_set_arg(&p_thread->ctx, arg);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

/**
 * @ingroup ULT
 * @brief   Retrieve the argument for the ULT function
 *
 * \c ABT_thread_get_arg() returns the argument for the ULT function, which was
 * passed to \c ABT_thread_create() when the target ULT \c thread was created
 * or was set by \c ABT_thread_set_arg().
 *
 * @param[in]  thread  handle to the target ULT
 * @param[out] arg     argument for the ULT function
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_thread_get_arg(ABT_thread thread, void **arg)
{
    int abt_errno = ABT_SUCCESS;

    ABTI_thread *p_thread = ABTI_thread_get_ptr(thread);
    ABTI_CHECK_NULL_THREAD_PTR(p_thread);

    *arg = ABTD_thread_context_get_arg(&p_thread->ctx);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

/**
 * @ingroup ULT
 * @brief   Get attributes of the target ULT
 *
 * \c ABT_thread_get_attr() returns the attributes of the ULT \c thread to
 * \c attr.  \c attr contains actual attribute values that may be different
 * from those used to create \c thread.  Since this routine allocates an
 * attribute object, when \c attr is no longer used it should be destroyed
 * using \c ABT_thread_attr_free().
 *
 * @param[in]  thread  handle to the target ULT
 * @param[out] attr    ULT attributes
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_thread_get_attr(ABT_thread thread, ABT_thread_attr *attr)
{
    int abt_errno = ABT_SUCCESS;

    ABTI_thread *p_thread = ABTI_thread_get_ptr(thread);
    ABTI_CHECK_NULL_THREAD_PTR(p_thread);

    ABTI_thread_attr *p_attr;
    p_attr = ABTI_thread_attr_dup(&p_thread->attr);

    *attr = ABTI_thread_attr_get_handle(p_attr);

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
#ifndef ABT_CONFIG_DISABLE_MIGRATION
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
    ABTI_spinlock_acquire(&p_thread->lock);
    ABTI_thread_add_req_arg(p_thread, ABTI_THREAD_REQ_MIGRATE, p_pool);
    ABTI_spinlock_release(&p_thread->lock);
    ABTI_thread_set_request(p_thread, ABTI_THREAD_REQ_MIGRATE);

    /* yielding if it is the same thread */
    if (lp_ABTI_local != NULL && p_thread == ABTI_local_get_thread()) {
        ABTI_thread_yield(p_thread);
    }
    goto fn_exit;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
#else
    return ABT_ERR_MIGRATION_NA;
#endif
}

int ABTI_thread_create_main(ABTI_xstream *p_xstream, ABTI_thread **p_thread)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_thread *p_newthread;
    ABT_thread h_newthread;
    ABTI_pool *p_pool;

    /* Get the first pool of ES */
    p_pool = ABTI_pool_get_ptr(p_xstream->p_main_sched->pools[0]);

    /* Allocate a ULT object */
    p_newthread = ABTI_mem_alloc_main_thread(ABT_THREAD_ATTR_NULL);

    /* Create a context */
    abt_errno = ABTD_thread_context_create(NULL, NULL, NULL, 0, NULL,
                                           &p_newthread->ctx);
    ABTI_CHECK_ERROR(abt_errno);

    p_newthread->state           = ABT_THREAD_STATE_RUNNING;
    p_newthread->request         = 0;
    p_newthread->p_last_xstream  = p_xstream;
#ifndef ABT_CONFIG_DISABLE_STACKABLE_SCHED
    p_newthread->is_sched        = NULL;
#endif
    p_newthread->p_pool          = p_pool;
    p_newthread->refcount        = 0;
    p_newthread->type            = ABTI_THREAD_TYPE_MAIN;
    p_newthread->p_req_arg       = NULL;
    p_newthread->p_keytable      = NULL;
    p_newthread->id              = ABTI_THREAD_INIT_ID;

    /* Create a spinlock */
    ABTI_spinlock_create(&p_newthread->lock);

    /* Create a wrapper unit */
    h_newthread = ABTI_thread_get_handle(p_newthread);
    p_newthread->unit = p_pool->u_create_from_thread(h_newthread);

    LOG_EVENT("[U%" PRIu64 ":E%" PRIu64 "] main ULT created\n",
              ABTI_thread_get_id(p_newthread),
              p_newthread->p_last_xstream->rank);

    /* Although this main ULT is running now, we add this main ULT to the pool
     * so that the scheduler can schedule the main ULT when the main ULT is
     * context switched to the scheduler for the first time. */
#ifdef ABT_CONFIG_DISABLE_POOL_PRODUCER_CHECK
    ABTI_pool_push(p_pool, p_newthread->unit);
#else
    abt_errno = ABTI_pool_push(p_pool, p_newthread->unit, p_xstream);
    if (abt_errno != ABT_SUCCESS) {
        ABTI_thread_free_main(p_newthread);
        goto fn_fail;
    }
#endif

    /* Return value */
    *p_thread = p_newthread;

  fn_exit:
    return abt_errno;

  fn_fail:
    *p_thread = NULL;
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

/* This routine is to create a ULT for the main scheduler of ES. */
int ABTI_thread_create_main_sched(ABTI_xstream *p_xstream, ABTI_sched *p_sched)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_thread *p_newthread;

    /* Create a ULT context */
    if (p_xstream->type == ABTI_XSTREAM_TYPE_PRIMARY) {
        /* Create a ULT object and its stack */
        size_t stacksize = ABTI_global_get_sched_stacksize();
        p_newthread = ABTI_mem_alloc_thread_with_stacksize(&stacksize);
        ABTI_VALGRIND_STACK_REGISTER(p_newthread->attr.p_stack, stacksize);

        /* When the main scheduler is terminated, the control will jump to the
         * primary ULT. */
        ABTI_thread *p_main_thread = ABTI_global_get_main();
        abt_errno = ABTD_thread_context_create(&p_main_thread->ctx,
                ABTI_xstream_schedule, (void *)p_xstream,
                stacksize, p_newthread->attr.p_stack, &p_newthread->ctx);
        ABTI_CHECK_ERROR(abt_errno);
    } else {
        /* For secondary ESs, the stack of OS thread is used for the main
         * scheduler's ULT. */
        p_newthread = ABTI_mem_alloc_main_thread(ABT_THREAD_ATTR_NULL);
        abt_errno = ABTD_thread_context_create(NULL, NULL, NULL, 0, NULL,
                                               &p_newthread->ctx);
        ABTI_CHECK_ERROR(abt_errno);
    }

    p_newthread->state          = ABT_THREAD_STATE_READY;
    p_newthread->request        = 0;
    p_newthread->p_last_xstream = p_xstream;
#ifndef ABT_CONFIG_DISABLE_STACKABLE_SCHED
    p_newthread->is_sched       = p_sched;
#endif
    p_newthread->unit           = ABT_UNIT_NULL;
    p_newthread->p_pool         = NULL;
    p_newthread->refcount       = 0;
    p_newthread->type           = ABTI_THREAD_TYPE_MAIN_SCHED;
    p_newthread->p_req_arg      = NULL;
    p_newthread->p_keytable     = NULL;
    p_newthread->id             = ABTI_THREAD_INIT_ID;

    /* Create a spinlock */
    ABTI_spinlock_create(&p_newthread->lock);

    LOG_EVENT("[U%" PRIu64 ":E%" PRIu64 "] main sched ULT created\n",
              ABTI_thread_get_id(p_newthread), p_xstream->rank);

    /* Return value */
    p_sched->p_thread = p_newthread;
    p_sched->p_ctx = &p_newthread->ctx;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

/* This routine is to create a ULT for the scheduler. */
int ABTI_thread_create_sched(ABTI_pool *p_pool, ABTI_sched *p_sched)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_thread *p_newthread;
    ABT_thread h_newthread;
    size_t stacksize;

    /* If p_sched is reused, ABT_thread_revive() can be used. */
    if (p_sched->p_thread) {
        ABT_thread h_thread = ABTI_thread_get_handle(p_sched->p_thread);
        ABT_pool h_pool = ABTI_pool_get_handle(p_pool);
        ABT_sched h_sched = ABTI_sched_get_handle(p_sched);
        abt_errno = ABT_thread_revive(h_pool, p_sched->run, (void *)h_sched,
                                      &h_thread);
        ABTI_CHECK_ERROR(abt_errno);
        goto fn_exit;
    }

    /* Allocate a ULT object and its stack */
    stacksize = ABTI_global_get_sched_stacksize();
    p_newthread = ABTI_mem_alloc_thread_with_stacksize(&stacksize);

    /* Create a ULT context */
    abt_errno = ABTD_thread_context_create(NULL,
            p_sched->run, (void *)ABTI_sched_get_handle(p_sched),
            stacksize, p_newthread->attr.p_stack, &p_newthread->ctx);
    ABTI_CHECK_ERROR(abt_errno);

    p_newthread->state          = ABT_THREAD_STATE_READY;
    p_newthread->request        = 0;
    p_newthread->p_last_xstream = NULL;
#ifndef ABT_CONFIG_DISABLE_STACKABLE_SCHED
    p_newthread->is_sched       = p_sched;
#endif
    p_newthread->p_pool         = p_pool;
    p_newthread->refcount       = 1;
    p_newthread->type           = ABTI_THREAD_TYPE_USER;
    p_newthread->p_req_arg      = NULL;
    p_newthread->p_keytable     = NULL;
    p_newthread->id             = ABTI_THREAD_INIT_ID;

    /* Create a spinlock */
    ABTI_spinlock_create(&p_newthread->lock);

    /* Create a wrapper unit */
    h_newthread = ABTI_thread_get_handle(p_newthread);
    p_newthread->unit = p_pool->u_create_from_thread(h_newthread);

    LOG_EVENT("[U%" PRIu64 "] created\n", ABTI_thread_get_id(p_newthread));

    /* Save the ULT pointer in p_sched */
    p_sched->p_thread = p_newthread;

    /* Add this thread to the pool */
#ifdef ABT_CONFIG_DISABLE_POOL_PRODUCER_CHECK
    ABTI_pool_push(p_pool, p_newthread->unit);
#else
    abt_errno = ABTI_pool_push(p_pool, p_newthread->unit, ABTI_xstream_self());
    if (abt_errno != ABT_SUCCESS) {
        p_sched->p_thread = NULL;
        ABTI_thread_free(p_newthread);
        goto fn_fail;
    }
#endif

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

void ABTI_thread_free(ABTI_thread *p_thread)
{
#ifndef ABT_CONFIG_DISABLE_MIGRATION
    /* p_thread's lock may have been acquired somewhere. We free p_thread when
       the lock can be acquired here. */
    ABTI_spinlock_acquire(&p_thread->lock);
#endif

    LOG_EVENT("[U%" PRIu64 ":E%" PRIu64 "] freed\n",
              ABTI_thread_get_id(p_thread), p_thread->p_last_xstream->rank);

    /* Free the unit */
    p_thread->p_pool->u_free(&p_thread->unit);

    /* Free the context */
    ABTD_thread_context_free(&p_thread->ctx);

    /* Free the key-value table */
    if (p_thread->p_keytable) {
        ABTI_ktable_free(p_thread->p_keytable);
    }

    /* Free the spinlock */
    ABTI_spinlock_free(&p_thread->lock);

    /* Free ABTI_thread (stack will also be freed) */
    ABTI_mem_free_thread(p_thread);
}

void ABTI_thread_free_main(ABTI_thread *p_thread)
{
    LOG_EVENT("[U%" PRIu64 ":E%" PRIu64 "] main ULT freed\n",
              ABTI_thread_get_id(p_thread), p_thread->p_last_xstream->rank);

    /* Free the key-value table */
    if (p_thread->p_keytable) {
        ABTI_ktable_free(p_thread->p_keytable);
    }

    ABTI_mem_free_thread(p_thread);
}

void ABTI_thread_free_main_sched(ABTI_thread *p_thread)
{
    LOG_EVENT("[U%" PRIu64 ":E%" PRIu64 "] main sched ULT freed\n",
              ABTI_thread_get_id(p_thread), p_thread->p_last_xstream->rank);

    /* Free the context */
    ABTD_thread_context_free(&p_thread->ctx);

    /* Free the key-value table */
    if (p_thread->p_keytable) {
        ABTI_ktable_free(p_thread->p_keytable);
    }

    ABTI_mem_free_thread(p_thread);
}

int ABTI_thread_set_blocked(ABTI_thread *p_thread)
{
    int abt_errno = ABT_SUCCESS;

    /* The main sched cannot be blocked */
    ABTI_CHECK_TRUE(p_thread->type != ABTI_THREAD_TYPE_MAIN_SCHED,
                    ABT_ERR_THREAD);

    /* To prevent the scheduler from adding the ULT to the pool */
    ABTI_thread_set_request(p_thread, ABTI_THREAD_REQ_BLOCK);

    /* Change the ULT's state to BLOCKED */
    p_thread->state = ABT_THREAD_STATE_BLOCKED;

    /* Increase the number of blocked ULTs */
    ABTI_pool *p_pool = p_thread->p_pool;
    ABTI_pool_inc_num_blocked(p_pool);

    LOG_EVENT("[U%" PRIu64 ":E%" PRIu64 "] blocked\n",
              ABTI_thread_get_id(p_thread), p_thread->p_last_xstream->rank);

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

    /* Switch to the scheduler, i.e., suspend p_thread  */
    ABTI_xstream *p_xstream = ABTI_local_get_xstream();
    LOG_EVENT("[U%" PRIu64 ":E%" PRIu64 "] suspended\n",
              ABTI_thread_get_id(p_thread), p_xstream->rank);
    ABTI_LOG_SET_SCHED(ABTI_xstream_get_top_sched(p_xstream));
    ABTD_thread_context_switch(&p_thread->ctx, ABTI_xstream_get_sched_ctx(p_xstream));

    /* The suspended ULT resumes its execution from here. */
    LOG_EVENT("[U%" PRIu64 ":E%" PRIu64 "] resumed\n",
              ABTI_thread_get_id(p_thread), p_thread->p_last_xstream->rank);
}

int ABTI_thread_set_ready(ABTI_thread *p_thread)
{
    int abt_errno = ABT_SUCCESS;

    /* The ULT should be in BLOCKED state. */
    ABTI_CHECK_TRUE(p_thread->state == ABT_THREAD_STATE_BLOCKED, ABT_ERR_THREAD);

    /* We should wait until the scheduler of the blocked ULT resets the BLOCK
     * request. Otherwise, the ULT can be pushed to a pool here and be
     * scheduled by another scheduler if it is pushed to a shared pool. */
    while (*(volatile uint32_t *)(&p_thread->request) & ABTI_THREAD_REQ_BLOCK) {
    }

    LOG_EVENT("[U%" PRIu64 ":E%" PRIu64 "] set ready\n",
              ABTI_thread_get_id(p_thread), p_thread->p_last_xstream->rank);

    /* Add the ULT to its associated pool */
    ABTI_POOL_ADD_THREAD(p_thread, ABTI_xstream_self());

    /* Decrease the number of blocked threads */
    ABTI_pool_dec_num_blocked(p_thread->p_pool);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

static inline ABT_bool ABTI_thread_is_ready(ABTI_thread *p_thread)
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

void ABTI_thread_print(ABTI_thread *p_thread, FILE *p_os, int indent)
{
    char *prefix = ABTU_get_indent_str(indent);

    if (p_thread == NULL) {
        fprintf(p_os, "%s== NULL ULT ==\n", prefix);
        goto fn_exit;
    }

    ABTI_xstream *p_xstream = p_thread->p_last_xstream;
    uint64_t xstream_rank = p_xstream ? p_xstream->rank : 0;
    char *type, *state;
    char attr[100];

    switch (p_thread->type) {
        case ABTI_THREAD_TYPE_MAIN:       type = "MAIN"; break;
        case ABTI_THREAD_TYPE_MAIN_SCHED: type = "MAIN_SCHED"; break;
        case ABTI_THREAD_TYPE_USER:       type = "USER"; break;
        default:                          type = "UNKNOWN"; break;
    }
    switch (p_thread->state) {
        case ABT_THREAD_STATE_READY:      state = "READY"; break;
        case ABT_THREAD_STATE_RUNNING:    state = "RUNNING"; break;
        case ABT_THREAD_STATE_BLOCKED:    state = "BLOCKED"; break;
        case ABT_THREAD_STATE_TERMINATED: state = "TERMINATED"; break;
        default:                          state = "UNKNOWN"; break;
    }
    ABTI_thread_attr_get_str(&p_thread->attr, attr);

    fprintf(p_os,
        "%s== ULT (%p) ==\n"
        "%sid      : %" PRIu64 "\n"
        "%stype    : %s\n"
        "%sstate   : %s\n"
        "%slast_ES : %p (%" PRIu64 ")\n"
#ifndef ABT_CONFIG_DISABLE_STACKABLE_SCHED
        "%sis_sched: %p\n"
#endif
        "%spool    : %p\n"
        "%srefcount: %u\n"
        "%srequest : 0x%x\n"
        "%sreq_arg : %p\n"
        "%skeytable: %p\n"
        "%sattr    : %s\n",
        prefix, p_thread,
        prefix, ABTI_thread_get_id(p_thread),
        prefix, type,
        prefix, state,
        prefix, p_xstream, xstream_rank,
#ifndef ABT_CONFIG_DISABLE_STACKABLE_SCHED
        prefix, p_thread->is_sched,
#endif
        prefix, p_thread->p_pool,
        prefix, p_thread->refcount,
        prefix, p_thread->request,
        prefix, p_thread->p_req_arg,
        prefix, p_thread->p_keytable,
        prefix, attr
    );

  fn_exit:
    fflush(p_os);
    ABTU_free(prefix);
}

#ifndef ABT_CONFIG_DISABLE_MIGRATION
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
#endif /* ABT_CONFIG_DISABLE_MIGRATION */

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
    if (p_thread == NULL) return ABTI_THREAD_INIT_ID;

    if (p_thread->id == ABTI_THREAD_INIT_ID) {
        p_thread->id = ABTI_thread_get_new_id();
    }
    return p_thread->id;
}

ABT_thread_id ABTI_thread_self_id(void)
{
    ABTI_thread *p_self = NULL;
    if (lp_ABTI_local) p_self = ABTI_local_get_thread();
    return ABTI_thread_get_id(p_self);
}

int ABTI_thread_get_xstream_rank(ABTI_thread *p_thread)
{
    if (p_thread == NULL) return -1;

    if (p_thread->p_last_xstream) {
        return p_thread->p_last_xstream->rank;
    } else {
        return -1;
    }
}

int ABTI_thread_self_xstream_rank(void)
{
    ABTI_thread *p_self = NULL;
    if (lp_ABTI_local) p_self = ABTI_local_get_thread();
    return ABTI_thread_get_xstream_rank(p_self);
}

/*****************************************************************************/
/* Internal static functions                                                 */
/*****************************************************************************/

static inline ABT_thread_id ABTI_thread_get_new_id(void)
{
    return (ABT_thread_id)ABTD_atomic_fetch_add_uint64(&g_thread_id, 1);
}

