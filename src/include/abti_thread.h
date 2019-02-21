/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#ifndef THREAD_H_INCLUDED
#define THREAD_H_INCLUDED

/* Inlined functions for User-level Thread (ULT) */

static inline
ABTI_thread *ABTI_thread_get_ptr(ABT_thread thread)
{
#ifndef ABT_CONFIG_DISABLE_ERROR_CHECK
    ABTI_thread *p_thread;
    if (thread == ABT_THREAD_NULL) {
        p_thread = NULL;
    } else {
        p_thread = (ABTI_thread *)thread;
    }
    return p_thread;
#else
    return (ABTI_thread *)thread;
#endif
}

static inline
ABT_thread ABTI_thread_get_handle(ABTI_thread *p_thread)
{
#ifndef ABT_CONFIG_DISABLE_ERROR_CHECK
    ABT_thread h_thread;
    if (p_thread == NULL) {
        h_thread = ABT_THREAD_NULL;
    } else {
        h_thread = (ABT_thread)p_thread;
    }
    return h_thread;
#else
    return (ABT_thread)p_thread;
#endif
}

#if ABT_CONFIG_THREAD_TYPE == ABT_THREAD_TYPE_DYNAMIC_PROMOTION
static inline
ABT_bool ABTI_thread_is_dynamic_promoted(ABTI_thread *p_thread)
{
    /*
     * Create a context and switch to it. The flow of the dynamic promotion
     * thread is as follows:
     *
     * - When a ULT does not yield:
     *  ABTI_xstream_schedule_thread : call init_and_call_fcontext
     *  init_and_call_fcontext       : jump to the stack top
     *                               : save the scheduler's context
     *                               : call the thread function
     *  thread_f                     : start thread_f
     *                               : [ULT body]
     *                               : end thread_f
     *  init_and_call_fcontext       : calculate the return address, which is
     *  [1] =>                         the original return address.
     *                               : `return`
     *  ABTI_xstream_schedule_thread : resume the scheduler
     *
     * The ULT can return to the original scheduler by `return` if the scheduler
     * has never been resumed during the execution of the ULT, because the
     * context of the parent scheduler can be restored in a normal return
     * procedure. In this case, the context is saved only once.
     *
     * - When a ULT yields:
     *  ABTI_xstream_schedule_thread : call init_and_call_fcontext
     *  init_and_call_fcontext       : jump to the stack top
     *                               : save the scheduler's context
     *                               : call the thread function
     *  thread_f                     : start thread_f
     *                               : [yield in ULT body]
     *  ABTD_thread_context_dynamic_promote
     *                               : rewrite the return address to
     *                                 ABTD_thread_terminate_thread_no_arg
     *  jump_fcontext                : save the ULT's context
     *                               : restore the scheduler's context
     *                               : jump to the scheduler
     *  ABTI_xstream_schedule_thread : resume the scheduler
     *
     *            ... After a while, a scheduler resumes this ULT ...
     *
     *  jump_fcontext                : save the scheduler's context
     *                               : restore the ULT's context
     *                               : jump to the ULT
     *  thread_f                     : [ULT body (remaining)]
     *                               : end thread_f
     *  init_and_call_fcontext       : calculate the return address, which is
     *  [2] =>                         ABTD_thread_terminate_thread_no_arg
     *                               : return
     *  ABTD_thread_terminate_thread_no_arg
     *                               : call take_fcontext
     *  take_fcontext                : restore the scheduler's context
     *                               : jump to the scheduler
     *  ABTI_xstream_schedule_thread : resume the scheduler
     *
     * When a ULT yields, ABTD_thread_terminate_thread_no_arg is set to
     * [ptr - 0x08] so that it can "jump" to the normal termination
     * function by "return" in init_and_call_fcontext. This termination
     * function calls take_fcontext, so the scheduler is resumed by user-level
     * context switch.
     *
     * For example, the stack will be as follows at [1] and [2] in the x86-64
     * case. Note that ptr points to the stack top (= p_stack + stacksize).
     *
     * In the case of [1] (no suspension):
     *  [0x12345600] : (the original instruction pointer)
     *   ...
     *  [ptr - 0x08] : the original stack pointer (i.e., 0x12345600)
     *  [ptr - 0x10] : unused (for 16-byte alignment)
     *  [ptr - xxxx] : used by thread_f
     *
     * In the case of [2] (after suspension):
     *  [ptr - 0x08] : pointing to (p_stack - 0x10)
     *  [ptr - 0x10] : the address of ABTD_thread_terminate_thread_no_arg
     *  [ptr - xxxx] : used by thread_f
     *
     * This technique was introduced as a "return-on-completion" thread in the
     * following paper:
     *   Lessons Learned from Analyzing Dynamic Promotion for User-Level
     *   Threading, S. Iwasaki, A. Amer, K. Taura, and P. Balaji (SC '18)
     */
    return ABTD_thread_context_is_dynamic_promoted(&p_thread->ctx);
}

