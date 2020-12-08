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
#if ABT_CONFIG_THREAD_TYPE == ABT_THREAD_TYPE_DYNAMIC_PROMOTION
void init_and_call_fcontext(void *p_arg, void (*f_thread)(void *),
                            void *p_stacktop, fcontext_t *old);
#endif

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

#if ABT_CONFIG_THREAD_TYPE == ABT_THREAD_TYPE_DYNAMIC_PROMOTION
static inline void
ABTD_ythread_context_init_and_call(ABTD_ythread_context *p_ctx, void *sp,
                                   void (*thread_func)(void *), void *arg)
{
#if defined(ABT_CONFIG_ENABLE_PEEK_CONTEXT) &&                                 \
    !defined(ABTD_CONTEXT_SUPPORT_PEEK_FCONTEXT)
    p_ctx->is_peeked = ABT_FALSE;
    init_and_call_fcontext(arg, thread_func, sp, &p_ctx->p_ctx);
    ABTDI_fcontext_check_peeked(p_ctx);
#else
    init_and_call_fcontext(arg, thread_func, sp, &p_ctx->p_ctx);
#endif
}
#endif

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
