/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"

static ABT_Thread_id ABTI_Thread_get_new_id();
static void ABTI_Thread_func_wrapper(void (*thread_func)(void *), void *arg);

int ABT_Thread_create(const ABT_Stream stream,
                      void (*thread_func)(void *), void *arg,
                      size_t stacksize, ABT_Thread *newthread)
{
    int abt_errno = ABT_SUCCESS;
    ABTD_Stream *stream_ptr;
    ABTD_Scheduler *sched;
    ABTD_Thread *newthread_ptr;
    ABT_Thread newthread_handle;

    stream_ptr = ABTI_Stream_get_ptr(stream);
    if (stream_ptr->state == ABT_STREAM_STATE_READY) {
        abt_errno = ABTI_Stream_start(stream_ptr);
        if (abt_errno != ABT_SUCCESS) goto fn_fail;
    }

    newthread_ptr = (ABTD_Thread *)ABTU_Malloc(sizeof(ABTD_Thread));
    if (!newthread_ptr) {
        HANDLE_ERROR("ABTU_Malloc");
        *newthread = NULL;
        abt_errno = ABT_ERR_MEM;
        goto fn_fail;
    }

    newthread_ptr->stream = stream_ptr;
    newthread_ptr->id = ABTI_Thread_get_new_id();
    newthread_ptr->name = NULL;
    newthread_ptr->refcount = (newthread != NULL) ? 1 : 0;
    newthread_ptr->state = ABT_THREAD_STATE_READY;

    /* Create a stack for this thread */
    newthread_ptr->stacksize = stacksize;
    newthread_ptr->stack = ABTU_Malloc(stacksize);
    if (!newthread_ptr->stack) {
        HANDLE_ERROR("ABTU_Malloc");
        abt_errno = ABT_ERR_MEM;
        goto fn_fail;
    }

    /* Make a thread context */
    abt_errno = ABTA_ULT_make(stream_ptr, newthread_ptr, thread_func, arg);
    if (abt_errno != ABT_SUCCESS) goto fn_fail;

    /* Create a wrapper work unit */
    sched = stream_ptr->sched;
    newthread_handle = ABTI_Thread_get_handle(newthread_ptr);
    newthread_ptr->unit = sched->u_create_from_thread(newthread_handle);

    /* Add this thread to the scheduler's pool */
    ABTA_ES_lock(&stream_ptr->lock);
    sched->p_push(sched->pool, newthread_ptr->unit);
    ABTA_ES_unlock(&stream_ptr->lock);

    /* Return value */
    if (newthread) {
        *newthread = newthread_handle;
    }

  fn_exit:
    return abt_errno;

  fn_fail:
    goto fn_exit;
}

int ABT_Thread_free(ABT_Thread thread)
{
    int abt_errno = ABT_SUCCESS;
    ABTD_Thread *thread_ptr = ABTI_Thread_get_ptr(thread);
    ABTD_Stream *stream_ptr = thread_ptr->stream;
    ABTD_Scheduler *sched = stream_ptr->sched;

    if (thread_ptr->state == ABT_THREAD_STATE_TERMINATED) {
        /* The thread has finished its execution */
        if (thread_ptr->refcount > 0) {
            /* The thread has finished but it is still referenced.
             * Thus it exists in the stream's deads pool. */
            ABTI_Pool_remove(stream_ptr->deads, thread_ptr->unit);
            ABTI_Unit_free(thread_ptr->unit);
        } else {
            /* Release the associated work unit */
            sched->u_free(thread_ptr->unit);
        }
    } else {
        /* The thread should be in the scheduler's pool */
        assert(thread_ptr->state == ABT_THREAD_STATE_READY);

        sched->p_remove(sched->pool, thread_ptr->unit);
        sched->u_free(thread_ptr->unit);
    }

    /* Free thd ABTD_Thread structure */
    if (thread_ptr->name) free(thread_ptr->name);
    free(thread_ptr->stack);
    free(thread_ptr);

    return abt_errno;
}

int ABT_Thread_yield()
{
    int abt_errno = ABT_SUCCESS;

    ABTD_Scheduler *sched = g_stream->sched;
    size_t pool_size = sched->p_get_size(sched->pool);

    if (g_thread == NULL) {
        /* This is the case of main program thread
         * FIXME: Currently, the main program thread waits until all threads
         * finish their execution. */
        if (pool_size < 1) goto fn_exit;

        /* Start the scheduling */
        abt_errno = ABTI_Stream_schedule(g_stream);
    } else {
        if (pool_size == 1) goto fn_exit;

        /* Change the state of current running thread */
        g_thread->state = ABT_THREAD_STATE_READY;

        /* Switch to the scheduler */
        int ret = ABTA_ULT_swap(&g_thread->ult, &g_stream->ult);
        if (ret != ABTA_ULT_SUCCESS) {
            abt_errno = ABT_ERR_THREAD;
            goto fn_fail;
        }
    }

  fn_exit:
    return abt_errno;

  fn_fail:
    goto fn_exit;
}

