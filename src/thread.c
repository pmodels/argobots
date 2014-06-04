/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"

static uint64_t ABTI_Thread_get_new_id();


int ABT_Thread_create(ABT_Stream stream,
                      void (*thread_func)(void *), void *arg,
                      size_t stacksize, ABT_Thread *newthread)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_Stream *p_stream;
    ABTI_Scheduler *p_sched;
    ABTI_Thread *p_newthread;
    ABT_Thread h_newthread;

    if (stream == ABT_STREAM_NULL) {
        HANDLE_ERROR("stream cannot be ABT_STREAM_NULL");
        goto fn_fail;
    }

    p_stream = ABTI_Stream_get_ptr(stream);
    if (p_stream->state == ABT_STREAM_STATE_CREATED) {
        abt_errno = ABTI_Stream_start(p_stream);
        if (abt_errno != ABT_SUCCESS) {
            HANDLE_ERROR("ABTI_Stream_start");
            goto fn_fail;
        }
    }

    p_newthread = (ABTI_Thread *)ABTU_Malloc(sizeof(ABTI_Thread));
    if (!p_newthread) {
        HANDLE_ERROR("ABTU_Malloc");
        if (newthread) *newthread = ABT_THREAD_NULL;
        abt_errno = ABT_ERR_MEM;
        goto fn_fail;
    }

    /* Create a wrapper unit */
    p_sched = p_stream->p_sched;
    h_newthread = ABTI_Thread_get_handle(p_newthread);
    p_newthread->unit = p_sched->u_create_from_thread(h_newthread);

    p_newthread->p_stream   = p_stream;
    p_newthread->id         = ABTI_Thread_get_new_id();
    p_newthread->p_name     = NULL;
    p_newthread->type       = ABTI_THREAD_TYPE_USER;
    p_newthread->state      = ABT_THREAD_STATE_READY;
    p_newthread->stacksize  = (stacksize == 0)
                            ? ABTI_THREAD_DEFAULT_STACKSIZE : stacksize;
    p_newthread->refcount   = (newthread != NULL) ? 1 : 0;
    p_newthread->f_callback = NULL;
    p_newthread->p_cb_arg   = NULL;
    p_newthread->request    = 0;
    p_newthread->p_req_arg  = NULL;

    /* Create a mutex */
    abt_errno = ABT_Mutex_create(&p_newthread->mutex);
    ABTI_CHECK_ERROR(abt_errno);

    /* Create a stack for this thread */
    p_newthread->p_stack = ABTU_Malloc(p_newthread->stacksize);
    if (!p_newthread->p_stack) {
        HANDLE_ERROR("ABTU_Malloc");
        abt_errno = ABT_ERR_MEM;
        goto fn_fail;
    }

    /* Create a thread context */
    abt_errno = ABTD_Thread_context_create(&p_sched->ctx,
            thread_func, arg, p_newthread->stacksize, p_newthread->p_stack,
            &p_newthread->ctx);
    ABTI_CHECK_ERROR(abt_errno);

    /* Add this thread to the scheduler's pool */
    ABTI_Scheduler_push(p_sched, p_newthread->unit);

    /* Return value */
    if (newthread) *newthread = h_newthread;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_Thread_create", abt_errno);
    goto fn_exit;
}

int ABT_Thread_free(ABT_Thread *thread)
{
    int abt_errno = ABT_SUCCESS;
    ABT_Thread h_thread = *thread;
    if (h_thread == ABT_THREAD_NULL) goto fn_exit;

    ABTI_Thread *p_thread = ABTI_Thread_get_ptr(h_thread);

    if (p_thread == ABTI_Local_get_thread()) {
        HANDLE_ERROR("The current thread cannot be freed.");
        abt_errno = ABT_ERR_INV_THREAD;
        goto fn_fail;
    }

    if (p_thread->type == ABTI_THREAD_TYPE_MAIN) {
        HANDLE_ERROR("The main thread cannot be freed explicitly.");
        abt_errno = ABT_ERR_INV_THREAD;
        goto fn_fail;
    }

    /* Wait until the thread terminates */
    while (p_thread->state != ABT_THREAD_STATE_TERMINATED) {
        ABT_Thread_yield();
    }

    if (p_thread->refcount > 0) {
        /* The thread has finished but it is still referenced.
         * Thus it exists in the stream's deads pool. */
        ABTI_Stream *p_stream = p_thread->p_stream;
        ABTI_Mutex_waitlock(p_stream->mutex);
        ABTI_Pool_remove(p_stream->deads, p_thread->unit);
        ABT_Mutex_unlock(p_stream->mutex);
    }

    /* Free the ABTI_Thread structure */
    abt_errno = ABTI_Thread_free(p_thread);
    ABTI_CHECK_ERROR(abt_errno);

    /* Return value */
    *thread = ABT_THREAD_NULL;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_Thread_free", abt_errno);
    goto fn_exit;
}

