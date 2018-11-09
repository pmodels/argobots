/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#ifndef ABTD_THREAD_H_INCLUDED
#define ABTD_THREAD_H_INCLUDED

#if defined(ABT_C_HAVE_VISIBILITY)
#define ABT_API_PRIVATE     __attribute__((visibility ("hidden")))
#else
#define ABT_API_PRIVATE
#endif

#if defined(ABT_CONFIG_USE_FCONTEXT)
void ABTD_thread_func_wrapper_thread(void *p_arg);
void ABTD_thread_func_wrapper_sched(void *p_arg);
fcontext_t make_fcontext(void *sp, size_t size, void (*thread_func)(void *))
                         ABT_API_PRIVATE;
void *jump_fcontext(fcontext_t *old, fcontext_t new, void *arg) ABT_API_PRIVATE;
void *take_fcontext(fcontext_t *old, fcontext_t new, void *arg) ABT_API_PRIVATE;
#if ABT_CONFIG_THREAD_TYPE == ABT_THREAD_TYPE_DYNAMIC_PROMOTION
void init_and_call_fcontext(void *p_arg, void (*f_thread)(void *),
                            void *p_stacktop, fcontext_t *old);
void ABTD_thread_terminate_thread_no_arg();
#endif
#else
void ABTD_thread_func_wrapper(int func_upper, int func_lower,
                              int arg_upper, int arg_lower);
#define ABTD_thread_func_wrapper_thread ABTD_thread_func_wrapper
#define ABTD_thread_func_wrapper_sched  ABTD_thread_func_wrapper
#endif

static inline
int ABTDI_thread_context_create(ABTD_thread_context *p_link,
                               void (*f_wrapper)(void *),
                               void (*f_thread)(void *), void *p_arg,
                               size_t stacksize, void *p_stack,
                               ABTD_thread_context *p_newctx)
{
    int abt_errno = ABT_SUCCESS;
#if defined(ABT_CONFIG_USE_FCONTEXT)
    void *p_stacktop;

    /* fcontext uses the top address of stack.
       Note that the parameter, p_stack, points to the bottom of stack. */
    p_stacktop = (void *)(((char *)p_stack) + stacksize);

    p_newctx->fctx = make_fcontext(p_stacktop, stacksize, f_wrapper);
    p_newctx->f_thread = f_thread;
    p_newctx->p_arg = p_arg;
    p_newctx->p_link = p_link;

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

    makecontext(p_newctx, (void (*)())f_wrapper, 4, func_upper, func_lower,
                arg_upper, arg_lower);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
#endif
}

static inline
int ABTD_thread_context_create_thread(ABTD_thread_context *p_link,
                                      void (*f_thread)(void *), void *p_arg,
                                      size_t stacksize, void *p_stack,
                                      ABTD_thread_context *p_newctx)
{
    return ABTDI_thread_context_create(p_link, ABTD_thread_func_wrapper_thread,
                                       f_thread, p_arg, stacksize, p_stack,
                                       p_newctx);
}

static inline
int ABTD_thread_context_create_sched(ABTD_thread_context *p_link,
                                     void (*f_thread)(void *), void *p_arg,
                                     size_t stacksize, void *p_stack,
                                     ABTD_thread_context *p_newctx)
{
    return ABTDI_thread_context_create(p_link, ABTD_thread_func_wrapper_sched,
                                       f_thread, p_arg, stacksize, p_stack,
                                       p_newctx);
}

static inline
int ABTD_thread_context_invalidate(ABTD_thread_context *p_newctx)
{
    int abt_errno = ABT_SUCCESS;
#if defined(ABT_CONFIG_USE_FCONTEXT)
#if ABT_CONFIG_THREAD_TYPE == ABT_THREAD_TYPE_DYNAMIC_PROMOTION
    /* fctx is used to check whether the context requires dynamic promotion is
     * necessary or not, so this value must not be NULL. */
    p_newctx->fctx = (void *)((intptr_t)0x1);
#else
    p_newctx->fctx = NULL;
#endif
    p_newctx->f_thread = NULL;
    p_newctx->p_arg = NULL;
    p_newctx->p_link = NULL;
    return abt_errno;
#endif
}

