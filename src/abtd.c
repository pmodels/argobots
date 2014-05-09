/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"

int ABTD_ULT_make(ABTI_Stream *p_stream, ABTI_Thread *p_thread,
                  void (*thread_func)(void *), void *p_arg)
{
    int abt_errno = ABT_SUCCESS;

    if (getcontext(&p_thread->ult) != ABTD_ULT_SUCCESS) {
        HANDLE_ERROR("getcontext");
        abt_errno = ABT_ERR_THREAD;
        goto fn_fail;
    }

    p_thread->ult.uc_link = &p_stream->ult;
    p_thread->ult.uc_stack.ss_sp = p_thread->p_stack;
    p_thread->ult.uc_stack.ss_size = p_thread->stacksize;
    makecontext(&p_thread->ult, (void (*)())ABTI_Thread_func_wrapper,
                2, thread_func, p_arg);

  fn_exit:
    return abt_errno;

  fn_fail:
    goto fn_exit;
}