int ABT_Thread_join(ABT_Thread thread)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_Thread *p_thread = ABTI_Thread_get_ptr(thread);

    if (p_thread == ABTI_Local_get_thread()) {
        HANDLE_ERROR("The target thread should be different.");
        abt_errno = ABT_ERR_INV_THREAD;
        goto fn_fail;
    }

    if (p_thread->type == ABTI_THREAD_TYPE_MAIN) {
        HANDLE_ERROR("The main thread cannot be joined.");
        abt_errno = ABT_ERR_INV_THREAD;
        goto fn_fail;
    }

    while (p_thread->state != ABT_THREAD_STATE_TERMINATED) {
        ABT_Thread_yield_to(thread);
    }

  fn_exit:
    return abt_errno;

  fn_fail:
    goto fn_exit;
}

int ABT_Thread_exit()
{
    int abt_errno = ABT_SUCCESS;

    ABTI_Thread *p_thread = ABTI_Local_get_thread();

    /* Set the exit request */
    ABTD_Atomic_fetch_or_uint32(&p_thread->request, ABTI_THREAD_REQ_EXIT);

    /* Switch the context to the scheduler */
    ABT_Thread_yield();

    return abt_errno;
}

int ABT_Thread_cancel(ABT_Thread thread)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_Thread *p_thread = ABTI_Thread_get_ptr(thread);

    if (p_thread == NULL) {
        HANDLE_ERROR("NULL THREAD");
        abt_errno = ABT_ERR_INV_THREAD;
        goto fn_fail;
    }

    if (p_thread->type == ABTI_THREAD_TYPE_MAIN) {
        HANDLE_ERROR("The main thread cannot be canceled.");
        abt_errno = ABT_ERR_INV_THREAD;
        goto fn_fail;
    }

    /* Set the cancel request */
    ABTD_Atomic_fetch_or_uint32(&p_thread->request, ABTI_THREAD_REQ_CANCEL);

  fn_exit:
    return abt_errno;

  fn_fail:
    goto fn_exit;
}

int ABT_Thread_yield()
{
    int abt_errno = ABT_SUCCESS;

    ABTI_Thread *p_thread = ABTI_Local_get_thread();
    ABTI_Stream *p_stream = ABTI_Local_get_stream();
    assert(p_thread->p_stream == p_stream);

    ABTI_Scheduler *p_sched = p_stream->p_sched;
    if (p_sched->p_get_size(p_sched->pool) < 1) {
        int has_global_task;
        ABTI_Global_has_task(&has_global_task);
        if (!has_global_task) goto fn_exit;
    }

    if (p_thread->type == ABTI_THREAD_TYPE_MAIN) {
        /* Currently, the main program thread waits until all threads
         * finish their execution. */

        /* Start the scheduling */
        abt_errno = ABTI_Stream_schedule(p_stream);
        ABTI_CHECK_ERROR(abt_errno);
    } else {
        /* Change the state of current running thread */
        p_thread->state = ABT_THREAD_STATE_READY;

        /* Switch to the scheduler */
        abt_errno = ABTD_Thread_context_switch(&p_thread->ctx, &p_sched->ctx);
        ABTI_CHECK_ERROR(abt_errno);
    }

    /* Back to the original thread */
    ABTI_Local_set_thread(p_thread);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_Thread_yield", abt_errno);
    goto fn_exit;
}

