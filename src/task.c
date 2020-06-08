/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"

static int ABTI_task_create(ABTI_xstream *p_local_xstream, ABTI_pool *p_pool,
                            void (*task_func)(void *), void *arg,
                            ABTI_sched *p_sched, int refcount,
                            ABTI_task **pp_newtask);
static int ABTI_task_revive(ABTI_xstream *p_local_xstream, ABTI_pool *p_pool,
                            void (*task_func)(void *), void *arg,
                            ABTI_task *p_task);
static inline uint64_t ABTI_task_get_new_id(void);

/** @defgroup TASK Tasklet
 * This group is for Tasklet.
 */

/**
 * @ingroup TASK
 * @brief   Create a new task and return its handle through newtask.
 *
 * \c ABT_task_create() creates a new tasklet that is pushed into \c pool. The
 * insertion is done from the ES where this call is made. Therefore, the access
 * type of \c pool should comply with that. The handle of the newly created
 * tasklet is obtained through \c newtask.
 *
 * If this is ABT_XSTREAM_NULL, the new task is managed globally and it can be
 * executed by any ES. Otherwise, the task is scheduled and runs in the
 * specified ES.
 * If newtask is NULL, the task object will be automatically released when
 * this \a unnamed task completes the execution of task_func. Otherwise,
 * ABT_task_free() can be used to explicitly release the task object.
 *
 * @param[in]  pool       handle to the associated pool
 * @param[in]  task_func  function to be executed by a new task
 * @param[in]  arg        argument for task_func
 * @param[out] newtask    handle to a newly created task
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_task_create(ABT_pool pool, void (*task_func)(void *), void *arg,
                    ABT_task *newtask)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_xstream *p_local_xstream = ABTI_local_get_xstream();
    ABTI_task *p_newtask;
    ABTI_pool *p_pool = ABTI_pool_get_ptr(pool);
    ABTI_CHECK_NULL_POOL_PTR(p_pool);

    int refcount = (newtask != NULL) ? 1 : 0;
    abt_errno = ABTI_task_create(p_local_xstream, p_pool, task_func, arg, NULL,
                                 refcount, &p_newtask);
    ABTI_CHECK_ERROR(abt_errno);

    /* Return value */
    if (newtask) {
        *newtask = ABTI_task_get_handle(p_newtask);
    }

fn_exit:
    return abt_errno;

fn_fail:
    if (newtask)
        *newtask = ABT_TASK_NULL;
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

/**
 * @ingroup TASK
 * @brief   Create a new tasklet associated with the target ES (\c xstream).
 *
 * \c ABT_task_create_on_xstream() creates a new tasklet associated with
 * the target ES and returns its handle through \c newtask. The new tasklet
 * is inserted into a proper pool associated with the main scheduler of
 * the target ES.
 *
 * This routine is only for convenience. If the user wants to focus on the
 * performance, we recommend to use \c ABT_task_create() with directly
 * dealing with pools. Pools are a right way to manage work units in Argobots.
 * ES is just an abstract, and it is not a mechanism for execution and
 * performance tuning.
 *
 * If \c newtask is \c NULL, this routine creates an unnamed tasklet.
 * The object for unnamed tasklet will be automatically freed when the unnamed
 * tasklet completes its execution. Otherwise, this routine creates a named
 * tasklet and \c ABT_task_free() can be used to explicitly free the tasklet
 * object.
 *
 * If \c newtask is not \c NULL and an error occurs in this routine, a non-zero
 * error code will be returned and \c newtask will be set to \c ABT_TASK_NULL.
 *
 * @param[in]  xstream    handle to the target ES
 * @param[in]  task_func  function to be executed by a new tasklet
 * @param[in]  arg        argument for <tt>task_func</tt>
 * @param[out] newtask    handle to a newly created tasklet
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_task_create_on_xstream(ABT_xstream xstream, void (*task_func)(void *),
                               void *arg, ABT_task *newtask)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_xstream *p_local_xstream = ABTI_local_get_xstream();
    ABTI_task *p_newtask;

    ABTI_xstream *p_xstream = ABTI_xstream_get_ptr(xstream);
    ABTI_CHECK_NULL_XSTREAM_PTR(p_xstream);

    /* TODO: need to consider the access type of target pool */
    ABTI_pool *p_pool = ABTI_xstream_get_main_pool(p_xstream);
    int refcount = (newtask != NULL) ? 1 : 0;
    abt_errno = ABTI_task_create(p_local_xstream, p_pool, task_func, arg, NULL,
                                 refcount, &p_newtask);
    ABTI_CHECK_ERROR(abt_errno);

    /* Return value */
    if (newtask)
        *newtask = ABTI_task_get_handle(p_newtask);