#if ABT_CONFIG_THREAD_TYPE == ABT_THREAD_TYPE_DYNAMIC_PROMOTION
static inline
int ABTD_thread_context_init(ABTD_thread_context *p_link,
                             void (*f_thread)(void *), void *p_arg,
                             ABTD_thread_context *p_newctx)
{
    int abt_errno = ABT_SUCCESS;
#if defined(ABT_CONFIG_USE_FCONTEXT)
    p_newctx->fctx = NULL;
    p_newctx->f_thread = f_thread;
    p_newctx->p_arg = p_arg;
    p_newctx->p_link = p_link;
    return abt_errno;
#else
#error "Not implemented yet"
#endif
}

static inline
int ABTD_thread_context_arm_thread(size_t stacksize, void *p_stack,
                                   ABTD_thread_context *p_newctx)
{
    /* This function *arms* the dynamic promotion thread (initialized by
     * ABTD_thread_context_init) as if it were created by
     * ABTD_thread_context_create; this function fully creates the context
     * so that the thread can be run by jump_fcontext. */
    int abt_errno = ABT_SUCCESS;
#if defined(ABT_CONFIG_USE_FCONTEXT)
    /* fcontext uses the top address of stack.
       Note that the parameter, p_stack, points to the bottom of stack. */
    void *p_stacktop = (void *)(((char *)p_stack) + stacksize);
    p_newctx->fctx = make_fcontext(p_stacktop, stacksize,
                                   ABTD_thread_func_wrapper_thread);
    return abt_errno;
#else
#error "Not implemented yet"
#endif
}
#endif

/* Currently, nothing to do */
#define ABTD_thread_context_free(p_ctx)

static inline
void ABTD_thread_context_switch(ABTD_thread_context *p_old,
                                ABTD_thread_context *p_new)
{
#if defined(ABT_CONFIG_USE_FCONTEXT)
    jump_fcontext(&p_old->fctx, p_new->fctx, p_new);

#else
    int ret = swapcontext(p_old, p_new);
    ABTI_ASSERT(ret == 0);
#endif
}

static inline
void ABTD_thread_finish_context(ABTD_thread_context *p_old,
                                ABTD_thread_context *p_new)
{
#if defined(ABT_CONFIG_USE_FCONTEXT)
    take_fcontext(&p_old->fctx, p_new->fctx, p_new);
#else
    int ret = swapcontext(p_old, p_new);
    ABTI_ASSERT(ret == 0);
#endif
}

#if ABT_CONFIG_THREAD_TYPE == ABT_THREAD_TYPE_DYNAMIC_PROMOTION
static inline
void ABTD_thread_context_make_and_call(ABTD_thread_context *p_old,
                                       void (*f_thread)(void *), void *p_arg,
                                       void *p_stacktop)
{
    init_and_call_fcontext(p_arg, f_thread, p_stacktop, &p_old->fctx);
}

static inline
ABT_bool ABTD_thread_context_is_dynamic_promoted(ABTD_thread_context *p_ctx)
{
    /* Check if the ULT has been dynamically promoted; internally, it checks if
     * the context is NULL. */
    return p_ctx->fctx ? ABT_TRUE : ABT_FALSE;
}

static inline
void ABTDI_thread_context_dynamic_promote(void *p_stacktop, void *jump_f)
{
    /* Perform dynamic promotion */
    void **p_return_address = (void **)(((char *) p_stacktop) - 0x10);
    void ***p_stack_pointer = (void ***)(((char *) p_stacktop) - 0x08);
    *p_stack_pointer = p_return_address;
    *p_return_address = jump_f;
}

static inline
void ABTD_thread_context_dynamic_promote_thread(void *p_stacktop)
{
    void *jump_f = (void *)ABTD_thread_terminate_thread_no_arg;
    ABTDI_thread_context_dynamic_promote(p_stacktop, jump_f);
}
#endif

static inline
void ABTD_thread_context_change_link(ABTD_thread_context *p_ctx,
                                     ABTD_thread_context *p_link)
{
#if defined(ABT_CONFIG_USE_FCONTEXT)
    ABTD_atomic_store_ptr((void **)&p_ctx->p_link, (void *)p_link);

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
}

static inline
void ABTD_thread_context_set_arg(ABTD_thread_context *p_ctx, void *arg)
{
#if defined(ABT_CONFIG_USE_FCONTEXT)
    p_ctx->p_arg = arg;
#else
#error "Not implemented yet"
#endif
}

static inline
void *ABTD_thread_context_get_arg(ABTD_thread_context *p_ctx)
{
#if defined(ABT_CONFIG_USE_FCONTEXT)
    return p_ctx->p_arg;
#else
#error "Not implemented yet"
#endif
}

#endif /* ABTD_THREAD_H_INCLUDED */
