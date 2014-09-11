/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"

static uint64_t ABTI_task_get_new_id();


/** @defgroup TASK Tasklet
 * This group is for Tasklet.
 */

/**
 * @ingroup TASK
 * @brief   Create a new task and return its handle through newtask.
 *
 * If this is ABT_XSTREAM_NULL, the new task is managed globally and it can be
 * executed by any ES. Otherwise, the task is scheduled and runs in the
 * specified ES.
 * If newtask is NULL, the task object will be automatically released when
 * this \a unnamed task completes the execution of task_func. Otherwise,
 * ABT_task_free() can be used to explicitly release the task object.
 *
 * @param[in]  xstream    handle to the associated ES
 * @param[in]  task_func  function to be executed by a new task
 * @param[in]  arg        argument for task_func
 * @param[out] newtask    handle to a newly created task
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_task_create(ABT_xstream xstream,
                    void (*task_func)(void *), void *arg,
                    ABT_task *newtask)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_task *p_newtask;
    ABT_task h_newtask;

    p_newtask = (ABTI_task *)ABTU_malloc(sizeof(ABTI_task));
    if (!p_newtask) {
        HANDLE_ERROR("ABTU_malloc");
        if (newtask) *newtask = ABT_TASK_NULL;
        abt_errno = ABT_ERR_MEM;
        goto fn_fail;
    }

    p_newtask->id       = ABTI_task_get_new_id();
    p_newtask->p_name   = NULL;
    p_newtask->state    = ABT_TASK_STATE_READY;
    p_newtask->refcount = (newtask != NULL) ? 1 : 0;
    p_newtask->request  = 0;
    p_newtask->f_task   = task_func;
    p_newtask->p_arg    = arg;

    /* Create a mutex */
    abt_errno = ABT_mutex_create(&p_newtask->mutex);
    ABTI_CHECK_ERROR(abt_errno);

    h_newtask = ABTI_task_get_handle(p_newtask);

    if (xstream == ABT_XSTREAM_NULL) {
        p_newtask->p_xstream = NULL;

        /* Create a wrapper work unit */
        p_newtask->unit = ABTI_unit_create_from_task(h_newtask);

        /* Add this task to the global task pool */
        abt_errno = ABTI_global_add_task(p_newtask);
        ABTI_CHECK_ERROR(abt_errno);

        /* Start any ES if there is no running ES */
        ABTI_xstream_pool *p_xstreams = gp_ABTI_global->p_xstreams;
        if (ABTI_pool_get_size(p_xstreams->active) <= 1 &&
            ABTI_pool_get_size(p_xstreams->created) > 0) {
            abt_errno = ABTI_xstream_start_any();
            ABTI_CHECK_ERROR(abt_errno);
        }
    } else {
        ABTI_xstream *p_xstream = ABTI_xstream_get_ptr(xstream);
        ABTI_sched *p_sched = p_xstream->p_sched;

        /* Set the ES for this task */
        p_newtask->p_xstream = p_xstream;

        /* Create a wrapper work unit */
        p_newtask->unit = p_sched->u_create_from_task(h_newtask);

        /* Add this task to the scheduler's pool */
        ABTI_sched_push(p_sched, p_newtask->unit);

        /* Start the ES if it is not running */
        if (p_xstream->state == ABT_XSTREAM_STATE_CREATED) {
            abt_errno = ABTI_xstream_start(p_xstream);
            ABTI_CHECK_ERROR(abt_errno);
        }
    }

    /* Return value */
    if (newtask) *newtask = h_newtask;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_task_create", abt_errno);
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

    if (ABTI_local_get_thread() == NULL) {
        HANDLE_ERROR("ABT_task_free cannot be called by task.");
        abt_errno = ABT_ERR_TASK;
        goto fn_fail;
    }

    /* Wait until the task terminates */
    while (p_task->state != ABT_TASK_STATE_TERMINATED) {
        ABT_thread_yield();
    }

    if (p_task->refcount > 0) {
        /* The task has finished but it is still referenced.
         * Thus it exists in the deads pool of ES. */
        ABTI_xstream *p_xstream = p_task->p_xstream;
        ABTI_mutex_waitlock(p_xstream->mutex);
        ABTI_pool_remove(p_xstream->deads, p_task->unit);
        ABT_mutex_unlock(p_xstream->mutex);
    }

    /* Free the ABTI_task structure */
    abt_errno = ABTI_task_free(p_task);
    ABTI_CHECK_ERROR(abt_errno);

    /* Return value */
    *task = ABT_TASK_NULL;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_task_free", abt_errno);
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

    if (ABTI_local_get_thread() == NULL) {
        HANDLE_ERROR("ABT_task_cancel cannot be called by task.");
        abt_errno = ABT_ERR_TASK;
        goto fn_fail;
    }

    /* Set the cancel request */
    ABTD_atomic_fetch_or_uint32(&p_task->request, ABTI_TASK_REQ_CANCEL);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_task_cancel", abt_errno);
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
 * @retval ABT_SUCCESS on success
 */