fn_exit:
    return abt_errno;

fn_fail:
    if (newtask)
        *newtask = ABT_TASK_NULL;
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

/**
 * @ingroup TASK
 * @brief   Revive the tasklet.
 *
 * \c ABT_task_revive() revives the tasklet, \c task, with \c task_func and
 * \arg and pushes the revived tasklet into \c pool.
 *
 * This function must be called with a valid tasklet handle, which has not been
 * freed by \c ABT_task_free().  However, the tasklet should have been joined
 * by \c ABT_task_join() before its handle is used in this routine.
 *
 * @param[in]     pool       handle to the associated pool
 * @param[in]     task_func  function to be executed by the tasklet
 * @param[in]     arg        argument for task_func
 * @param[in,out] task       handle to the tasklet
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_task_revive(ABT_pool pool, void (*task_func)(void *), void *arg,
                    ABT_task *task)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_xstream *p_local_xstream = ABTI_local_get_xstream();

    ABTI_task *p_task = ABTI_task_get_ptr(*task);
    ABTI_CHECK_NULL_TASK_PTR(p_task);

    ABTI_pool *p_pool = ABTI_pool_get_ptr(pool);
    ABTI_CHECK_NULL_POOL_PTR(p_pool);

    abt_errno =
        ABTI_task_revive(p_local_xstream, p_pool, task_func, arg, p_task);
    ABTI_CHECK_ERROR(abt_errno);

fn_exit:
    return abt_errno;

fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

/**
 * @ingroup TASK
 * @brief   Release the task object associated with task handle.
 *
 * This routine deallocates memory used for the task object. If the task is
 * still running when this routine is called, the deallocation happens after
 * the task terminates and then this routine returns. If it is successfully
 * processed, task is set as ABT_TASK_NULL.
 *
 * @param[in,out] task  handle to the target task
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_task_free(ABT_task *task)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_xstream *p_local_xstream = ABTI_local_get_xstream();
    ABT_task h_task = *task;
    ABTI_task *p_task = ABTI_task_get_ptr(h_task);
    ABTI_CHECK_NULL_TASK_PTR(p_task);

    /* Wait until the task terminates */
    while (ABTD_atomic_acquire_load_int(&p_task->unit_def.state) !=
           ABTI_UNIT_STATE_TERMINATED) {
#ifndef ABT_CONFIG_DISABLE_EXT_THREAD
        if (!ABTI_unit_type_is_thread(ABTI_self_get_type(p_local_xstream))) {
            ABTD_atomic_pause();
            continue;
        }
#endif
        ABTI_thread_yield(&p_local_xstream, p_local_xstream->p_thread);
    }

    /* Free the ABTI_task structure */
    ABTI_task_free(p_local_xstream, p_task);

    /* Return value */
    *task = ABT_TASK_NULL;

fn_exit:
    return abt_errno;

fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