int ABT_Thread_yield_to(ABT_Thread thread)
{
    int abt_errno = ABT_SUCCESS;

    ABTD_Thread *thread_ptr = ABTI_Thread_get_ptr(thread);
    assert(thread_ptr->stream == g_stream);

    ABTD_Scheduler *sched = g_stream->sched;

    if (g_thread == NULL) {
        /* This is the case of main program thread
         * FIXME: Currently, the main program thread waits until all threads
         * finish their execution. */

        g_thread = thread_ptr;

        ABTA_ES_lock(&g_stream->lock);
        sched->p_remove(sched->pool, g_thread->unit);
        ABTA_ES_unlock(&g_stream->lock);

        g_thread->state = ABT_THREAD_STATE_RUNNING;

        /* Switch the context */
        DEBUG_PRINT("[S%lu:T%lu] yield_to started\n",
                    g_stream->id, g_thread->id);
        int ret = ABTA_ULT_swap(&g_stream->ult, &g_thread->ult);
        if (ret != ABTA_ULT_SUCCESS) {
            HANDLE_ERROR("ABTA_ULT_swap");
            abt_errno = ABT_ERR_THREAD;
            goto fn_fail;
        }
        DEBUG_PRINT("[S%lu:T%lu] yield_to ended\n",
                    g_stream->id, g_thread->id);

        if (g_thread->state == ABT_THREAD_STATE_TERMINATED) {
            if (g_thread->refcount == 0) {
                ABT_Thread_free(ABTI_Thread_get_handle(g_thread));
            } else
                ABTI_Stream_keep_thread(g_stream, g_thread);
        } else {
            /* The thread did not finish its execution.
             * Add it to the pool again. */
            ABTA_ES_lock(&g_stream->lock);
            sched->p_push(sched->pool, g_thread->unit);
            ABTA_ES_unlock(&g_stream->lock);
        }

        g_thread = NULL;

        ABTI_Stream_schedule(g_stream);
    } else {
        if (thread_ptr == g_thread) goto fn_exit;

        ABTD_Thread *cur_thread = g_thread;

        /* Change the state of current running thread */
        cur_thread->state = ABT_THREAD_STATE_READY;

        ABTA_ES_lock(&g_stream->lock);
        /* Add the current thread to the pool again */
        sched->p_push(sched->pool, cur_thread->unit);

        /* Remove the work unit for thread in the pool */
        sched->p_remove(sched->pool, thread_ptr->unit);
        ABTA_ES_unlock(&g_stream->lock);

        g_thread = thread_ptr;
        g_thread->state = ABT_THREAD_STATE_RUNNING;

        /* Switch the context */
        DEBUG_PRINT("[S%lu:T%lu] yield_to started\n",
                    g_stream->id, cur_thread->id);
        int ret = ABTA_ULT_swap(&cur_thread->ult, &g_thread->ult);
        if (ret != ABTA_ULT_SUCCESS) {
            HANDLE_ERROR("ABTA_ULT_swap");
            abt_errno = ABT_ERR_THREAD;
            goto fn_fail;
        }
        DEBUG_PRINT("[S%lu:T%lu] yield_to ended\n",
                    g_stream->id, g_thread->id);
    }

  fn_exit:
    return abt_errno;

  fn_fail:
    goto fn_exit;
}

int ABT_Thread_equal(ABT_Thread thread1, ABT_Thread thread2)
{
    ABTD_Thread *thread1_ptr = ABTI_Thread_get_ptr(thread1);
    ABTD_Thread *thread2_ptr = ABTI_Thread_get_ptr(thread2);
    return thread1_ptr == thread2_ptr;
}

ABT_Thread_state ABT_Thread_get_state(ABT_Thread thread)
{
    ABTD_Thread *thread_ptr = ABTI_Thread_get_ptr(thread);
    return thread_ptr->state;
}

int ABT_Thread_set_name(ABT_Thread thread, const char *name)
{
    int abt_errno = ABT_SUCCESS;
    ABTD_Thread *thread_ptr = ABTI_Thread_get_ptr(thread);

    size_t len = strlen(name);
    if (thread_ptr->name) free(thread_ptr->name);
    thread_ptr->name = (char *)ABTU_Malloc(len + 1);
    if (!thread_ptr->name) {
        HANDLE_ERROR("ABTU_Malloc");
        abt_errno = ABT_ERR_MEM;
        goto fn_fail;
    }
    strcpy(thread_ptr->name, name);

  fn_exit:
    return abt_errno;

  fn_fail:
    goto fn_exit;
}

int ABT_Thread_get_name(ABT_Thread thread, char *name, size_t len)
{
    int abt_errno = ABT_SUCCESS;
    ABTD_Thread *thread_ptr = ABTI_Thread_get_ptr(thread);

    size_t name_len = strlen(thread_ptr->name);
    if (name_len >= len) {
        strncpy(name, thread_ptr->name, len - 1);
        name[len - 1] = '\0';
    } else {
        strncpy(name, thread_ptr->name, name_len);
        name[name_len] = '\0';
    }

    return abt_errno;
}


/* Internal static functions */
static ABT_Thread_id g_thread_id = 0;
static ABT_Thread_id ABTI_Thread_get_new_id() {
    /* FIXME: Need to be atomic */
    return g_thread_id++;
}

static void ABTI_Thread_func_wrapper(void (*thread_func)(void *), void *arg)
{
    thread_func(arg);

    /* Now, the thread has finished its job. Change the thread state. */
    g_thread->state = ABT_THREAD_STATE_TERMINATED;
}


/* Architectue- or library-dependent code */
/* FIXME: Should be separated from here */
int ABTA_ULT_make(ABTD_Stream *stream_ptr, ABTD_Thread *thread_ptr,
                  void (*thread_func)(void *), void *arg)
{
    int abt_errno = ABT_SUCCESS;

    if (getcontext(&thread_ptr->ult) != ABTA_ULT_SUCCESS) {
        HANDLE_ERROR("getcontext");
        abt_errno = ABT_ERR_THREAD;
        goto fn_fail;
    }

    thread_ptr->ult.uc_link = &stream_ptr->ult;
    thread_ptr->ult.uc_stack.ss_sp = thread_ptr->stack;
    thread_ptr->ult.uc_stack.ss_size = thread_ptr->stacksize;
    makecontext(&thread_ptr->ult, (void (*)())ABTI_Thread_func_wrapper,
            2, thread_func, arg);

  fn_exit:
    return abt_errno;

  fn_fail:
    goto fn_exit;
}