static inline
void ABTI_thread_dynamic_promote_thread(ABTI_thread *p_thread)
{
    LOG_EVENT("[U%" PRIu64 "] dynamic-promote ULT\n",
              ABTI_thread_get_id(p_thread));
    void *p_stack = p_thread->attr.p_stack;
    size_t stacksize = p_thread->attr.stacksize;
    void *p_stacktop = (void *)(((char *)p_stack) + stacksize);
    ABTD_thread_context_dynamic_promote_thread(p_stacktop);
}
#endif

static inline
void ABTI_thread_context_switch_thread_to_thread_internal(ABTI_thread *p_old,
                                                          ABTI_thread *p_new,
                                                          ABT_bool is_finish)
{
#ifndef ABT_CONFIG_DISABLE_STACKABLE_SCHED
    ABTI_ASSERT(!p_old->is_sched && !p_new->is_sched);
#endif
    ABTI_local_set_thread(p_new);
#if ABT_CONFIG_THREAD_TYPE == ABT_THREAD_TYPE_DYNAMIC_PROMOTION
    /* Dynamic promotion is unnecessary if p_old is discarded. */
    if (!is_finish && !ABTI_thread_is_dynamic_promoted(p_old)) {
        ABTI_thread_dynamic_promote_thread(p_old);
    }
    if (!ABTI_thread_is_dynamic_promoted(p_new)) {
        /* p_new does not have a context, so we first need to make it. */
        ABTD_thread_context_arm_thread(p_new->attr.stacksize,
                                       p_new->attr.p_stack, &p_new->ctx);
    }
#endif
    if (is_finish) {
        ABTD_thread_finish_context(&p_old->ctx, &p_new->ctx);
    } else {
        ABTD_thread_context_switch(&p_old->ctx, &p_new->ctx);
    }
}

static inline
void ABTI_thread_context_switch_thread_to_sched_internal(ABTI_thread *p_old,
                                                         ABTI_sched *p_new,
                                                         ABT_bool is_finish)
{
#ifndef ABT_CONFIG_DISABLE_STACKABLE_SCHED
    ABTI_ASSERT(!p_old->is_sched);
#endif
    ABTI_LOG_SET_SCHED(p_new);
#if ABT_CONFIG_THREAD_TYPE == ABT_THREAD_TYPE_DYNAMIC_PROMOTION
    /* Dynamic promotion is unnecessary if p_old is discarded. */
    if (!is_finish && !ABTI_thread_is_dynamic_promoted(p_old))
        ABTI_thread_dynamic_promote_thread(p_old);
    /* Schedulers' contexts must be eagerly initialized. */
    ABTI_ASSERT(!p_new->p_thread
                || ABTI_thread_is_dynamic_promoted(p_new->p_thread));
#endif
    if (is_finish) {
        ABTD_thread_finish_context(&p_old->ctx, p_new->p_ctx);
    } else {
        ABTD_thread_context_switch(&p_old->ctx, p_new->p_ctx);
    }
}

