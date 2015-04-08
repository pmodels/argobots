/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"

#if defined(ABT_CONFIG_USE_FCONTEXT)
static void ABTD_thread_func_wrapper(void *p_arg);
fcontext_t make_fcontext(void *sp, size_t size, void (*thread_func)(void *));
void *jump_fcontext(fcontext_t *old, fcontext_t new, void *arg,
                    int preserve_fpu);
#else
static void ABTD_thread_func_wrapper(int func_upper, int func_lower,
                                     int arg_upper, int arg_lower);
#endif


int ABTD_thread_context_create(ABTD_thread_context *p_link,
                               void (*f_thread)(void *), void *p_arg,
                               size_t stacksize, void *p_stack,
                               ABTD_thread_context *p_newctx)
{
    int abt_errno = ABT_SUCCESS;
#if defined(ABT_CONFIG_USE_FCONTEXT)
    void *p_stacktop;

    /* If stack is NULL, we don't need to make a new context */
    if (p_stack == NULL) goto fn_exit;

    /* fcontext uses the top address of stack.
       Note that the parameter, p_stack, points to the bottom of stack. */
    p_stacktop = (void *)(((char *)p_stack) + stacksize);

    p_newctx->fctx = make_fcontext(p_stacktop, stacksize,
                                   ABTD_thread_func_wrapper);
    p_newctx->f_thread = f_thread;
    p_newctx->p_arg = p_arg;
    p_newctx->p_link = p_link;

  fn_exit:
    return abt_errno;

#else
    int func_upper, func_lower;
    int arg_upper, arg_lower;
    size_t ptr_size, int_size;

    /* If stack is NULL, we don't need to make a new context */
    if (p_stack == NULL) goto fn_exit;

    abt_errno = getcontext(p_newctx);
    ABTI_CHECK_TRUE(!abt_errno, ABT_ERR_THREAD);

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
        ABTI_ASSERT(0);
    }

    makecontext(p_newctx, (void (*)())ABTD_thread_func_wrapper,
                4, func_upper, func_lower, arg_upper, arg_lower);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
#endif
}

int ABTD_thread_context_free(ABTD_thread_context *p_ctx)
{
    ABTI_UNUSED(p_ctx);
    int abt_errno = ABT_SUCCESS;
    /* Currently, nothing to do */
    return abt_errno;
}

int ABTD_thread_context_switch(ABTD_thread_context *p_old,
                               ABTD_thread_context *p_new)
{
    int abt_errno = ABT_SUCCESS;

#if defined(ABT_CONFIG_USE_FCONTEXT)
    jump_fcontext(&p_old->fctx, p_new->fctx, p_new,
                  ABTD_FCONTEXT_PRESERVE_FPU);

#else
    int ret = swapcontext(p_old, p_new);
    if (ret != 0) {
        HANDLE_ERROR("swapcontext");
        abt_errno = ABT_ERR_THREAD;
    }
#endif

    return abt_errno;
}

int ABTD_thread_context_change_link(ABTD_thread_context *p_ctx,
                                    ABTD_thread_context *p_link)
{
    int abt_errno = ABT_SUCCESS;

#if defined(ABT_CONFIG_USE_FCONTEXT)
    p_ctx->p_link = p_link;

#else
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
    ABTI_ASSERT(sp[idx_uc_link] == (unsigned long int)p_ctx->uc_link);
    sp[idx_uc_link] = (unsigned long int)p_link;
#endif

    p_ctx->uc_link = p_link;
#endif

    return abt_errno;
}

#if defined(ABT_CONFIG_USE_FCONTEXT)
static void ABTD_thread_func_wrapper(void *p_arg)
{
    ABTD_thread_context *p_fctx = (ABTD_thread_context *)p_arg;
    void (*thread_func)(void *) = p_fctx->f_thread;

    thread_func(p_fctx->p_arg);

    /* Now, the ULT has finished its job. Terminate the ULT.
     * We don't need to use the atomic operation here because the ULT will be
     * terminated regardless of other requests. */
    ABTI_thread *p_thread = ABTI_local_get_thread();
    p_thread->request |= ABTI_THREAD_REQ_TERMINATE;

    /* Since fcontext does not switch to the other fcontext when it finishes,
       we need to explicitly switch to the scheduler. */
    jump_fcontext(&p_fctx->fctx, p_fctx->p_link->fctx, NULL,
                  ABTD_FCONTEXT_PRESERVE_FPU);
}
#else
static void ABTD_thread_func_wrapper(int func_upper, int func_lower,
                                     int arg_upper, int arg_lower)
{
    void (*thread_func)(void *);
    void *p_arg;
    size_t ptr_size, int_size;

    ptr_size = sizeof(void *);
    int_size = sizeof(int);
    if (ptr_size == int_size) {
        thread_func = (void (*)(void *))(uintptr_t)func_lower;
        p_arg = (void *)(uintptr_t)arg_lower;
    } else if (ptr_size == int_size * 2) {
        uintptr_t shift_bits = CHAR_BIT * int_size;
        uintptr_t mask = ((uintptr_t)1 << shift_bits) - 1;
        thread_func = (void (*)(void *))(
                ((uintptr_t)func_upper << shift_bits) |
                ((uintptr_t)func_lower & mask));
        p_arg = (void *)(
                ((uintptr_t)arg_upper << shift_bits) |
                ((uintptr_t)arg_lower & mask));
    } else {
        ABTI_ASSERT(0);
    }

    thread_func(p_arg);

    /* Now, the ULT has finished its job. Terminate the ULT.
     * We don't need to use the atomic operation here because the ULT will be
     * terminated regardless of other requests. */
    ABTI_thread *p_thread = ABTI_local_get_thread();
    p_thread->request |= ABTI_THREAD_REQ_TERMINATE;
}
#endif

