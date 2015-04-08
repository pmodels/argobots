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

    /* If the pool is directly associated to a ES and this ES is not running,
     * then start it */
    ABTI_xstream *p_xstream = p_pool->consumer;
    if (p_xstream && p_xstream->state == ABT_XSTREAM_STATE_CREATED) {
        ABT_xstream xstream = ABTI_xstream_get_handle(p_xstream);
        abt_errno = ABT_xstream_start(xstream);
        ABTI_CHECK_ERROR_MSG(abt_errno, "ABT_xstream_start");
    }

    p_newtask = (ABTI_task *)ABTU_malloc(sizeof(ABTI_task));

    p_newtask->p_xstream  = NULL;
    p_newtask->is_sched   = NULL;
    p_newtask->p_pool     = p_pool;
    p_newtask->state      = ABT_TASK_STATE_CREATED;
    p_newtask->migratable = ABT_TRUE;
    p_newtask->refcount   = (newtask != NULL) ? 1 : 0;
    p_newtask->request    = 0;
    p_newtask->f_task     = task_func;
    p_newtask->p_arg      = arg;
    p_newtask->id         = ABTI_TASK_INIT_ID;
    p_newtask->p_name     = NULL;

    /* Create a wrapper work unit */
    h_newtask = ABTI_task_get_handle(p_newtask);
    p_newtask->unit = p_pool->u_create_from_task(h_newtask);

    /* Add this task to the scheduler's pool */
    abt_errno = ABTI_pool_push(p_pool, p_newtask->unit, ABTI_xstream_self());
    if (abt_errno != ABT_SUCCESS) {
        int ret = ABT_task_free(&h_newtask);
        ABTI_CHECK_TRUE(ret == ABT_SUCCESS, ret);
        goto fn_fail;
    }

    p_newtask->state = ABT_TASK_STATE_READY;

    /* Return value */
    if (newtask) *newtask = h_newtask;

  fn_exit:
    return abt_errno;

  fn_fail:
    if (newtask) *newtask = ABT_TASK_NULL;
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

    if (p_task->refcount > 0 &&
        p_task->state != ABT_TASK_STATE_CREATED) {
        /* The task has finished but it is still referenced.
         * Thus it exists in the deads pool of ES. */
        ABTI_xstream *p_xstream = p_task->p_xstream;
        ABTI_mutex_spinlock(&p_xstream->mutex);
        ABTI_contn_remove(p_xstream->deads, p_task->unit);
        ABTI_mutex_unlock(&p_xstream->mutex);
    }

    /* Free the ABTI_task structure */
    abt_errno = ABTI_task_free(p_task);
    ABTI_CHECK_ERROR(abt_errno);

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
    ABTD_atomic_fetch_or_uint32(&p_task->request, ABTI_TASK_REQ_CANCEL);

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

/**
 * @ingroup TASK
 * @brief   Set the task's name.
 *
 * @param[in] task  handle to the target task
 * @param[in] name  task name
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_task_set_name(ABT_task task, const char *name)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_task *p_task = ABTI_task_get_ptr(task);
    ABTI_CHECK_NULL_TASK_PTR(p_task);

    size_t len = strlen(name);
    ABTI_mutex *p_mutex = &p_task->p_xstream->mutex;
    ABTI_mutex_spinlock(p_mutex);
    if (p_task->p_name) ABTU_free(p_task->p_name);
    p_task->p_name = (char *)ABTU_malloc(len + 1);
    ABTU_strcpy(p_task->p_name, name);
    ABTI_mutex_unlock(p_mutex);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

/**
 * @ingroup TASK
 * @brief   Return the name of tasklet and its length.
 *
 * \c ABT_task_get_name() gets the string name of target tasklet and the length
 * of name in bytes. If \c name is NULL, only \c len is returned.
 * If \c name is not NULL, it should have enough space to save \c len bytes of
 * characters. If \c len is NULL, \c len is ignored.
 *
 * @param[in]  task  handle to the target tasklet
 * @param[out] name  tasklet name
 * @param[out] len   the length of name in bytes
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_task_get_name(ABT_task task, char *name, size_t *len)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_task *p_task = ABTI_task_get_ptr(task);
    ABTI_CHECK_NULL_TASK_PTR(p_task);

    if (name != NULL) {
        ABTU_strcpy(name, p_task->p_name);
    }
    if (len != NULL) {
        *len = strlen(p_task->p_name);
    }

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}


/*****************************************************************************/
/* Private APIs                                                              */
/*****************************************************************************/

int ABTI_task_free(ABTI_task *p_task)
{
    int abt_errno = ABT_SUCCESS;

    /* Free the unit */
    if (p_task->refcount == 0) {
        p_task->p_pool->u_free(&p_task->unit);
    }

    if (p_task->p_name) ABTU_free(p_task->p_name);

    ABTU_free(p_task);

  fn_exit:
    return abt_errno;

  fn_fail:
    goto fn_exit;
}

int ABTI_task_print(ABTI_task *p_task)
{
    int abt_errno = ABT_SUCCESS;
    if (p_task == NULL) {
        printf("[NULL TASK]");
        goto fn_exit;
    }

    printf("[");
    printf("id:%" PRIu64 " ", ABTI_task_get_id(p_task));
    printf("xstream:%" PRIu64 " ", p_task->p_xstream->rank);
    printf("name:%s ", p_task->p_name);
    printf("state:");
    switch (p_task->state) {
        case ABT_TASK_STATE_CREATED:    printf("CREATED "); break;
        case ABT_TASK_STATE_READY:      printf("READY "); break;
        case ABT_TASK_STATE_RUNNING:    printf("RUNNING "); break;
        case ABT_TASK_STATE_TERMINATED: printf("TERMINATED "); break;
        default: printf("UNKNOWN ");
    }
    printf("refcount:%u ", p_task->refcount);
    printf("request:%x ", p_task->request);
    printf("]");

  fn_exit:
    return abt_errno;
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

