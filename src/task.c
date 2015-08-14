/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"

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
int ABT_task_create(ABT_pool pool,
                    void (*task_func)(void *), void *arg,
                    ABT_task *newtask)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_task *p_newtask;
    ABT_task h_newtask;
    ABTI_pool *p_pool = ABTI_pool_get_ptr(pool);
    ABTI_CHECK_NULL_POOL_PTR(p_pool);

    p_newtask = (ABTI_task *)ABTU_malloc(sizeof(ABTI_task));

    p_newtask->p_xstream  = NULL;
    p_newtask->is_sched   = NULL;
    p_newtask->p_pool     = p_pool;
    p_newtask->state      = ABT_TASK_STATE_READY;
    p_newtask->migratable = ABT_TRUE;
    p_newtask->refcount   = (newtask != NULL) ? 1 : 0;
    p_newtask->request    = 0;
    p_newtask->f_task     = task_func;
    p_newtask->p_arg      = arg;
    p_newtask->id         = ABTI_TASK_INIT_ID;

    /* Create a wrapper work unit */
    h_newtask = ABTI_task_get_handle(p_newtask);
    p_newtask->unit = p_pool->u_create_from_task(h_newtask);

    LOG_EVENT("[T%" PRIu64 "] created\n", ABTI_task_get_id(p_newtask));

    /* Add this task to the scheduler's pool */
    abt_errno = ABTI_pool_push(p_pool, p_newtask->unit, ABTI_xstream_self());
    if (abt_errno != ABT_SUCCESS) {
        p_newtask->state = ABT_TASK_STATE_CREATED;
        int ret = ABT_task_free(&h_newtask);
        ABTI_CHECK_TRUE(ret == ABT_SUCCESS, ret);
        goto fn_fail;
    }


    /* Return value */
    if (newtask) *newtask = h_newtask;

  fn_exit:
    return abt_errno;

  fn_fail:
    if (newtask) *newtask = ABT_TASK_NULL;
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

/* This routine is to create a tasklet for the scheduler. */
int ABTI_task_create_sched(ABTI_pool *p_pool, ABTI_sched *p_sched)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_task *p_newtask;
    ABT_task h_newtask;

    p_newtask = (ABTI_task *)ABTU_malloc(sizeof(ABTI_task));

    p_newtask->p_xstream  = NULL;
    p_newtask->is_sched   = p_sched;
    p_newtask->p_pool     = p_pool;
    p_newtask->state      = ABT_TASK_STATE_READY;
    p_newtask->migratable = ABT_TRUE;
    p_newtask->refcount   = 1;
    p_newtask->request    = 0;
    p_newtask->f_task     = p_sched->run;
    p_newtask->p_arg      = (void *)ABTI_sched_get_handle(p_sched);
    p_newtask->id         = ABTI_TASK_INIT_ID;

    /* Create a wrapper unit */
    h_newtask = ABTI_task_get_handle(p_newtask);
    p_newtask->unit = p_pool->u_create_from_task(h_newtask);

    LOG_EVENT("[T%" PRIu64 "] created\n", ABTI_task_get_id(p_newtask));

    /* Save the tasklet pointer in p_sched */
    p_sched->p_task = p_newtask;

    /* Add this tasklet to the pool */
    abt_errno = ABTI_pool_push(p_pool, p_newtask->unit, ABTI_xstream_self());
    if (abt_errno != ABT_SUCCESS) {
        p_sched->p_task = NULL;
        ABTI_task_free(p_newtask);
        goto fn_fail;
    }

  fn_exit:
    return abt_errno;

  fn_fail:
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
    ABT_pool pool;

    /* TODO: need to consider the access type of target pool */
    abt_errno = ABT_xstream_get_main_pools(xstream, 1, &pool);
    ABTI_CHECK_ERROR(abt_errno);

    abt_errno = ABT_task_create(pool, task_func, arg, newtask);
    ABTI_CHECK_ERROR(abt_errno);

  fn_exit:
    return abt_errno;

  fn_fail:
    if (newtask) *newtask = ABT_TASK_NULL;
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
    ABT_task h_task = *task;
    ABTI_task *p_task = ABTI_task_get_ptr(h_task);
    ABTI_CHECK_NULL_TASK_PTR(p_task);

    /* Wait until the task terminates */
    while (p_task->state != ABT_TASK_STATE_TERMINATED &&
           p_task->state != ABT_TASK_STATE_CREATED) {
        ABT_thread_yield();
    }

    /* Free the ABTI_task structure */
    ABTI_task_free(p_task);

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
 * @brief   Request the cancelation of the target task.
 *
 * @param[in] task  handle to the target task
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_task_cancel(ABT_task task)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_task *p_task = ABTI_task_get_ptr(task);
    ABTI_CHECK_NULL_TASK_PTR(p_task);

    /* Set the cancel request */
    ABTI_task_set_request(p_task, ABTI_TASK_REQ_CANCEL);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
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

    /* In case that Argobots has not been initialized or this routine is called
     * by an external thread, e.g., pthread, return an error code instead of
     * making the call fail. */
    if (gp_ABTI_global == NULL) {
        abt_errno = ABT_ERR_UNINITIALIZED;
        *task = ABT_TASK_NULL;
        goto fn_exit;
    }
    if (lp_ABTI_local == NULL) {
        abt_errno = ABT_ERR_INV_XSTREAM;
        *task = ABT_TASK_NULL;
        goto fn_exit;
    }

    ABTI_task *p_task = ABTI_local_get_task();
    if (p_task != NULL) {
        ABTI_task_retain(p_task);
        *task = ABTI_task_get_handle(p_task);
    } else {
        abt_errno = ABT_ERR_INV_TASK;
        *task = ABT_TASK_NULL;
    }

  fn_exit:
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
    *xstream = ABTI_xstream_get_handle(p_task->p_xstream);

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
    *state = p_task->state;

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
    *pool = ABTI_pool_get_handle(p_task->p_pool);

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
    int abt_errno = ABT_SUCCESS;
    ABTI_task *p_task = ABTI_task_get_ptr(task);
    ABTI_CHECK_NULL_TASK_PTR(p_task);

    p_task->migratable = flag;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
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
    int abt_errno = ABT_SUCCESS;
    ABTI_task *p_task = ABTI_task_get_ptr(task);
    ABTI_CHECK_NULL_TASK_PTR(p_task);

    *flag = p_task->migratable;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
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
 * @brief   Increment the tasklet's reference count.
 *
 * \c ABT_task_retain() increments the tasklet's reference count by one.
 * If the user obtains a tasklet handle through \c ABT_task_create() or
 * \c ABT_task_self(), those routines perform an implicit retain.
 *
 * @param[in] task  handle to the tasklet
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_task_retain(ABT_task task)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_task *p_task = ABTI_task_get_ptr(task);
    ABTI_CHECK_NULL_TASK_PTR(p_task);

    ABTI_task_retain(p_task);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

