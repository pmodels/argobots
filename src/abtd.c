/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"

int ABTD_xstream_context_create(void *(*f_xstream)(void *), void *p_arg,
                                ABTD_xstream_context *p_ctx)
{
    int abt_errno = ABT_SUCCESS;
    int ret = pthread_create(p_ctx, NULL, f_xstream, p_arg);
    if (ret != 0) {
        HANDLE_ERROR("pthread_create");
        abt_errno = ABT_ERR_XSTREAM;
    }
    return abt_errno;
}

int ABTD_xstream_context_free(ABTD_xstream_context *p_ctx)
{
    int abt_errno = ABT_SUCCESS;
    /* Currently, nothing to do */
    return abt_errno;
}

int ABTD_xstream_context_join(ABTD_xstream_context ctx)
{
    int abt_errno = ABT_SUCCESS;
    int ret = pthread_join(ctx, NULL);
    if (ret != 0) {
        HANDLE_ERROR("pthread_join");
        abt_errno = ABT_ERR_XSTREAM;
    }
    return abt_errno;
}

int ABTD_xstream_context_exit()
{
    pthread_exit(NULL);
    return ABT_SUCCESS;
}

int ABTD_xstream_context_self(ABTD_xstream_context *p_ctx)
{
    int abt_errno = ABT_SUCCESS;
    *p_ctx = pthread_self();
    return abt_errno;
}

int ABTD_thread_context_create(ABTD_thread_context *p_link,
                               void (*f_thread)(void *), void *p_arg,
                               size_t stacksize, void *p_stack,
                               ABTD_thread_context *p_newctx)
{
    int abt_errno = ABT_SUCCESS;

    /* If stack is NULL, we don't need to make a new context */
    if (p_stack == NULL) goto fn_exit;

    if (getcontext(p_newctx) != 0) {
        HANDLE_ERROR("getcontext");
        abt_errno = ABT_ERR_THREAD;
        goto fn_fail;
    }

    p_newctx->uc_link = p_link;
    p_newctx->uc_stack.ss_sp = p_stack;
    p_newctx->uc_stack.ss_size = stacksize;
    makecontext(p_newctx, (void (*)())ABTI_thread_func_wrapper,
                2, f_thread, p_arg);

  fn_exit:
    return abt_errno;

  fn_fail:
    goto fn_exit;
}

int ABTD_thread_context_free(ABTD_thread_context *p_ctx)
{
    int abt_errno = ABT_SUCCESS;
    /* Currently, nothing to do */
    return abt_errno;
}

int ABTD_thread_context_switch(ABTD_thread_context *p_old,
                               ABTD_thread_context *p_new)
{
    int abt_errno = ABT_SUCCESS;
    int ret = swapcontext(p_old, p_new);
    if (ret != 0) {
        HANDLE_ERROR("swapcontext");
        abt_errno = ABT_ERR_THREAD;
    }
    return abt_errno;
}

int ABTD_thread_context_change_link(ABTD_thread_context *p_ctx,
                                    ABTD_thread_context *p_link)
{
    int abt_errno = ABT_SUCCESS;
    p_ctx->uc_link = p_link;
    return abt_errno;
}