int ABT_Thread_yield_to(ABT_Thread thread)
{
    int abt_errno = ABT_SUCCESS;

    ABTI_Thread *p_cur_thread = ABTI_Local_get_thread();
    ABTI_Thread *p_tar_thread = ABTI_Thread_get_ptr(thread);
    DEBUG_PRINT("YIELD_TO: TH%lu -> TH%lu\n",
                p_cur_thread->id, p_tar_thread->id);

    /* If the target thread is the same as the running thread, just keep
     * its execution. */
    if (p_cur_thread == p_tar_thread) goto fn_exit;

    if (p_tar_thread->state == ABT_THREAD_STATE_TERMINATED) {
        HANDLE_ERROR("Cannot yield to the terminated thread");
        abt_errno = ABT_ERR_INV_THREAD;
        goto fn_fail;
    }

    /* Both threads must be associated with the same stream. */
    ABTI_Stream *p_stream = p_cur_thread->p_stream;
    if (p_stream != p_tar_thread->p_stream) {
        HANDLE_ERROR("The target thread's stream is not the same as mine.");
        abt_errno = ABT_ERR_INV_THREAD;
        goto fn_fail;
    }

    ABTI_Scheduler *p_sched = p_stream->p_sched;

    if (p_cur_thread->type == ABTI_THREAD_TYPE_MAIN) {
        /* Remove the target thread from the pool */
        ABTI_Scheduler_remove(p_sched, p_tar_thread->unit);

        abt_errno = ABTI_Stream_schedule_thread(p_tar_thread);
        ABTI_CHECK_ERROR(abt_errno);

        ABTI_Local_set_thread(p_cur_thread);

        /* Yield to another thread to execute all threads */
        ABT_Thread_yield();
    } else {
        p_cur_thread->state = ABT_THREAD_STATE_READY;

        /* Add the current thread to the pool again */
        ABTI_Scheduler_push(p_sched, p_cur_thread->unit);

        /* Remove the target thread from the pool */
        ABTI_Scheduler_remove(p_sched, p_tar_thread->unit);

        /* Switch the context */
        ABTI_Local_set_thread(p_tar_thread);
        p_tar_thread->state = ABT_THREAD_STATE_RUNNING;
        abt_errno = ABTD_Thread_context_switch(&p_cur_thread->ctx,
                                               &p_tar_thread->ctx);
        ABTI_CHECK_ERROR(abt_errno);

        ABTI_Local_set_thread(p_cur_thread);
    }

  fn_exit:
    return abt_errno;

  fn_fail:
    goto fn_exit;
}

int ABT_Thread_set_callback(ABT_Thread thread,
                            void (*callback_func)(void *arg), void *arg)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_Thread *p_thread = ABTI_Thread_get_ptr(thread);
    if (p_thread == NULL) {
        HANDLE_ERROR("NULL THREAD");
        abt_errno = ABT_ERR_INV_THREAD;
        goto fn_fail;
    }

    p_thread->f_callback = callback_func;
    p_thread->p_cb_arg = arg;

  fn_exit:
    return abt_errno;

  fn_fail:
    goto fn_exit;
}

int ABT_Thread_self(ABT_Thread *thread)
{
    int abt_errno = ABT_SUCCESS;

    ABTI_Thread *p_thread = ABTI_Local_get_thread();
    if (p_thread == NULL) {
        HANDLE_ERROR("NULL THREAD");
        abt_errno = ABT_ERR_THREAD;
        goto fn_fail;
    }

    /* Return value */
    *thread = ABTI_Thread_get_handle(p_thread);

  fn_exit:
    return abt_errno;

  fn_fail:
    goto fn_exit;
}

int ABT_Thread_retain(ABT_Thread thread)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_Thread *p_thread = ABTI_Thread_get_ptr(thread);

    if (p_thread == NULL) {
        HANDLE_ERROR("NULL THREAD");
        abt_errno = ABT_ERR_INV_THREAD;
        goto fn_fail;
    }

    ABTD_Atomic_fetch_add_uint32(&p_thread->refcount, 1);

  fn_exit:
    return abt_errno;

  fn_fail:
    goto fn_exit;
}

int ABT_Thread_release(ABT_Thread thread)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_Thread *p_thread = ABTI_Thread_get_ptr(thread);
    uint32_t refcount;

    if (p_thread == NULL) {
        HANDLE_ERROR("NULL THREAD");
        abt_errno = ABT_ERR_INV_THREAD;
        goto fn_fail;
    }

    while ((refcount = p_thread->refcount) > 0) {
        if (ABTD_Atomic_cas_uint32(&p_thread->refcount, refcount,
            refcount - 1) == refcount) {
            break;
        }
    }

  fn_exit:
    return abt_errno;

  fn_fail:
    goto fn_exit;
}

int ABT_Thread_equal(ABT_Thread thread1, ABT_Thread thread2, int *result)
{
    ABTI_Thread *p_thread1 = ABTI_Thread_get_ptr(thread1);
    ABTI_Thread *p_thread2 = ABTI_Thread_get_ptr(thread2);
    *result = p_thread1 == p_thread2;
    return ABT_SUCCESS;
}