static inline
void ABTI_thread_context_switch_sched_to_thread_internal(ABTI_sched *p_old,
                                                         ABTI_thread *p_new,
                                                         ABT_bool is_finish)
{
#ifndef ABT_CONFIG_DISABLE_STACKABLE_SCHED
    ABTI_ASSERT(!p_new->is_sched);
#endif
    ABTI_LOG_SET_SCHED(NULL);
    ABTI_local_set_thread(p_new);
    ABTI_local_set_task(NULL); /* A tasklet scheduler can invoke ULT. */
#if ABT_CONFIG_THREAD_TYPE == ABT_THREAD_TYPE_DYNAMIC_PROMOTION
    /* Schedulers' contexts must be eagerly initialized. */
    ABTI_ASSERT(!p_old->p_thread
                || ABTI_thread_is_dynamic_promoted(p_old->p_thread));
    if (!ABTI_thread_is_dynamic_promoted(p_new)) {
        void *p_stacktop = ((char *)p_new->attr.p_stack) +
                            p_new->attr.stacksize;
        LOG_EVENT("[U%" PRIu64 "] run ULT (dynamic promotion)\n",
                  ABTI_thread_get_id(p_new));
        ABTD_thread_context_make_and_call(p_old->p_ctx, p_new->ctx.f_thread,
                                          p_new->ctx.p_arg, p_stacktop);
        /* The scheduler continues from here. If the previous thread has not
         * run dynamic promotion, ABTI_thread_context_make_and_call took the
         * fast path. In this case, the request handling has not been done,
         * so it must be done here. */
        ABTI_thread *p_prev = ABTI_local_get_thread();
        if (!ABTI_thread_is_dynamic_promoted(p_prev)) {
            ABTI_ASSERT(p_prev == p_new);
#if defined(ABT_CONFIG_USE_FCONTEXT)
            /* See ABTDI_thread_terminate for details.
             * TODO: avoid making a copy of the code. */
            ABTD_thread_context *p_fctx = &p_prev->ctx;
            ABTD_thread_context *p_link = (ABTD_thread_context *)
                ABTD_atomic_load_ptr((void **)&p_fctx->p_link);
            if (p_link) {
                /* If p_link is set, it means that other ULT has called the
                 * join. */
                ABTI_thread *p_joiner = (ABTI_thread *)p_link;
                /* The scheduler may not use a bypass mechanism, so just makes
                 * p_joiner ready. */
                ABTI_thread_set_ready(p_joiner);

                /* We don't need to use the atomic OR operation here because
                 * the ULT will be terminated regardless of other requests. */
                ABTD_atomic_store_uint32(&p_prev->request,
                                         ABTI_THREAD_REQ_TERMINATE);
            } else {
                uint32_t req = ABTD_atomic_fetch_or_uint32(&p_prev->request,
                        ABTI_THREAD_REQ_JOIN | ABTI_THREAD_REQ_TERMINATE);
                if (req & ABTI_THREAD_REQ_JOIN) {
                    /* This case means there has been a join request and the
                     * joiner has blocked.  We have to wake up the joiner ULT.
                     */
                    do {
                        p_link = (ABTD_thread_context *)
                            ABTD_atomic_load_ptr((void **)&p_fctx->p_link);
                    } while (!p_link);
                    ABTI_thread_set_ready((ABTI_thread *)p_link);
                }
            }
#else
#error "Not implemented yet"
#endif
        }
        return;
    }
#endif
    if (is_finish) {
        ABTD_thread_finish_context(p_old->p_ctx, &p_new->ctx);
    } else {
        ABTD_thread_context_switch(p_old->p_ctx, &p_new->ctx);
    }
}

static inline
void ABTI_thread_context_switch_sched_to_sched_internal(ABTI_sched *p_old,
                                                        ABTI_sched *p_new,
                                                        ABT_bool is_finish)
{
    ABTI_LOG_SET_SCHED(p_new);
#if ABT_CONFIG_THREAD_TYPE == ABT_THREAD_TYPE_DYNAMIC_PROMOTION
    /* Schedulers' contexts must be initialized eagerly. */
    ABTI_ASSERT(!p_old->p_thread
                || ABTI_thread_is_dynamic_promoted(p_old->p_thread));
    ABTI_ASSERT(!p_new->p_thread
                || ABTI_thread_is_dynamic_promoted(p_new->p_thread));
#endif
    if (is_finish) {
        ABTD_thread_finish_context(p_old->p_ctx, p_new->p_ctx);
    } else {
        ABTD_thread_context_switch(p_old->p_ctx, p_new->p_ctx);
    }
}

