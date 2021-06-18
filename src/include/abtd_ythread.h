/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#ifndef ABTD_YTHREAD_H_INCLUDED
#define ABTD_YTHREAD_H_INCLUDED

#if defined(ABT_C_HAVE_VISIBILITY)
#define ABT_API_PRIVATE __attribute__((visibility("hidden")))
#else
#define ABT_API_PRIVATE
#endif

void ABTD_ythread_func_wrapper(void *p_arg);

static inline void ABTD_ythread_context_create(ABTD_ythread_context *p_link,
                                               size_t stacksize, void *p_stack,
                                               ABTD_ythread_context *p_newctx)
{
    /* ABTD_ythread_context_make uses the top address of stack.
       Note that the parameter, p_stack, points to the bottom of stack. */
    void *p_stacktop = (void *)(((char *)p_stack) + stacksize);

    ABTD_ythread_context_make(p_newctx, p_stacktop, stacksize,
                              ABTD_ythread_func_wrapper);
    ABTD_atomic_relaxed_store_ythread_context_ptr(&p_newctx->p_link, p_link);
}

static inline void
ABTD_ythread_context_invalidate(ABTD_ythread_context *p_newctx)
{
    p_newctx->p_ctx = NULL;
    ABTD_atomic_relaxed_store_ythread_context_ptr(&p_newctx->p_link, NULL);
}

static inline void ABTD_ythread_context_switch(ABTD_ythread_context *p_old,
                                               ABTD_ythread_context *p_new)
{
    ABTD_ythread_context_jump(p_old, p_new, p_new);
}

ABTU_noreturn static inline void
ABTD_ythread_finish_context(ABTD_ythread_context *p_old,
                            ABTD_ythread_context *p_new)
{
    ABTD_ythread_context_take(p_old, p_new, p_new);
}

#endif /* ABTD_YTHREAD_H_INCLUDED */
