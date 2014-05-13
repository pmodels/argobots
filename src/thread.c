/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"

static ABT_Thread_id ABTI_Thread_get_new_id();

int ABT_Thread_create(const ABT_Stream stream,
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
        if (newthread) *newthread = NULL;
        abt_errno = ABT_ERR_MEM;
        goto fn_fail;
    }

    p_newthread->p_stream = p_stream;
    p_newthread->id       = ABTI_Thread_get_new_id();
    p_newthread->p_name   = NULL;
    p_newthread->refcount = (newthread != NULL) ? 1 : 0;
    p_newthread->state    = ABT_THREAD_STATE_READY;

    /* Create a stack for this thread */
    p_newthread->stacksize = (stacksize == 0) ? ABTI_THREAD_DEFAULT_STACKSIZE
                           : stacksize;
    p_newthread->p_stack = ABTU_Malloc(p_newthread->stacksize);
    if (!p_newthread->p_stack) {
        HANDLE_ERROR("ABTU_Malloc");
        abt_errno = ABT_ERR_MEM;
        goto fn_fail;
    }

    /* Make a thread context */
    abt_errno = ABTD_ULT_make(p_stream, p_newthread, thread_func, arg);
    if (abt_errno != ABT_SUCCESS) goto fn_fail;

    /* Create a wrapper work unit */
    p_sched = p_stream->p_sched;
    h_newthread = ABTI_Thread_get_handle(p_newthread);
    p_newthread->unit = p_sched->u_create_from_thread(h_newthread);

    /* Add this thread to the scheduler's pool */
    ABTD_ES_lock(&p_stream->lock);
    p_sched->p_push(p_sched->pool, p_newthread->unit);
    ABTD_ES_unlock(&p_stream->lock);

    /* Return value */
    if (newthread) *newthread = h_newthread;

  fn_exit:
    return abt_errno;

  fn_fail:
    goto fn_exit;
}

int ABT_Thread_free(ABT_Thread thread)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_Thread *p_thread = ABTI_Thread_get_ptr(thread);
    ABTI_Stream *p_stream = p_thread->p_stream;
    ABTI_Scheduler *p_sched = p_stream->p_sched;

    if (p_thread->state == ABT_THREAD_STATE_TERMINATED) {
        /* The thread has finished its execution */
        if (p_thread->refcount > 0) {
            /* The thread has finished but it is still referenced.
             * Thus it exists in the stream's deads pool. */
            ABTI_Pool_remove(p_stream->deads, p_thread->unit);
            ABTI_Unit_free(p_thread->unit);
        } else {
            /* Release the associated work unit */
            p_sched->u_free(p_thread->unit);
        }
    } else {
        /* The thread should be in the scheduler's pool */
        assert(p_thread->state == ABT_THREAD_STATE_READY);

        p_sched->p_remove(p_sched->pool, p_thread->unit);
        p_sched->u_free(p_thread->unit);
    }

    /* Free thd ABTI_Thread structure */
    if (p_thread->p_name) ABTU_Free(p_thread->p_name);
    ABTU_Free(p_thread->p_stack);
    ABTU_Free(p_thread);

    return abt_errno;
}

