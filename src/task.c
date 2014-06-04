/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"

static uint64_t ABTI_Task_get_new_id();


int ABT_Task_create(ABT_Stream stream,
                    void (*task_func)(void *), void *arg,
                    ABT_Task *newtask)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_Task *p_newtask;
    ABT_Task h_newtask;

    p_newtask = (ABTI_Task *)ABTU_Malloc(sizeof(ABTI_Task));
    if (!p_newtask) {
        HANDLE_ERROR("ABTU_Malloc");
        if (newtask) *newtask = ABT_TASK_NULL;
        abt_errno = ABT_ERR_MEM;
        goto fn_fail;
    }

    p_newtask->id       = ABTI_Task_get_new_id();
    p_newtask->p_name   = NULL;
    p_newtask->state    = ABT_TASK_STATE_CREATED;
    p_newtask->refcount = (newtask != NULL) ? 1 : 0;
    p_newtask->request  = 0;
    p_newtask->f_task   = task_func;
    p_newtask->p_arg    = arg;

    /* Create a mutex */
    abt_errno = ABT_Mutex_create(&p_newtask->mutex);
    ABTI_CHECK_ERROR(abt_errno);

    h_newtask = ABTI_Task_get_handle(p_newtask);

    if (stream == ABT_STREAM_NULL) {
        p_newtask->p_stream = NULL;

        /* Create a wrapper work unit */
        p_newtask->unit = ABTI_Unit_create_from_task(h_newtask);

        /* Add this task to the global task pool */
        abt_errno = ABTI_Global_add_task(p_newtask);
        ABTI_CHECK_ERROR(abt_errno);

        /* Start any stream if there is no running stream */
        ABTI_Stream_pool *p_streams = gp_ABTI_Global->p_streams;
        if (ABTI_Pool_get_size(p_streams->active) <= 1 &&
            ABTI_Pool_get_size(p_streams->created) > 0) {
            abt_errno = ABTI_Stream_start_any();
            ABTI_CHECK_ERROR(abt_errno);
        }
    } else {
        ABTI_Stream *p_stream = ABTI_Stream_get_ptr(stream);
        ABTI_Scheduler *p_sched = p_stream->p_sched;

        /* Set the state as DELAYED */
        p_newtask->state = ABT_TASK_STATE_DELAYED;

        /* Set the stream for this task */
        p_newtask->p_stream = p_stream;

        /* Create a wrapper work unit */
        p_newtask->unit = p_sched->u_create_from_task(h_newtask);

        /* Add this task to the scheduler's pool */
        ABTI_Scheduler_push(p_sched, p_newtask->unit);

        /* Start the stream if it is not running */
        if (p_stream->state == ABT_STREAM_STATE_CREATED) {
            abt_errno = ABTI_Stream_start(p_stream);
            ABTI_CHECK_ERROR(abt_errno);
        }
    }

    /* Return value */
    if (newtask) *newtask = h_newtask;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_Task_create", abt_errno);
    goto fn_exit;
}

int ABT_Task_free(ABT_Task *task)
{
    int abt_errno = ABT_SUCCESS;

    if (ABTI_Local_get_thread() == NULL) {
        HANDLE_ERROR("ABT_Task_free cannot be called by task.");
        abt_errno = ABT_ERR_TASK;
        goto fn_fail;
    }

    ABT_Task h_task = *task;
    ABTI_Task *p_task = ABTI_Task_get_ptr(h_task);

    /* Wait until the task terminates */
    while (p_task->state != ABT_TASK_STATE_TERMINATED) {
        ABT_Thread_yield();
    }

    if (p_task->refcount > 0) {
        /* The task has finished but it is still referenced.
         * Thus it exists in the stream's deads pool. */
        ABTI_Stream *p_stream = p_task->p_stream;
        ABTI_Mutex_waitlock(p_stream->mutex);
        ABTI_Pool_remove(p_stream->deads, p_task->unit);
        ABT_Mutex_unlock(p_stream->mutex);
    }

    /* Free the ABTI_Task structure */
    abt_errno = ABTI_Task_free(p_task);
    ABTI_CHECK_ERROR(abt_errno);

    /* Return value */
    *task = ABT_TASK_NULL;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_Task_free", abt_errno);
    goto fn_exit;
}

int ABT_Task_cancel(ABT_Task task)
{
    int abt_errno = ABT_SUCCESS;

    if (ABTI_Local_get_thread() == NULL) {
        HANDLE_ERROR("ABT_Task_cancel cannot be called by task.");
        abt_errno = ABT_ERR_TASK;
        goto fn_fail;
    }

    ABTI_Task *p_task = ABTI_Task_get_ptr(task);

    if (p_task == NULL) {
        HANDLE_ERROR("NULL TASK");
        abt_errno = ABT_ERR_INV_TASK;
        goto fn_fail;
    }

    /* Set the cancel request */
    ABTD_Atomic_fetch_or_uint32(&p_task->request, ABTI_TASK_REQ_CANCEL);

  fn_exit:
    return abt_errno;

  fn_fail:
    goto fn_exit;
}

int ABT_Task_retain(ABT_Task task)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_Task *p_task = ABTI_Task_get_ptr(task);

    if (p_task == NULL) {
        HANDLE_ERROR("NULL TASK");
        abt_errno = ABT_ERR_INV_TASK;
        goto fn_fail;
    }

    ABTD_Atomic_fetch_add_uint32(&p_task->refcount, 1);

  fn_exit:
    return abt_errno;

  fn_fail:
    goto fn_exit;
}