/**
 * @ingroup TASK
 * @brief   Decrement the tasklet's reference count.
 *
 * \c ABT_task_release() decrements the tasklet's reference count by one.
 * After the tasklet's reference count becomes zero, the tasklet object will
 * be freed.
 *
 * @param[in] task  handle to the tasklet
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_task_release(ABT_task task)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_task *p_task = ABTI_task_get_ptr(task);
    ABTI_CHECK_NULL_TASK_PTR(p_task);

    ABTI_task_release(p_task);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}


/*****************************************************************************/
/* Private APIs                                                              */
/*****************************************************************************/

void ABTI_task_free(ABTI_task *p_task)
{
    LOG_EVENT("[T%" PRIu64 "] freed\n", ABTI_task_get_id(p_task));

    /* Free the unit */
    p_task->p_pool->u_free(&p_task->unit);

    ABTU_free(p_task);
}

void ABTI_task_print(ABTI_task *p_task, FILE *p_os, int indent)
{
    char *prefix = ABTU_get_indent_str(indent);

    if (p_task == NULL) {
        fprintf(p_os, "%s== NULL TASKLET ==\n", prefix);
        goto fn_exit;
    }

    ABTI_xstream *p_xstream = p_task->p_xstream;
    uint64_t xstream_rank = p_xstream ? p_xstream->rank : 0;
    char *state;
    switch (p_task->state) {
        case ABT_TASK_STATE_CREATED:    state = "CREATED"; break;
        case ABT_TASK_STATE_READY:      state = "READY"; break;
        case ABT_TASK_STATE_RUNNING:    state = "RUNNING"; break;
        case ABT_TASK_STATE_TERMINATED: state = "TERMINATED"; break;
        default:                        state = "UNKNOWN";
    }

    fprintf(p_os,
        "%s== TASKLET (%p) ==\n"
        "%sid        : %" PRIu64 "\n"
        "%sstate     : %s\n"
        "%sES        : %p (%" PRIu64 ")\n"
        "%sis_sched  : %p\n"
        "%spool      : %p\n"
        "%smigratable: %s\n"
        "%srefcount  : %u\n"
        "%srequest   : 0x%x\n"
        "%sf_task    : %p\n"
        "%sp_arg     : %p\n",
        prefix, p_task,
        prefix, ABTI_task_get_id(p_task),
        prefix, state,
        prefix, p_task->p_xstream, xstream_rank,
        prefix, p_task->is_sched,
        prefix, p_task->p_pool,
        prefix, (p_task->migratable == ABT_TRUE) ? "TRUE" : "FALSE",
        prefix, p_task->refcount,
        prefix, p_task->request,
        prefix, p_task->f_task,
        prefix, p_task->p_arg
    );

  fn_exit:
    fflush(p_os);
    ABTU_free(prefix);
}

void ABTI_task_retain(ABTI_task *p_task)
{
    ABTD_atomic_fetch_add_uint32(&p_task->refcount, 1);
}

void ABTI_task_release(ABTI_task *p_task)
{
    uint32_t refcount;
    while ((refcount = p_task->refcount) > 0) {
        if (ABTD_atomic_cas_uint32(&p_task->refcount, refcount,
            refcount - 1) == refcount) {
            break;
        }
    }
}

static uint64_t g_task_id = 0;
void ABTI_task_reset_id(void)
{
    g_task_id = 0;
}

uint64_t ABTI_task_get_id(ABTI_task *p_task)
{
    if (p_task->id == ABTI_TASK_INIT_ID) {
        p_task->id = ABTI_task_get_new_id();
    }
    return p_task->id;
}

/*****************************************************************************/
/* Internal static functions                                                 */
/*****************************************************************************/

static inline uint64_t ABTI_task_get_new_id(void)
{
    return ABTD_atomic_fetch_add_uint64(&g_task_id, 1);
}