/**
 * @ingroup TASK
 * @brief   Wait for the tasklet to terminate.
 *
 * \c ABT_task_join() blocks until the target tasklet \c task terminates.
 * Since this routine blocks, only ULTs can call this routine.  If tasklets use
 * this routine, the behavior is undefined.
 *
 * @param[in] task  handle to the target tasklet
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_task_join(ABT_task task)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_xstream *p_local_xstream = ABTI_local_get_xstream();

    ABTI_task *p_task = ABTI_task_get_ptr(task);
    ABTI_CHECK_NULL_TASK_PTR(p_task);

    /* TODO: better implementation */
    while (ABTD_atomic_acquire_load_int(&p_task->unit_def.state) !=
           ABTI_UNIT_STATE_TERMINATED) {
#ifndef ABT_CONFIG_DISABLE_EXT_THREAD
        if (!ABTI_unit_type_is_thread(ABTI_self_get_type(p_local_xstream))) {
            ABTD_atomic_pause();
            continue;
        }
#endif
        ABTI_thread_yield(&p_local_xstream, p_local_xstream->p_thread);
    }

fn_exit:
    return abt_errno;

fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

/**
 * @ingroup TASK
 * @brief   Request the cancellation of the target task.
 *
 * @param[in] task  handle to the target task
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_task_cancel(ABT_task task)
{
#ifdef ABT_CONFIG_DISABLE_TASK_CANCEL
    return ABT_ERR_FEATURE_NA;
#else
    int abt_errno = ABT_SUCCESS;
    ABTI_task *p_task = ABTI_task_get_ptr(task);
    ABTI_CHECK_NULL_TASK_PTR(p_task);

    /* Set the cancel request */
    ABTI_task_set_request(p_task, ABTI_UNIT_REQ_CANCEL);

fn_exit:
    return abt_errno;

fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
#endif
}

/**
 * @ingroup TASK
 * @brief   Return the handle of the calling tasklet.
 *
 * \c ABT_task_self() returns the handle of the calling tasklet.
 * If ULTs call this routine, \c ABT_TASK_NULL will be returned to \c task.
 *
 * @param[out] task  tasklet handle
 * @return Error code
 * @retval ABT_SUCCESS           on success
 * @retval ABT_ERR_UNINITIALIZED Argobots has not been initialized
 * @retval ABT_ERR_INV_XSTREAM   called by an external thread, e.g., pthread
 * @retval ABT_ERR_INV_TASK      called by a ULT
 */
int ABT_task_self(ABT_task *task)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_xstream *p_local_xstream = ABTI_local_get_xstream();

#ifndef ABT_CONFIG_DISABLE_EXT_THREAD
    /* In case that Argobots has not been initialized or this routine is called
     * by an external thread, e.g., pthread, return an error code instead of
     * making the call fail. */
    if (gp_ABTI_global == NULL) {
        abt_errno = ABT_ERR_UNINITIALIZED;
        *task = ABT_TASK_NULL;
        return abt_errno;
    }
    if (p_local_xstream == NULL) {
        abt_errno = ABT_ERR_INV_XSTREAM;
        *task = ABT_TASK_NULL;
        return abt_errno;
    }
#endif

    ABTI_task *p_task = p_local_xstream->p_task;
    if (p_task != NULL) {
        *task = ABTI_task_get_handle(p_task);
    } else {
        abt_errno = ABT_ERR_INV_TASK;
        *task = ABT_TASK_NULL;
    }

    return abt_errno;
}

/**
 * @ingroup TASK
 * @brief   Return the ID of the calling tasklet.
 *
 * \c ABT_task_self_id() returns the ID of the calling tasklet.
 *
 * @param[out] id  tasklet id
 * @return Error code
 * @retval ABT_SUCCESS           on success
 * @retval ABT_ERR_UNINITIALIZED Argobots has not been initialized
 * @retval ABT_ERR_INV_XSTREAM   called by an external thread, e.g., pthread
 * @retval ABT_ERR_INV_TASK      called by a ULT
 */
int ABT_task_self_id(ABT_unit_id *id)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_xstream *p_local_xstream = ABTI_local_get_xstream();