int ABT_Thread_yield()
{
    int abt_errno = ABT_SUCCESS;

    ABTI_Scheduler *p_sched = gp_stream->p_sched;
    size_t pool_size = p_sched->p_get_size(p_sched->pool);

    if (gp_thread == NULL) {
        /* This is the case of main program thread
         * FIXME: Currently, the main program thread waits until all threads
         * finish their execution. */
        if (pool_size < 1) goto fn_exit;

        /* Start the scheduling */
        abt_errno = ABTI_Stream_schedule(gp_stream);
    } else {
        if (pool_size < 1) goto fn_exit;

        /* Change the state of current running thread */
        gp_thread->state = ABT_THREAD_STATE_READY;

        /* Switch to the scheduler */
        int ret = ABTD_ULT_swap(&gp_thread->ult, &gp_stream->ult);
        if (ret != ABTD_ULT_SUCCESS) {
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

    ABTI_Thread *p_thread = ABTI_Thread_get_ptr(thread);
    assert(p_thread->p_stream == gp_stream);

    ABTI_Scheduler *p_sched = gp_stream->p_sched;

    if (gp_thread == NULL) {
        /* This is the case of main program thread
         * FIXME: Currently, the main program thread waits until all threads
         * finish their execution. */

        gp_thread = p_thread;

        ABTD_ES_lock(&gp_stream->lock);
        p_sched->p_remove(p_sched->pool, gp_thread->unit);
        ABTD_ES_unlock(&gp_stream->lock);

        gp_thread->state = ABT_THREAD_STATE_RUNNING;

        /* Switch the context */
        DEBUG_PRINT("[S%lu:T%lu] yield_to started\n",
                    gp_stream->id, gp_thread->id);
        int ret = ABTD_ULT_swap(&gp_stream->ult, &gp_thread->ult);
        if (ret != ABTD_ULT_SUCCESS) {
            HANDLE_ERROR("ABTD_ULT_swap");
            abt_errno = ABT_ERR_THREAD;
            goto fn_fail;
        }
        DEBUG_PRINT("[S%lu:T%lu] yield_to ended\n",
                    gp_stream->id, gp_thread->id);

        if (gp_thread->state == ABT_THREAD_STATE_TERMINATED) {
            if (gp_thread->refcount == 0) {
                ABT_Thread_free(ABTI_Thread_get_handle(gp_thread));
            } else
                ABTI_Stream_keep_thread(gp_stream, gp_thread);
        } else {
            /* The thread did not finish its execution.
             * Add it to the pool again. */
            ABTD_ES_lock(&gp_stream->lock);
            p_sched->p_push(p_sched->pool, gp_thread->unit);
            ABTD_ES_unlock(&gp_stream->lock);
        }

        gp_thread = NULL;

        ABTI_Stream_schedule(gp_stream);
    } else {
        if (p_thread == gp_thread) goto fn_exit;

        ABTI_Thread *p_cthread = gp_thread;

        /* Change the state of current running thread */
        p_cthread->state = ABT_THREAD_STATE_READY;

        ABTD_ES_lock(&gp_stream->lock);
        /* Add the current thread to the pool again */
        p_sched->p_push(p_sched->pool, p_cthread->unit);

        /* Remove the work unit for thread in the pool */
        p_sched->p_remove(p_sched->pool, p_thread->unit);
        ABTD_ES_unlock(&gp_stream->lock);

        gp_thread = p_thread;
        gp_thread->state = ABT_THREAD_STATE_RUNNING;

        /* Switch the context */
        DEBUG_PRINT("[S%lu:T%lu] yield_to started\n",
                    gp_stream->id, p_cthread->id);
        int ret = ABTD_ULT_swap(&p_cthread->ult, &gp_thread->ult);
        if (ret != ABTD_ULT_SUCCESS) {
            HANDLE_ERROR("ABTD_ULT_swap");
            abt_errno = ABT_ERR_THREAD;
            goto fn_fail;
        }
        DEBUG_PRINT("[S%lu:T%lu] yield_to ended\n",
                    gp_stream->id, gp_thread->id);
    }

  fn_exit:
    return abt_errno;

  fn_fail:
    goto fn_exit;
}

int ABT_Thread_equal(ABT_Thread thread1, ABT_Thread thread2)
{
    ABTI_Thread *p_thread1 = ABTI_Thread_get_ptr(thread1);
    ABTI_Thread *p_thread2 = ABTI_Thread_get_ptr(thread2);
    return p_thread1 == p_thread2;
}

ABT_Thread_state ABT_Thread_get_state(ABT_Thread thread)
{
    ABTI_Thread *p_thread = ABTI_Thread_get_ptr(thread);
    return p_thread->state;
}

int ABT_Thread_set_name(ABT_Thread thread, const char *name)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_Thread *p_thread = ABTI_Thread_get_ptr(thread);

    size_t len = strlen(name);
    if (p_thread->p_name) ABTU_Free(p_thread->p_name);
    p_thread->p_name = (char *)ABTU_Malloc(len + 1);
    if (!p_thread->p_name) {
        HANDLE_ERROR("ABTU_Malloc");
        abt_errno = ABT_ERR_MEM;
        goto fn_fail;
    }
    ABTU_Strcpy(p_thread->p_name, name);

  fn_exit:
    return abt_errno;

  fn_fail:
    goto fn_exit;
}

int ABT_Thread_get_name(ABT_Thread thread, char *name, size_t len)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_Thread *p_thread = ABTI_Thread_get_ptr(thread);

    size_t name_len = strlen(p_thread->p_name);
    if (name_len >= len) {
        ABTU_Strncpy(name, p_thread->p_name, len - 1);
        name[len - 1] = '\0';
    } else {
        ABTU_Strncpy(name, p_thread->p_name, name_len);
        name[name_len] = '\0';
    }

    return abt_errno;
}

void ABTI_Thread_func_wrapper(void (*thread_func)(void *), void *p_arg)
{
    thread_func(p_arg);

    /* Now, the thread has finished its job. Change the thread state. */
    gp_thread->state = ABT_THREAD_STATE_COMPLETED;
}


/* Internal static functions */
static ABT_Thread_id g_thread_id = 0;
static ABT_Thread_id ABTI_Thread_get_new_id() {
    ABT_Thread_id new_id;

    ABTI_Stream_pool *p_streams = gp_ABT->p_streams;
    ABTD_ES_lock(&p_streams->lock);
    new_id = g_thread_id++;
    ABTD_ES_unlock(&p_streams->lock);

    return new_id;
}