int ABT_Task_release(ABT_Task task)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_Task *p_task = ABTI_Task_get_ptr(task);
    uint32_t refcount;

    if (p_task == NULL) {
        HANDLE_ERROR("NULL TASK");
        abt_errno = ABT_ERR_INV_TASK;
        goto fn_fail;
    }

    while ((refcount = p_task->refcount) > 0) {
        if (ABTD_Atomic_cas_uint32(&p_task->refcount, refcount,
            refcount - 1) == refcount) {
            break;
        }
    }

  fn_exit:
    return abt_errno;

  fn_fail:
    goto fn_exit;
}

int ABT_Task_equal(ABT_Task task1, ABT_Task task2, int *result)
{
    ABTI_Task *p_task1 = ABTI_Task_get_ptr(task1);
    ABTI_Task *p_task2 = ABTI_Task_get_ptr(task2);
    *result = p_task1 == p_task2;
    return ABT_SUCCESS;
}

int ABT_Task_get_state(ABT_Task task, ABT_Task_state *state)
{
    int abt_errno = ABT_SUCCESS;

    ABTI_Task *p_task = ABTI_Task_get_ptr(task);
    if (p_task == NULL) {
        abt_errno = ABT_ERR_INV_TASK;
        goto fn_fail;
    }

    /* Return value */
    *state = p_task->state;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_Task_get_state", abt_errno);
    goto fn_exit;
}

int ABT_Task_set_name(ABT_Task task, const char *name)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_Task *p_task = ABTI_Task_get_ptr(task);
    if (p_task == NULL) {
        abt_errno = ABT_ERR_INV_TASK;
        goto fn_fail;
    }

    size_t len = strlen(name);
    ABTI_Mutex_waitlock(p_task->mutex);
    if (p_task->p_name) ABTU_Free(p_task->p_name);
    p_task->p_name = (char *)ABTU_Malloc(len + 1);
    if (!p_task->p_name) {
        ABT_Mutex_unlock(p_task->mutex);
        abt_errno = ABT_ERR_MEM;
        goto fn_fail;
    }
    ABTU_Strcpy(p_task->p_name, name);
    ABT_Mutex_unlock(p_task->mutex);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_Task_set_name", abt_errno);
    goto fn_exit;
}

int ABT_Task_get_name(ABT_Task task, char *name, size_t *len)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_Task *p_task = ABTI_Task_get_ptr(task);
    if (p_task == NULL) {
        abt_errno = ABT_ERR_INV_TASK;
        goto fn_fail;
    }

    *len = strlen(p_task->p_name);
    if (name != NULL) {
        ABTU_Strcpy(name, p_task->p_name);
    }

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_Task_get_name", abt_errno);
    goto fn_exit;
}


/* Private APIs */
ABTI_Task *ABTI_Task_get_ptr(ABT_Task task)
{
    ABTI_Task *p_task;
    if (task == ABT_TASK_NULL) {
        p_task = NULL;
    } else {
        p_task = (ABTI_Task *)task;
    }
    return p_task;
}

ABT_Task ABTI_Task_get_handle(ABTI_Task *p_task)
{
    ABT_Task h_task;
    if (p_task == NULL) {
        h_task = ABT_TASK_NULL;
    } else {
        h_task = (ABT_Task)p_task;
    }
    return h_task;
}

int ABTI_Task_free(ABTI_Task *p_task)
{
    int abt_errno = ABT_SUCCESS;

    /* Free the unit */
    if (p_task->refcount > 0) {
        ABTI_Unit_free(&p_task->unit);
    } else {
        p_task->p_stream->p_sched->u_free(&p_task->unit);
    }

    if (p_task->p_name) ABTU_Free(p_task->p_name);

    /* Free the mutex */
    abt_errno = ABT_Mutex_free(&p_task->mutex);
    ABTI_CHECK_ERROR(abt_errno);

    ABTU_Free(p_task);

  fn_exit:
    return abt_errno;

  fn_fail:
    goto fn_exit;
}

int ABTI_Task_print(ABTI_Task *p_task)
{
    int abt_errno = ABT_SUCCESS;
    if (p_task == NULL) {
        printf("[NULL TASK]");
        goto fn_exit;
    }

    printf("[");
    printf("id:%lu ", p_task->id);
    printf("stream:%lu ", p_task->p_stream->id);
    printf("name:%s ", p_task->p_name);
    printf("state:");
    switch (p_task->state) {
        case ABT_TASK_STATE_CREATED:    printf("CREATED "); break;
        case ABT_TASK_STATE_DELAYED:    printf("DELAYED "); break;
        case ABT_TASK_STATE_RUNNING:    printf("RUNNING "); break;
        case ABT_TASK_STATE_COMPLETED:  printf("COMPLETED "); break;
        case ABT_TASK_STATE_TERMINATED: printf("TERMINATED "); break;
        default: printf("UNKNOWN ");
    }
    printf("refcount:%u ", p_task->refcount);
    printf("request:%x ", p_task->request);
    printf("]");

  fn_exit:
    return abt_errno;
}


/* Internal static functions */
static uint64_t ABTI_Task_get_new_id()
{
    static uint64_t task_id = 0;
    return ABTD_Atomic_fetch_add_uint64(&task_id, 1);
}