#ifndef ABT_CONFIG_DISABLE_EXT_THREAD
    /* In case that Argobots has not been initialized or this routine is called
     * by an external thread, e.g., pthread, return an error code instead of
     * making the call fail. */
    if (gp_ABTI_global == NULL) {
        abt_errno = ABT_ERR_UNINITIALIZED;
        return abt_errno;
    }
    if (p_local_xstream == NULL) {
        abt_errno = ABT_ERR_INV_XSTREAM;
        return abt_errno;
    }
#endif

    ABTI_task *p_task = p_local_xstream->p_task;
    if (p_task != NULL) {
        *id = ABTI_task_get_id(p_task);
    } else {
        abt_errno = ABT_ERR_INV_TASK;
    }

    return abt_errno;
}

/**
 * @ingroup TASK
 * @brief   Get the ES associated with the target tasklet.
 *
 * \c ABT_task_get_xstream() returns the ES handle associated with the target
 * tasklet to \c xstream. If the target tasklet is not associated with any ES,
 * \c ABT_XSTREAM_NULL is returned to \c xstream.
 *
 * @param[in]  task     handle to the target tasklet
 * @param[out] xstream  ES handle
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_task_get_xstream(ABT_task task, ABT_xstream *xstream)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_task *p_task = ABTI_task_get_ptr(task);
    ABTI_CHECK_NULL_TASK_PTR(p_task);

    /* Return value */
    *xstream = ABTI_xstream_get_handle(p_task->unit_def.p_last_xstream);

fn_exit:
    return abt_errno;

fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

/**
 * @ingroup TASK
 * @brief   Return the state of task.
 *
 * @param[in]  task   handle to the target task
 * @param[out] state  the task's state
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_task_get_state(ABT_task task, ABT_task_state *state)
{
    int abt_errno = ABT_SUCCESS;

    ABTI_task *p_task = ABTI_task_get_ptr(task);
    ABTI_CHECK_NULL_TASK_PTR(p_task);

    /* Return value */
    *state = ABTI_unit_state_get_task_state(
        (ABTI_unit_state)ABTD_atomic_acquire_load_int(&p_task->unit_def.state));

fn_exit:
    return abt_errno;

fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

/**
 * @ingroup TASK
 * @brief   Return the last pool of task.
 *
 * If the task is not running, we get the pool where it is, else we get the
 * last pool where it was (the pool from the task was popped).
 *
 * @param[in]  task  handle to the target task
 * @param[out] pool  the last pool of the task
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_task_get_last_pool(ABT_task task, ABT_pool *pool)
{
    int abt_errno = ABT_SUCCESS;

    ABTI_task *p_task = ABTI_task_get_ptr(task);
    ABTI_CHECK_NULL_TASK_PTR(p_task);

    /* Return value */
    *pool = ABTI_pool_get_handle(p_task->unit_def.p_pool);

fn_exit:
    return abt_errno;

fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