static inline
void ABTI_thread_context_switch_thread_to_thread(ABTI_thread *p_old,
                                                 ABTI_thread *p_new)
{
    ABTI_thread_context_switch_thread_to_thread_internal(p_old, p_new,
                                                         ABT_FALSE);
}

static inline
void ABTI_thread_context_switch_thread_to_sched(ABTI_thread *p_old,
                                                ABTI_sched *p_new)
{
    ABTI_thread_context_switch_thread_to_sched_internal(p_old, p_new,
                                                        ABT_FALSE);
}

static inline
void ABTI_thread_context_switch_sched_to_thread(ABTI_sched *p_old,
                                                ABTI_thread *p_new)
{
    ABTI_thread_context_switch_sched_to_thread_internal(p_old, p_new,
                                                        ABT_FALSE);
}

static inline
void ABTI_thread_context_switch_sched_to_sched(ABTI_sched *p_old,
                                               ABTI_sched *p_new)
{
    ABTI_thread_context_switch_sched_to_sched_internal(p_old, p_new, ABT_FALSE);
}

static inline
void ABTI_thread_finish_context_thread_to_thread(ABTI_thread *p_old,
                                                 ABTI_thread *p_new)
{
    ABTI_thread_context_switch_thread_to_thread_internal(p_old, p_new,
                                                         ABT_TRUE);
}

static inline
void ABTI_thread_finish_context_thread_to_sched(ABTI_thread *p_old,
                                                ABTI_sched *p_new)
{
    ABTI_thread_context_switch_thread_to_sched_internal(p_old, p_new, ABT_TRUE);
}

static inline
void ABTI_thread_finish_context_sched_to_thread(ABTI_sched *p_old,
                                                ABTI_thread *p_new)
{
    ABTI_thread_context_switch_sched_to_thread_internal(p_old, p_new, ABT_TRUE);
}

static inline
void ABTI_thread_finish_context_sched_to_sched(ABTI_sched *p_old,
                                               ABTI_sched *p_new)
{
    ABTI_thread_context_switch_sched_to_sched_internal(p_old, p_new, ABT_TRUE);
}

static inline
void ABTI_thread_set_request(ABTI_thread *p_thread, uint32_t req)
{
    ABTD_atomic_fetch_or_uint32(&p_thread->request, req);
}

static inline
void ABTI_thread_unset_request(ABTI_thread *p_thread, uint32_t req)
{
    ABTD_atomic_fetch_and_uint32(&p_thread->request, ~req);
}

#ifdef ABT_CONFIG_DISABLE_MIGRATION
static inline
void  ABTI_thread_put_req_arg(ABTI_thread *p_thread,
                              ABTI_thread_req_arg *p_req_arg)
{
    ABTI_ASSERT(p_thread->p_req_arg == NULL);
    p_thread->p_req_arg = p_req_arg;
}

static inline
ABTI_thread_req_arg *ABTI_thread_get_req_arg(ABTI_thread *p_thread,
                                             uint32_t req)
{
    ABTI_thread_req_arg *p_result = p_thread->p_req_arg;
    p_thread->p_req_arg = NULL;
    return p_result;
}
#endif /* ABT_CONFIG_DISABLE_MIGRATION */

static inline
void ABTI_thread_yield(ABTI_thread *p_thread)
{
    ABTI_sched *p_sched;

    LOG_EVENT("[U%" PRIu64 ":E%d] yield\n",
              ABTI_thread_get_id(p_thread), p_thread->p_last_xstream->rank);

    /* Change the state of current running thread */
    p_thread->state = ABT_THREAD_STATE_READY;

    /* Switch to the top scheduler */
    p_sched = ABTI_xstream_get_top_sched(p_thread->p_last_xstream);
    ABTI_thread_context_switch_thread_to_sched(p_thread, p_sched);

    /* Back to the original thread */
    LOG_EVENT("[U%" PRIu64 ":E%d] resume after yield\n",
              ABTI_thread_get_id(p_thread), p_thread->p_last_xstream->rank);
}

#endif /* THREAD_H_INCLUDED */

