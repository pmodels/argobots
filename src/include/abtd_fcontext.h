/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#ifndef ABTD_FCONTEXT_H_INCLUDED
#define ABTD_FCONTEXT_H_INCLUDED

typedef struct {
    void *dummy;
} fcontext_t;

static inline int ABTDI_fcontext_is_created(const fcontext_t *p_ftx)
{
    /* Return 0 if p_ftx->dummy is NULL. */
    return !!p_ftx->dummy;
}

static inline void ABTDI_fcontext_init(fcontext_t *p_ftx)
{
    p_ftx->dummy = NULL;
}

#if defined(ABT_C_HAVE_VISIBILITY)
#define ABT_API_PRIVATE __attribute__((visibility("hidden")))
#else
#define ABT_API_PRIVATE
#endif

fcontext_t *switch_fcontext(fcontext_t *p_new_ctx,
                            fcontext_t *p_old_ctx) ABT_API_PRIVATE;
void jump_fcontext(fcontext_t *p_new_ctx,
                   fcontext_t *p_old_ctx) ABT_API_PRIVATE;
fcontext_t *init_and_switch_fcontext(fcontext_t *p_new_ctx,
                                     void (*f_thread)(fcontext_t *),
                                     void *p_stacktop,
                                     fcontext_t *p_old_ctx) ABT_API_PRIVATE;
void init_and_jump_fcontext(fcontext_t *p_new_ctx,
                            void (*f_thread)(fcontext_t *),
                            void *p_stacktop) ABT_API_PRIVATE;
void peek_fcontext(void *arg, void (*f_peek)(void *),
                   fcontext_t *p_target_ctx) ABT_API_PRIVATE;

struct ABTD_ythread_context {
    fcontext_t ctx; /* actual context of fcontext */
    void *p_stacktop;
    size_t stacksize;
    ABTD_ythread_context_atomic_ptr
        p_link; /* pointer to the waiter's context */
};

static inline ABTD_ythread_context *
ABTDI_ythread_context_get_context(fcontext_t *p_fctx)
{
    return (ABTD_ythread_context *)(((char *)p_fctx) -
                                    offsetof(ABTD_ythread_context, ctx));
}

static void ABTD_ythread_context_func_wrapper(fcontext_t *p_fctx)
{
    ABTD_ythread_context *p_ctx = ABTDI_ythread_context_get_context(p_fctx);
    ABTD_ythread_func_wrapper(p_ctx);
    /* ABTD_ythread_func_wrapper() must context-switch to another before it
     * finishes. */
    ABTU_unreachable();
}

static inline void ABTD_ythread_context_init(ABTD_ythread_context *p_ctx,
                                             void *p_stack, size_t stacksize)
{
    ABTDI_fcontext_init(&p_ctx->ctx);
    void *p_stacktop = (void *)(((char *)p_stack) + stacksize);
    p_ctx->p_stacktop = p_stacktop;
    p_ctx->stacksize = stacksize;
    ABTD_atomic_relaxed_store_ythread_context_ptr(&p_ctx->p_link, NULL);
}

static inline void ABTD_ythread_context_reinit(ABTD_ythread_context *p_ctx)
{
    ABTDI_fcontext_init(&p_ctx->ctx);
    ABTD_atomic_relaxed_store_ythread_context_ptr(&p_ctx->p_link, NULL);
}

static inline void *ABTD_ythread_context_get_stack(ABTD_ythread_context *p_ctx)
{
    void *p_stack = (void *)(((char *)p_ctx->p_stacktop) - p_ctx->stacksize);
    return p_stack;
}

static inline size_t
ABTD_ythread_context_get_stacksize(ABTD_ythread_context *p_ctx)
{
    return p_ctx->stacksize;
}

static inline ABTD_ythread_context *
ABTD_ythread_context_switch(ABTD_ythread_context *p_old,
                            ABTD_ythread_context *p_new)
{
    fcontext_t *p_fctx;
    if (ABTDI_fcontext_is_created(&p_new->ctx)) {
        /* The context is already initialized. */
        p_fctx = switch_fcontext(&p_new->ctx, &p_old->ctx);
    } else {
        /* First time. */
        p_fctx = init_and_switch_fcontext(&p_new->ctx,
                                          ABTD_ythread_context_func_wrapper,
                                          p_new->p_stacktop, &p_old->ctx);
    }
    return ABTDI_ythread_context_get_context(p_fctx);
}

ABTU_noreturn static inline void
ABTD_ythread_context_jump(ABTD_ythread_context *p_old,
                          ABTD_ythread_context *p_new)
{
    if (ABTDI_fcontext_is_created(&p_new->ctx)) {
        /* The context is already initialized. */
        jump_fcontext(&p_new->ctx, &p_old->ctx);
    } else {
        /* First time. */
        init_and_jump_fcontext(&p_new->ctx, ABTD_ythread_context_func_wrapper,
                               p_new->p_stacktop);
    }
    ABTU_unreachable();
}

static inline ABT_bool
ABTD_ythread_context_peek(ABTD_ythread_context *p_target_ctx,
                          void (*f_peek)(void *), void *arg)
{
    if (ABTDI_fcontext_is_created(&p_target_ctx->ctx)) {
        peek_fcontext(arg, f_peek, &p_target_ctx->ctx);
        return ABT_TRUE;
    } else {
        return ABT_FALSE;
    }
}

#endif /* ABTD_FCONTEXT_H_INCLUDED */