int ABT_Thread_get_state(ABT_Thread thread, ABT_Thread_state *state)
{
    int abt_errno = ABT_SUCCESS;

    ABTI_Thread *p_thread = ABTI_Thread_get_ptr(thread);
    if (p_thread == NULL) {
        abt_errno = ABT_ERR_INV_THREAD;
        goto fn_fail;
    }

    /* Return value */
    *state = p_thread->state;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_Thread_get_state", abt_errno);
    goto fn_exit;
}

int ABT_Thread_set_name(ABT_Thread thread, const char *name)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_Thread *p_thread = ABTI_Thread_get_ptr(thread);
    if (p_thread == NULL) {
        abt_errno = ABT_ERR_INV_THREAD;
        goto fn_fail;
    }

    size_t len = strlen(name);
    ABTI_Mutex_waitlock(p_thread->mutex);
    if (p_thread->p_name) ABTU_Free(p_thread->p_name);
    p_thread->p_name = (char *)ABTU_Malloc(len + 1);
    if (!p_thread->p_name) {
        ABT_Mutex_unlock(p_thread->mutex);
        abt_errno = ABT_ERR_MEM;
        goto fn_fail;
    }
    ABTU_Strcpy(p_thread->p_name, name);
    ABT_Mutex_unlock(p_thread->mutex);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_Thread_set_name", abt_errno);
    goto fn_exit;
}

int ABT_Thread_get_name(ABT_Thread thread, char *name, size_t *len)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_Thread *p_thread = ABTI_Thread_get_ptr(thread);
    if (p_thread == NULL) {
        abt_errno = ABT_ERR_INV_THREAD;
        goto fn_fail;
    }

    *len = strlen(p_thread->p_name);
    if (name != NULL) {
        ABTU_Strcpy(name, p_thread->p_name);
    }

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_Thread_get_name", abt_errno);
    goto fn_exit;
}


/* Private APIs */
ABTI_Thread *ABTI_Thread_get_ptr(ABT_Thread thread)
{
    ABTI_Thread *p_thread;
    if (thread == ABT_THREAD_NULL) {
        p_thread = NULL;
    } else {
        p_thread = (ABTI_Thread *)thread;
    }
    return p_thread;
}

ABT_Thread ABTI_Thread_get_handle(ABTI_Thread *p_thread)
{
    ABT_Thread h_thread;
    if (p_thread == NULL) {
        h_thread = ABT_THREAD_NULL;
    } else {
        h_thread = (ABT_Thread)p_thread;
    }
    return h_thread;
}

int ABTI_Thread_create_main(ABTI_Stream *p_stream, ABTI_Thread **p_thread)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_Thread *p_newthread;
    ABT_Thread h_newthread;

    p_newthread = (ABTI_Thread *)ABTU_Malloc(sizeof(ABTI_Thread));
    if (!p_newthread) {
        HANDLE_ERROR("ABTU_Malloc");
        *p_thread = NULL;
        abt_errno = ABT_ERR_MEM;
        goto fn_fail;
    }

    /* The main thread does not need to create context and stack.
     * And, it is not added to the scheduler's pool. */
    h_newthread = ABTI_Thread_get_handle(p_newthread);
    p_newthread->unit = p_stream->p_sched->u_create_from_thread(h_newthread);
    p_newthread->p_stream   = p_stream;
    p_newthread->id         = ABTI_Thread_get_new_id();
    p_newthread->p_name     = NULL;
    p_newthread->type       = ABTI_THREAD_TYPE_MAIN;
    p_newthread->state      = ABT_THREAD_STATE_RUNNING;
    p_newthread->stacksize  = ABTI_THREAD_DEFAULT_STACKSIZE;
    p_newthread->refcount   = 0;
    p_newthread->f_callback = NULL;
    p_newthread->p_cb_arg   = NULL;
    p_newthread->request    = 0;
    p_newthread->p_req_arg  = NULL;

    /* Create a mutex */
    abt_errno = ABT_Mutex_create(&p_newthread->mutex);
    if (abt_errno != ABT_SUCCESS) {
        HANDLE_ERROR("ABT_Mutex_create");
        goto fn_fail;
    }

    *p_thread = p_newthread;

  fn_exit:
    return abt_errno;

  fn_fail:
    goto fn_exit;
}