/**
 * @ingroup TASK
 * @brief   Get the last pool's ID of the tasklet
 *
 * \c ABT_task_get_last_pool_id() returns the last pool's ID of \c task.  If
 * the tasklet is not running, this routine returns the ID of the pool where it
 * is residing.  Otherwise, it returns the ID of the last pool where the
 * tasklet was (i.e., the pool from which the tasklet was popped).
 *
 * @param[in]  task  handle to the target tasklet
 * @param[out] id    pool id
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_task_get_last_pool_id(ABT_task task, int *id)
{
    int abt_errno = ABT_SUCCESS;

    ABTI_task *p_task = ABTI_task_get_ptr(task);
    ABTI_CHECK_NULL_TASK_PTR(p_task);

    ABTI_ASSERT(p_task->unit_def.p_pool);
    *id = (int)(p_task->unit_def.p_pool->id);

fn_exit:
    return abt_errno;

fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

/**
 * @ingroup TASK
 * @brief   Set the tasklet's migratability.
 *
 * \c ABT_task_set_migratable() sets the tasklet's migratability. By default,
 * all tasklets are migratable.
 * If \c flag is \c ABT_TRUE, the target tasklet becomes migratable. On the
 * other hand, if \c flag is \c ABT_FALSE, the target tasklet becomes
 * unmigratable.
 *
 * @param[in] task  handle to the target tasklet
 * @param[in] flag  migratability flag (<tt>ABT_TRUE</tt>: migratable,
 *                  <tt>ABT_FALSE</tt>: not)
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_task_set_migratable(ABT_task task, ABT_bool flag)
{
#ifndef ABT_CONFIG_DISABLE_MIGRATION
    int abt_errno = ABT_SUCCESS;
    ABTI_task *p_task = ABTI_task_get_ptr(task);
    ABTI_CHECK_NULL_TASK_PTR(p_task);

    p_task->unit_def.migratable = flag;

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
 * @ingroup TASK
 * @brief   Get the tasklet's migratability.
 *
 * \c ABT_task_is_migratable() returns the tasklet's migratability through
 * \c flag. If the target tasklet is migratable, \c ABT_TRUE is returned to
 * \c flag. Otherwise, \c flag is set to \c ABT_FALSE.
 *
 * @param[in]  task  handle to the target tasklet
 * @param[out] flag  migratability flag (<tt>ABT_TRUE</tt>: migratable,
 *                   <tt>ABT_FALSE</tt>: not)
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_task_is_migratable(ABT_task task, ABT_bool *flag)
{
#ifndef ABT_CONFIG_DISABLE_MIGRATION
    int abt_errno = ABT_SUCCESS;
    ABTI_task *p_task = ABTI_task_get_ptr(task);
    ABTI_CHECK_NULL_TASK_PTR(p_task);

    *flag = p_task->unit_def.migratable;

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
 * @ingroup TASK
 * @brief   Compare two tasklet handles for equality.
 *
 * \c ABT_task_equal() compares two tasklet handles for equality. If two handles
 * are associated with the same tasklet object, \c result will be set to
 * \c ABT_TRUE. Otherwise, \c result will be set to \c ABT_FALSE.
 *
 * @param[in]  task1   handle to the tasklet 1
 * @param[in]  task2   handle to the tasklet 2
 * @param[out] result  comparison result (<tt>ABT_TRUE</tt>: same,
 *                     <tt>ABT_FALSE</tt>: not same)
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_task_equal(ABT_task task1, ABT_task task2, ABT_bool *result)
{
    ABTI_task *p_task1 = ABTI_task_get_ptr(task1);
    ABTI_task *p_task2 = ABTI_task_get_ptr(task2);
    *result = (p_task1 == p_task2) ? ABT_TRUE : ABT_FALSE;
    return ABT_SUCCESS;
}

/**
 * @ingroup TASK
 * @brief   Get the tasklet's id
 *
 * \c ABT_task_get_id() returns the id of \c task.
 *
 * @param[in]  task     handle to the target tasklet
 * @param[out] task_id  tasklet id
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_task_get_id(ABT_task task, ABT_unit_id *task_id)
{
    int abt_errno = ABT_SUCCESS;

    ABTI_task *p_task = ABTI_task_get_ptr(task);
    ABTI_CHECK_NULL_TASK_PTR(p_task);

    *task_id = ABTI_task_get_id(p_task);

fn_exit:
    return abt_errno;

fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

/**
 * @ingroup TASK
 * @brief   Retrieve the argument for the tasklet function
 *
 * \c ABT_task_get_arg() returns the argument for the taslet function, which was
 * passed to \c ABT_task_create() when the target tasklet \c task was created.
 *
 * @param[in]  task  handle to the target tasklet
 * @param[out] arg   argument for the tasklet function
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_task_get_arg(ABT_task task, void **arg)
{
    int abt_errno = ABT_SUCCESS;

    ABTI_task *p_task = ABTI_task_get_ptr(task);
    ABTI_CHECK_NULL_TASK_PTR(p_task);

    *arg = p_task->unit_def.p_arg;

fn_exit:
    return abt_errno;

fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

/*****************************************************************************/
/* Private APIs                                                              */
/*****************************************************************************/