int ABT_task_self(ABT_task *task)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_CHECK_INITIALIZED();

    ABTI_task *p_task = ABTI_local_get_task();
    if (p_task != NULL) {
        ABTI_task_retain(p_task);
    }
    *task = ABTI_task_get_handle(p_task);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_task_self", abt_errno);
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
    HANDLE_ERROR_WITH_CODE("ABT_task_get_state", abt_errno);
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
    HANDLE_ERROR_WITH_CODE("ABT_task_retain", abt_errno);
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
    HANDLE_ERROR_WITH_CODE("ABT_task_release", abt_errno);
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
    ABTI_mutex_waitlock(p_task->mutex);
    if (p_task->p_name) ABTU_free(p_task->p_name);
    p_task->p_name = (char *)ABTU_malloc(len + 1);
    if (!p_task->p_name) {
        ABT_mutex_unlock(p_task->mutex);
        abt_errno = ABT_ERR_MEM;
        goto fn_fail;
    }
    ABTU_strcpy(p_task->p_name, name);
    ABT_mutex_unlock(p_task->mutex);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_task_set_name", abt_errno);
    goto fn_exit;
}

/**
 * @ingroup TASK
 * @brief   Return the task's name and its length.
 *
 * If name is NULL, only len is returned.
 *
 * @param[in]  task  handle to the target task
 * @param[out] name  task name
 * @param[out] len   the length of name in bytes
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_task_get_name(ABT_task task, char *name, size_t *len)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_task *p_task = ABTI_task_get_ptr(task);
    ABTI_CHECK_NULL_TASK_PTR(p_task);

    *len = strlen(p_task->p_name);
    if (name != NULL) {
        ABTU_strcpy(name, p_task->p_name);
    }

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_task_get_name", abt_errno);
    goto fn_exit;
}


/*****************************************************************************/
/* Private APIs                                                              */
/*****************************************************************************/

ABTI_task *ABTI_task_get_ptr(ABT_task task)
{
    ABTI_task *p_task;
    if (task == ABT_TASK_NULL) {
        p_task = NULL;
    } else {
        p_task = (ABTI_task *)task;
    }
    return p_task;
}

ABT_task ABTI_task_get_handle(ABTI_task *p_task)
{
    ABT_task h_task;
    if (p_task == NULL) {
        h_task = ABT_TASK_NULL;
    } else {
        h_task = (ABT_task)p_task;
    }
    return h_task;
}

int ABTI_task_free(ABTI_task *p_task)
{
    int abt_errno = ABT_SUCCESS;

    /* Free the unit */
    if (p_task->refcount > 0) {
        ABTI_unit_free(&p_task->unit);
    } else {
        p_task->p_xstream->p_sched->u_free(&p_task->unit);
    }

    if (p_task->p_name) ABTU_free(p_task->p_name);

    /* Free the mutex */
    abt_errno = ABT_mutex_free(&p_task->mutex);
    ABTI_CHECK_ERROR(abt_errno);

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
    printf("id:%lu ", p_task->id);
    printf("xstream:%lu ", p_task->p_xstream->rank);
    printf("name:%s ", p_task->p_name);
    printf("state:");
    switch (p_task->state) {
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


/*****************************************************************************/
/* Internal static functions                                                 */
/*****************************************************************************/

static uint64_t ABTI_task_get_new_id()
{
    static uint64_t task_id = 0;
    return ABTD_atomic_fetch_add_uint64(&task_id, 1);
}

