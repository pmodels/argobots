/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#ifndef ABTD_FCONTEXT_H_INCLUDED
#define ABTD_FCONTEXT_H_INCLUDED

typedef void *fcontext_t;

#if defined(ABT_C_HAVE_VISIBILITY)
#define ABT_API_PRIVATE __attribute__((visibility("hidden")))
#else
#define ABT_API_PRIVATE
#endif

fcontext_t make_fcontext(void *sp, size_t size,
                         void (*thread_func)(void *)) ABT_API_PRIVATE;
void *jump_fcontext(fcontext_t *old, fcontext_t new, void *arg) ABT_API_PRIVATE;
void *take_fcontext(fcontext_t *old, fcontext_t new, void *arg) ABT_API_PRIVATE;

#if defined(ABT_CONFIG_ENABLE_PEEK_CONTEXT) && defined(__x86_64__)
/* We implement peek_fcontext only for x86-64. */
void peek_fcontext(fcontext_t new, void (*peek_func)(void *),
                   void *arg) ABT_API_PRIVATE;
#define ABTD_SUPPORT_PEEK_FCONTEXT 1
#endif /* defined(ABT_CONFIG_ENABLE_PEEK_CONTEXT) && defined(__x86_64__) */

struct ABTD_ythread_context {
    void *p_ctx;                            /* actual context of fcontext, or a
                                             * pointer to uctx */
    ABTD_ythread_context_atomic_ptr p_link; /* pointer to scheduler context */
    size_t stacksize;                       /* stack size */
    void *p_stacktop;                       /* top of stack */
#if defined(ABT_CONFIG_ENABLE_PEEK_CONTEXT) &&                                 \
    !defined(ABTD_CONTEXT_SUPPORT_PEEK_FCONTEXT)
    void (*thread_func)(void *);
    void *arg;
    void (*peek_func)(void *);
    void *peek_arg;
    void *p_peek_ctx;
    ABT_bool is_peeked;
#endif
};

#if defined(ABT_CONFIG_ENABLE_PEEK_CONTEXT) &&                                 \
    !defined(ABTD_CONTEXT_SUPPORT_PEEK_FCONTEXT)
static inline void ABTDI_fcontext_check_peeked(ABTD_ythread_context *p_self)
{
    /* Check if this thread is called only for peeked */
    while (ABTU_unlikely(p_self->is_peeked)) {
        p_self->peek_func(p_self->peek_arg);
        /* Reset the flag. */
        p_self->is_peeked = ABT_FALSE;
        jump_fcontext(&p_self->p_ctx, p_self->p_peek_ctx, NULL);
    }
}

static inline void ABTDI_fcontext_wrapper(void *arg)
{
    ABTD_ythread_context *p_self = (ABTD_ythread_context *)arg;
    ABTDI_fcontext_check_peeked(p_self);
    p_self->thread_func(p_self->arg);
}
#endif

static inline void ABTD_ythread_context_init(ABTD_ythread_context *p_ctx,
                                             void *p_stack, size_t stacksize)
{
    p_ctx->stacksize = stacksize;
    p_ctx->p_stacktop = (void *)(((char *)p_stack) + stacksize);
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

static inline void ABTD_ythread_context_make(ABTD_ythread_context *p_ctx,
                                             void *sp, size_t size,
                                             void (*thread_func)(void *))
{
#if defined(ABT_CONFIG_ENABLE_PEEK_CONTEXT) &&                                 \
    !defined(ABTD_CONTEXT_SUPPORT_PEEK_FCONTEXT)
    p_ctx->thread_func = thread_func;
    p_ctx->is_peeked = ABT_FALSE;
    p_ctx->p_ctx = make_fcontext(sp, size, ABTDI_fcontext_wrapper);
#else
    p_ctx->p_ctx = make_fcontext(sp, size, thread_func);
#endif
}

static inline void ABTD_ythread_context_jump(ABTD_ythread_context *p_old,
                                             ABTD_ythread_context *p_new,
                                             void *arg)
{
#if defined(ABT_CONFIG_ENABLE_PEEK_CONTEXT) &&                                 \
    !defined(ABTD_CONTEXT_SUPPORT_PEEK_FCONTEXT)
    p_new->arg = arg;
    p_old->is_peeked = ABT_FALSE;
    jump_fcontext(&p_old->p_ctx, p_new->p_ctx, p_new);
    ABTDI_fcontext_check_peeked(p_old);
#else
    jump_fcontext(&p_old->p_ctx, p_new->p_ctx, arg);
#endif
}

ABTU_noreturn static inline void
ABTD_ythread_context_take(ABTD_ythread_context *p_old,
                          ABTD_ythread_context *p_new, void *arg)
{
#if defined(ABT_CONFIG_ENABLE_PEEK_CONTEXT) &&                                 \
    !defined(ABTD_CONTEXT_SUPPORT_PEEK_FCONTEXT)
    p_new->arg = arg;
    take_fcontext(&p_old->p_ctx, p_new->p_ctx, p_new);
#else
    take_fcontext(&p_old->p_ctx, p_new->p_ctx, arg);
#endif
    ABTU_unreachable();
}

#ifdef ABT_CONFIG_ENABLE_PEEK_CONTEXT
static inline void ABTD_ythread_context_peek(ABTD_ythread_context *p_ctx,
                                             void (*peek_func)(void *),
                                             void *arg)
{
#ifdef ABTD_CONTEXT_SUPPORT_PEEK_FCONTEXT
    peek_fcontext(p_ctx->p_ctx, peek_func, *arg);
#else
    p_ctx->peek_arg = arg;
    p_ctx->peek_func = peek_func;
    p_ctx->is_peeked = ABT_TRUE;
    jump_fcontext(&p_ctx->p_peek_ctx, p_ctx->p_ctx, p_ctx);
#endif
}
#endif /*!ABT_CONFIG_ENABLE_PEEK_CONTEXT */

#endif /* ABTD_FCONTEXT_H_INCLUDED */
