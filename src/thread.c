/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"

static ABTD_Thread_id ABTI_Thread_get_new_id();
static void ABTI_Thread_func_wrapper(void (*thread_func)(void *), void *arg);

int ABT_Thread_create(const ABT_Stream stream,
                      void (*thread_func)(void *), void *arg,
                      size_t stacksize, ABT_Thread *newthread)
{
    int abt_errno = ABT_SUCCESS;
    ABTD_Stream *stream_ptr;
    ABTD_Thread *newthread_ptr;

    stream_ptr = (ABTD_Stream *)stream;
    if (stream_ptr->state == ABT_STREAM_STATE_READY) {
        abt_errno = ABTI_Stream_start(stream);
        if (abt_errno != ABT_SUCCESS) goto fn_fail;
    }

    newthread_ptr = (ABTD_Thread *)ABTU_Malloc(sizeof(ABTD_Thread));
    if (!newthread_ptr) {
        HANDLE_ERROR("ABTU_Malloc");
        *newthread = NULL;
        abt_errno = ABT_ERR_MEM;
        goto fn_fail;
    }
    newthread_ptr->id = ABTI_Thread_get_new_id();
    newthread_ptr->state = ABT_THREAD_STATE_READY;
    newthread_ptr->stacksize = stacksize;
    newthread_ptr->stack = ABTU_Malloc(stacksize);
    if (!newthread_ptr->stack) {
        HANDLE_ERROR("ABTU_Malloc");
        abt_errno = ABT_ERR_MEM;
        goto fn_fail;
    }

    abt_errno = ABTA_ULT_make(stream_ptr, newthread_ptr, thread_func, arg);
    if (abt_errno != ABT_SUCCESS) goto fn_fail;

    ABTI_Stream_add_thread(stream_ptr, newthread_ptr);
    *newthread = (ABT_Thread)newthread_ptr;

  fn_exit:
    return abt_errno;

  fn_fail:
    goto fn_exit;
}

int ABT_Thread_yield()
{
    int abt_errno = ABT_SUCCESS;

    if (g_thread == NULL) {
        /* This is the case of main program thread
         * FIXME: Currently, the main program thread waits until all threads
         * finish their execution. */
        abt_errno = ABTI_Stream_schedule_main(NULL);
    } else {
        abt_errno = ABTI_Stream_schedule_to(g_stream, g_thread->next);
    }

    return abt_errno;
}

int ABT_Thread_yield_to(ABT_Thread thread)
{
    int abt_errno = ABT_SUCCESS;
    ABTD_Thread *thread_ptr = (ABTD_Thread *)thread;

    if (g_thread == NULL) {
        /* This is the case of main program thread
         * FIXME: Currently, the main program thread waits until all threads
         * finish their execution. */
        abt_errno = ABTI_Stream_schedule_main(thread_ptr);
    } else {
        abt_errno = ABTI_Stream_schedule_to(g_stream, thread_ptr);
    }

    return abt_errno;
}

ABT_Thread_state ABT_Thread_get_state(ABT_Thread thread)
{
    ABTD_Thread *thread_ptr = (ABTD_Thread *)thread;
    return thread_ptr->state;
}


/* Internal static functions */
static ABTD_Thread_id g_thread_id = 0;
static ABTD_Thread_id ABTI_Thread_get_new_id() {
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