static int ABTI_task_create(ABTI_xstream *p_local_xstream, ABTI_pool *p_pool,
                            void (*task_func)(void *), void *arg,
                            ABTI_sched *p_sched, int refcount,
                            ABTI_task **pp_newtask)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_task *p_newtask;
    ABT_task h_newtask;
    ABTI_CHECK_NULL_POOL_PTR(p_pool);

    /* Allocate a task object */
    p_newtask = ABTI_mem_alloc_task(p_local_xstream);

    p_newtask->unit_def.p_last_xstream = NULL;
    ABTD_atomic_relaxed_store_int(&p_newtask->unit_def.state,
                                  ABTI_UNIT_STATE_READY);
    ABTD_atomic_relaxed_store_uint32(&p_newtask->unit_def.request, 0);
    p_newtask->unit_def.f_unit = task_func;
    p_newtask->unit_def.p_arg = arg;
    p_newtask->unit_def.p_pool = p_pool;
    p_newtask->unit_def.refcount = refcount;
    p_newtask->unit_def.p_keytable = NULL;
#ifndef ABT_CONFIG_DISABLE_MIGRATION
    p_newtask->unit_def.migratable = ABT_TRUE;
#endif
    p_newtask->unit_def.id = ABTI_TASK_INIT_ID;

    /* Create a wrapper work unit */
    h_newtask = ABTI_task_get_handle(p_newtask);
    p_newtask->unit_def.type = ABTI_UNIT_TYPE_TASK;
    p_newtask->unit_def.unit = p_pool->u_create_from_task(h_newtask);

    LOG_DEBUG("[T%" PRIu64 "] created\n", ABTI_task_get_id(p_newtask));

    /* Add this task to the scheduler's pool */
#ifdef ABT_CONFIG_DISABLE_POOL_PRODUCER_CHECK
    ABTI_pool_push(p_pool, p_newtask->unit_def.unit);
#else
    abt_errno = ABTI_pool_push(p_pool, p_newtask->unit_def.unit,
                               ABTI_self_get_native_thread_id(p_local_xstream));
    if (abt_errno != ABT_SUCCESS) {
        ABTI_task_free(p_local_xstream, p_newtask);
        goto fn_fail;
    }
#endif

    /* Return value */
    *pp_newtask = p_newtask;

fn_exit:
    return abt_errno;

fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

