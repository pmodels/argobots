/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#ifndef ABTD_UCONTEXT_H_INCLUDED
#define ABTD_UCONTEXT_H_INCLUDED

static void ABTD_ucontext_wrapper(int arg1, int arg2)
{
    ABTD_thread_context *p_self;
    if (sizeof(void *) == 8) {
        p_self = (ABTD_thread_context *)(((uintptr_t)((uint32_t)arg1) << 32) |
                                         ((uintptr_t)((uint32_t)arg2)));
    } else if (sizeof(void *) == 4) {
        p_self = (ABTD_thread_context *)((uintptr_t)arg1);
    } else {
        ABTI_ASSERT(0);
    }
    p_self->f_uctx_thread(p_self->p_uctx_arg);
    /* ABTD_thread_context_jump or take must be called at the end of
     * f_uctx_thread, */
    ABTI_ASSERT(0);
}

static inline
void ABTD_thread_context_make(ABTD_thread_context *p_ctx, void *sp, size_t size,
                              void (*thread_func)(void *))
{
    getcontext(&p_ctx->uctx);
    p_ctx->p_ctx = &p_ctx->uctx;

    /* uc_link is not used. */
    p_ctx->uctx.uc_link = NULL;
    p_ctx->uctx.uc_stack.ss_sp = (void *)(((char *)sp) - size);
    p_ctx->uctx.uc_stack.ss_size = size;
    p_ctx->f_uctx_thread = thread_func;

    if (sizeof(void *) == 8) {
        int arg_upper = (int)(((uintptr_t)p_ctx) >> 32);
        int arg_lower = (int)(((uintptr_t)p_ctx) >> 0);
        makecontext(&p_ctx->uctx, (void (*)())ABTD_ucontext_wrapper, 2,
                    arg_upper, arg_lower);
    } else if (sizeof(void *) == 4) {
        int arg = (int)((uintptr_t)p_ctx);
        makecontext(&p_ctx->uctx, (void (*)())ABTD_ucontext_wrapper, 1, arg);
    } else {
        ABTI_ASSERT(0);
    }
}

static inline
void ABTD_thread_context_jump(ABTD_thread_context *p_old,
                              ABTD_thread_context *p_new, void *arg)
{
    p_new->p_uctx_arg = arg;
    swapcontext(&p_old->uctx, &p_new->uctx);
}

static inline
void ABTD_thread_context_take(ABTD_thread_context *p_old,
                              ABTD_thread_context *p_new, void *arg)
{
    p_new->p_uctx_arg = arg;
    setcontext(&p_new->uctx);
    /* Unreachable. */
}

#if ABT_CONFIG_THREAD_TYPE == ABT_THREAD_TYPE_DYNAMIC_PROMOTION
#error "ABTD_thread_context_make_and_call is not implemented."
#endif

#endif /* ABTD_UCONTEXT_H_INCLUDED */