int ABTI_Thread_free_main(ABTI_Thread *p_thread)
{
    int abt_errno = ABT_SUCCESS;

    /* Free the unit */
    p_thread->p_stream->p_sched->u_free(&p_thread->unit);

    if (p_thread->p_name) ABTU_Free(p_thread->p_name);

    /* Free the mutex */
    abt_errno = ABT_Mutex_free(&p_thread->mutex);
    ABTI_CHECK_ERROR(abt_errno);

    ABTU_Free(p_thread);

  fn_exit:
    return abt_errno;

  fn_fail:
    goto fn_exit;
}

int ABTI_Thread_free(ABTI_Thread *p_thread)
{
    int abt_errno = ABT_SUCCESS;

    /* Free the unit */
    if (p_thread->refcount > 0) {
        ABTI_Unit_free(&p_thread->unit);
    } else {
        p_thread->p_stream->p_sched->u_free(&p_thread->unit);
    }

    if (p_thread->p_name) ABTU_Free(p_thread->p_name);

    /* Free the mutex */
    abt_errno = ABT_Mutex_free(&p_thread->mutex);
    ABTI_CHECK_ERROR(abt_errno);

    /* Free the stack and the context */
    ABTU_Free(p_thread->p_stack);
    abt_errno = ABTD_Thread_context_free(&p_thread->ctx);
    ABTI_CHECK_ERROR(abt_errno);

    ABTU_Free(p_thread);

  fn_exit:
    return abt_errno;

  fn_fail:
    goto fn_exit;
}

int ABTI_Thread_print(ABTI_Thread *p_thread)
{
    int abt_errno = ABT_SUCCESS;
    if (p_thread == NULL) {
        printf("[NULL THREAD]");
        goto fn_exit;
    }

    printf("[");
    printf("id:%lu ", p_thread->id);
    printf("stream:%lu ", p_thread->p_stream->id);
    printf("name:%s ", p_thread->p_name);
    printf("type:");
    switch (p_thread->type) {
        case ABTI_THREAD_TYPE_MAIN: printf("MAIN "); break;
        case ABTI_THREAD_TYPE_USER: printf("USER "); break;
        default: printf("UNKNOWN "); break;
    }
    printf("state:");
    switch (p_thread->state) {
        case ABT_THREAD_STATE_READY:      printf("READY "); break;
        case ABT_THREAD_STATE_RUNNING:    printf("RUNNING "); break;
        case ABT_THREAD_STATE_BLOCKED:    printf("BLOCKED "); break;
        case ABT_THREAD_STATE_COMPLETED:  printf("COMPLETED "); break;
        case ABT_THREAD_STATE_TERMINATED: printf("TERMINATED "); break;
        default: printf("UNKNOWN ");
    }
    printf("stacksize:%lu ", p_thread->stacksize);
    printf("refcount:%u ", p_thread->refcount);
    printf("callback:%p ", p_thread->f_callback);
    printf("cb_arg:%p ", p_thread->p_cb_arg);
    printf("request:%x ", p_thread->request);
    printf("req_arg:%p ", p_thread->p_req_arg);
    printf("stack:%p ", p_thread->p_stack);
    printf("]");

  fn_exit:
    return abt_errno;
}

void ABTI_Thread_func_wrapper(void (*thread_func)(void *), void *p_arg)
{
    thread_func(p_arg);

    /* Now, the thread has finished its job. Change the thread state. */
    ABTI_Thread *p_thread = ABTI_Local_get_thread();
    p_thread->state = ABT_THREAD_STATE_COMPLETED;
}

int ABTI_Thread_set_ready(ABT_Thread thread)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_Thread *p_thread = ABTI_Thread_get_ptr(thread);
    ABTI_Stream *p_stream = ABTI_Local_get_stream();
    ABTI_Scheduler *p_sched = p_stream->p_sched;
    p_thread->state = ABT_THREAD_STATE_READY;
    ABTI_Scheduler_push(p_sched, p_thread->unit);
    return abt_errno;
}

int ABTI_Thread_suspend()
{
    return ABT_Thread_yield();
}

ABT_Thread *ABTI_Thread_current()
{
    return (ABT_Thread *)ABTI_Local_get_thread();
}


/* Internal static functions */
static uint64_t ABTI_Thread_get_new_id() {
    static uint64_t thread_id = 0;
    return ABTD_Atomic_fetch_add_uint64(&thread_id, 1);
}