static int ABTI_task_revive(ABTI_xstream *p_local_xstream, ABTI_pool *p_pool,
                            void (*task_func)(void *), void *arg,
                            ABTI_task *p_task)
{
    int abt_errno = ABT_SUCCESS;

    ABTI_CHECK_TRUE(ABTD_atomic_relaxed_load_int(&p_task->unit_def.state) ==
                        ABTI_UNIT_STATE_TERMINATED,
                    ABT_ERR_INV_TASK);

    p_task->unit_def.p_last_xstream = NULL;
    ABTD_atomic_relaxed_store_int(&p_task->unit_def.state,
                                  ABTI_UNIT_STATE_READY);
    ABTD_atomic_relaxed_store_uint32(&p_task->unit_def.request, 0);
    p_task->unit_def.f_unit = task_func;
    p_task->unit_def.p_arg = arg;
    p_task->unit_def.refcount = 1;
    p_task->unit_def.p_keytable = NULL;

    if (p_task->unit_def.p_pool != p_pool) {
        /* Free the unit for the old pool */
        p_task->unit_def.p_pool->u_free(&p_task->unit_def.unit);

        /* Set the new pool */
        p_task->unit_def.p_pool = p_pool;

        /* Create a wrapper work unit */
        ABT_task task = ABTI_task_get_handle(p_task);
        p_task->unit_def.unit = p_pool->u_create_from_task(task);
    }

    LOG_DEBUG("[T%" PRIu64 "] revived\n", ABTI_task_get_id(p_task));

    /* Add this task to the scheduler's pool */
#ifdef ABT_CONFIG_DISABLE_POOL_PRODUCER_CHECK
    ABTI_pool_push(p_pool, p_task->unit_def.unit);
#else
    abt_errno = ABTI_pool_push(p_pool, p_task->unit_def.unit,
                               ABTI_self_get_native_thread_id(p_local_xstream));
    ABTI_CHECK_ERROR(abt_errno);
#endif

fn_exit:
    return abt_errno;

fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

void ABTI_task_free(ABTI_xstream *p_local_xstream, ABTI_task *p_task)
{
    LOG_DEBUG("[T%" PRIu64 "] freed\n", ABTI_task_get_id(p_task));

    /* Free the unit */
    p_task->unit_def.p_pool->u_free(&p_task->unit_def.unit);

    /* Free the key-value table */
    if (p_task->unit_def.p_keytable) {
        ABTI_ktable_free(p_task->unit_def.p_keytable);
    }

    ABTI_mem_free_task(p_local_xstream, p_task);
}

void ABTI_task_print(ABTI_task *p_task, FILE *p_os, int indent)
{
    char *prefix = ABTU_get_indent_str(indent);

    if (p_task == NULL) {
        fprintf(p_os, "%s== NULL TASKLET ==\n", prefix);
        goto fn_exit;
    }

    ABTI_xstream *p_xstream = p_task->unit_def.p_last_xstream;
    int xstream_rank = p_xstream ? p_xstream->rank : 0;
    char *state;
    switch (ABTD_atomic_acquire_load_int(&p_task->unit_def.state)) {
        case ABTI_UNIT_STATE_READY:
            state = "READY";
            break;
        case ABTI_UNIT_STATE_RUNNING:
            state = "RUNNING";
            break;
        case ABTI_UNIT_STATE_TERMINATED:
            state = "TERMINATED";
            break;
        default:
            state = "UNKNOWN";
    }

    fprintf(p_os,
            "%s== TASKLET (%p) ==\n"
            "%sid        : %" PRIu64 "\n"
            "%sstate     : %s\n"
            "%sES        : %p (%d)\n"
            "%spool      : %p\n"
#ifndef ABT_CONFIG_DISABLE_MIGRATION
            "%smigratable: %s\n"
#endif
            "%srefcount  : %u\n"
            "%srequest   : 0x%x\n"
            "%sp_arg     : %p\n"
            "%skeytable  : %p\n",
            prefix, (void *)p_task, prefix, ABTI_task_get_id(p_task), prefix,
            state, prefix, (void *)p_task->unit_def.p_last_xstream,
            xstream_rank, prefix, (void *)p_task->unit_def.p_pool,
#ifndef ABT_CONFIG_DISABLE_MIGRATION
            prefix,
            (p_task->unit_def.migratable == ABT_TRUE) ? "TRUE" : "FALSE",
#endif
            prefix, p_task->unit_def.refcount, prefix,
            ABTD_atomic_acquire_load_uint32(&p_task->unit_def.request), prefix,
            p_task->unit_def.p_arg, prefix,
            (void *)p_task->unit_def.p_keytable);

fn_exit:
    fflush(p_os);
    ABTU_free(prefix);
}

static ABTD_atomic_uint64 g_task_id = ABTD_ATOMIC_UINT64_STATIC_INITIALIZER(0);
void ABTI_task_reset_id(void)
{
    ABTD_atomic_release_store_uint64(&g_task_id, 0);
}

uint64_t ABTI_task_get_id(ABTI_task *p_task)
{
    if (p_task->unit_def.id == ABTI_TASK_INIT_ID) {
        p_task->unit_def.id = ABTI_task_get_new_id();
    }
    return p_task->unit_def.id;
}

/*****************************************************************************/
/* Internal static functions                                                 */
/*****************************************************************************/

static inline ABT_unit_id ABTI_task_get_new_id(void)
{
    return ABTD_atomic_fetch_add_uint64(&g_task_id, 1);
}
