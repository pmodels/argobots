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
    int func_upper, func_lower;
    int arg_upper, arg_lower;
    size_t ptr_size, int_size;

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

    ptr_size = sizeof(void *);
    int_size = sizeof(int);
    if (ptr_size == int_size) {
        func_upper = 0;
        func_lower = (int)(uintptr_t)f_thread;
        arg_upper = 0;
        arg_lower = (int)(uintptr_t)p_arg;
    } else if (ptr_size == int_size * 2) {
        uintptr_t shift_bits = CHAR_BIT * int_size;
        func_upper = (int)((uintptr_t)f_thread >> shift_bits);
        func_lower = (int)(uintptr_t)f_thread;
        arg_upper = (int)((uintptr_t)p_arg >> shift_bits);
        arg_lower = (int)(uintptr_t)p_arg;
    } else {
        assert(0);
    }

    makecontext(p_newctx, (void (*)())ABTI_thread_func_wrapper,
                4, func_upper, func_lower, arg_upper, arg_lower);

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

#ifdef __GLIBC__
    /* FIXME: this will work only with glibc. */
    unsigned long int *sp;
    unsigned long int idx_uc_link = 1;

    /* Calulate the position where uc_link is saved. */
    sp = (unsigned long int *)
         ((uintptr_t)p_ctx->uc_stack.ss_sp + p_ctx->uc_stack.ss_size);
    sp -= 1;
    sp = (unsigned long int *)((((uintptr_t)sp) & -16L) - 8);

    /* The value in stack must be the same as that in the thread context. */
    assert(sp[idx_uc_link] == (unsigned long int)p_ctx->uc_link);
    sp[idx_uc_link] = (unsigned long int)p_link;
#endif

    p_ctx->uc_link = p_link;

    return abt_errno;
}

