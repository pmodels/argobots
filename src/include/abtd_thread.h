/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#ifndef ABTD_THREAD_H_INCLUDED
#define ABTD_THREAD_H_INCLUDED

#if defined(ABT_C_HAVE_VISIBILITY)
#define ABT_API_PRIVATE __attribute__((visibility("hidden")))
#else
#define ABT_API_PRIVATE
#endif

void ABTD_thread_func_wrapper_thread(void *p_arg);
void ABTD_thread_func_wrapper_sched(void *p_arg);
#if ABT_CONFIG_THREAD_TYPE == ABT_THREAD_TYPE_DYNAMIC_PROMOTION
void ABTD_thread_terminate_thread_no_arg();
#endif

static inline int ABTDI_thread_context_create(ABTD_thread_context *p_link,
                                              void (*f_wrapper)(void *),
                                              void (*f_thread)(void *),
                                              void *p_arg, size_t stacksize,
                                              void *p_stack,
                                              ABTD_thread_context *p_newctx)
{
    int abt_errno = ABT_SUCCESS;
    void *p_stacktop;

    /* ABTD_thread_context_make uses the top address of stack.
       Note that the parameter, p_stack, points to the bottom of stack. */
    p_stacktop = (void *)(((char *)p_stack) + stacksize);

    ABTD_thread_context_make(p_newctx, p_stacktop, stacksize, f_wrapper);
    p_newctx->f_thread = f_thread;
    p_newctx->p_arg = p_arg;
    p_newctx->p_link = p_link;

    return abt_errno;
}

static inline int ABTD_thread_context_create_thread(
    ABTD_thread_context *p_link, void (*f_thread)(void *), void *p_arg,
    size_t stacksize, void *p_stack, ABTD_thread_context *p_newctx)
{
    return ABTDI_thread_context_create(p_link, ABTD_thread_func_wrapper_thread,
                                       f_thread, p_arg, stacksize, p_stack,
                                       p_newctx);
}

static inline int ABTD_thread_context_create_sched(
    ABTD_thread_context *p_link, void (*f_thread)(void *), void *p_arg,
    size_t stacksize, void *p_stack, ABTD_thread_context *p_newctx)
{
    return ABTDI_thread_context_create(p_link, ABTD_thread_func_wrapper_sched,
                                       f_thread, p_arg, stacksize, p_stack,
                                       p_newctx);
}

static inline int ABTD_thread_context_invalidate(ABTD_thread_context *p_newctx)
{
    int abt_errno = ABT_SUCCESS;
#if ABT_CONFIG_THREAD_TYPE == ABT_THREAD_TYPE_DYNAMIC_PROMOTION
    /* p_ctx is used to check whether the context requires dynamic promotion is
     * necessary or not, so this value must not be NULL. */
    p_newctx->p_ctx = (void *)((intptr_t)0x1);
#else
    p_newctx->p_ctx = NULL;
#endif
    p_newctx->f_thread = NULL;
    p_newctx->p_arg = NULL;
    p_newctx->p_link = NULL;
    return abt_errno;
}

#if ABT_CONFIG_THREAD_TYPE == ABT_THREAD_TYPE_DYNAMIC_PROMOTION
static inline int ABTD_thread_context_init(ABTD_thread_context *p_link,
                                           void (*f_thread)(void *),
                                           void *p_arg,
                                           ABTD_thread_context *p_newctx)
{
    int abt_errno = ABT_SUCCESS;
    p_newctx->p_ctx = NULL;
    p_newctx->f_thread = f_thread;
    p_newctx->p_arg = p_arg;
    p_newctx->p_link = p_link;
    return abt_errno;
}

static inline int ABTD_thread_context_arm_thread(size_t stacksize,
                                                 void *p_stack,
                                                 ABTD_thread_context *p_newctx)
{
    /* This function *arms* the dynamic promotion thread (initialized by
     * ABTD_thread_context_init) as if it were created by
     * ABTD_thread_context_create; this function fully creates the context
     * so that the thread can be run by ABTD_thread_context_jump. */
    int abt_errno = ABT_SUCCESS;
    /* ABTD_thread_context_make uses the top address of stack.
       Note that the parameter, p_stack, points to the bottom of stack. */
    void *p_stacktop = (void *)(((char *)p_stack) + stacksize);
    ABTD_thread_context_make(p_newctx, p_stacktop, stacksize,
                             ABTD_thread_func_wrapper_thread);
    return abt_errno;
}
#endif

/* Currently, nothing to do */
#define ABTD_thread_context_free(p_ctx)

static inline void ABTD_thread_context_switch(ABTD_thread_context *p_old,
                                              ABTD_thread_context *p_new)
{
    ABTD_thread_context_jump(p_old, p_new, p_new);
}

static inline void ABTD_thread_finish_context(ABTD_thread_context *p_old,
                                              ABTD_thread_context *p_new)
{
    ABTD_thread_context_take(p_old, p_new, p_new);
}

#if ABT_CONFIG_THREAD_TYPE == ABT_THREAD_TYPE_DYNAMIC_PROMOTION
static inline void ABTD_thread_context_make_and_call(ABTD_thread_context *p_old,
                                                     void (*f_thread)(void *),
                                                     void *p_arg,
                                                     void *p_stacktop)
{
    ABTD_thread_context_init_and_call(p_old, p_stacktop, f_thread, p_arg);
}

static inline ABT_bool
ABTD_thread_context_is_dynamic_promoted(ABTD_thread_context *p_ctx)
{
    /* Check if the ULT has been dynamically promoted; internally, it checks if
     * the context is NULL. */
    return p_ctx->p_ctx ? ABT_TRUE : ABT_FALSE;
}

static inline void ABTDI_thread_context_dynamic_promote(void *p_stacktop,
                                                        void *jump_f)
{
    /* Perform dynamic promotion */
    void **p_return_address = (void **)(((char *)p_stacktop) - 0x10);
    void ***p_stack_pointer = (void ***)(((char *)p_stacktop) - 0x08);
    *p_stack_pointer = p_return_address;
    *p_return_address = jump_f;
}

static inline void ABTD_thread_context_dynamic_promote_thread(void *p_stacktop)
{
    union fp_conv {
        void (*f)(void *);
        void *ptr;
    } conv;
    conv.f = ABTD_thread_terminate_thread_no_arg;
    void *jump_f = conv.ptr;
    ABTDI_thread_context_dynamic_promote(p_stacktop, jump_f);
}
#endif

static inline void ABTD_thread_context_change_link(ABTD_thread_context *p_ctx,
                                                   ABTD_thread_context *p_link)
{
    ABTD_atomic_store_ptr((void **)&p_ctx->p_link, (void *)p_link);
}

static inline void ABTD_thread_context_set_arg(ABTD_thread_context *p_ctx,
                                               void *arg)
{
    p_ctx->p_arg = arg;
}

static inline void *ABTD_thread_context_get_arg(ABTD_thread_context *p_ctx)
{
    return p_ctx->p_arg;
}

#endif /* ABTD_THREAD_H_INCLUDED */
