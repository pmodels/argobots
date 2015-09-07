/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"

#if defined(ABT_CONFIG_USE_FCONTEXT)
void ABTD_thread_func_wrapper(void *p_arg)
{
    ABTD_thread_context *p_fctx = (ABTD_thread_context *)p_arg;
    void (*thread_func)(void *) = p_fctx->f_thread;

    thread_func(p_fctx->p_arg);

    /* NOTE: ctx is located in the beginning of ABTI_thread */
    ABTI_thread *p_thread = (ABTI_thread *)p_fctx;

    /* Now, the ULT has finished its job. Terminate the ULT. */
    if (p_fctx->p_link) {
        /* If p_link is set, it means that other ULT has called the join. */
        ABTI_thread *p_joiner = (ABTI_thread *)p_fctx->p_link;
        if (p_thread->p_last_xstream == p_joiner->p_last_xstream) {
            /* Only when the current ULT is on the same ES as p_joiner's,
             * we can jump to the joiner ULT. */
            p_thread->state = ABT_THREAD_STATE_TERMINATED;
            LOG_EVENT("[U%" PRIu64 ":E%" PRIu64 "] terminated\n",
                      ABTI_thread_get_id(p_thread),
                      p_thread->p_last_xstream->rank);

            ABTD_thread_finish_context(p_fctx, p_fctx->p_link);
            return;
        } else {
            /* If the current ULT's associated ES is different from p_joiner's,
             * we can't directly jump to p_joiner.  Instead, we wake up
             * p_joiner here so that p_joiner's scheduler can resume it. */
            ABTI_thread_set_ready(p_joiner);
        }
    }

    /* No other ULT is waiting or blocked for this ULT. Since fcontext does
     * not switch to other fcontext when it finishes, we need to explicitly
     * switch to the scheduler. */
    ABTD_thread_context *p_sched_ctx;
    p_sched_ctx = ABTI_xstream_get_sched_ctx(p_thread->p_last_xstream);
    ABTI_LOG_SET_SCHED((p_sched_ctx == p_fctx->p_link)
                       ? ABTI_xstream_get_top_sched(p_thread->p_last_xstream)
                       : NULL);

    /* We don't need to use the atomic operation here because the ULT will be
     * terminated regardless of other requests. */
    p_thread->request |= ABTI_THREAD_REQ_TERMINATE;
    ABTD_thread_finish_context(p_fctx, p_sched_ctx);
}
#else
void ABTD_thread_func_wrapper(int func_upper, int func_lower,
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

void ABTD_thread_exit(ABTI_thread *p_thread)
{
#if defined(ABT_CONFIG_USE_FCONTEXT)
    ABTD_thread_context *p_fctx = &p_thread->ctx;
    if (p_fctx->p_link) {
        p_thread->state = ABT_THREAD_STATE_TERMINATED;
        LOG_EVENT("[U%" PRIu64 ":E%" PRIu64 "] terminated\n",
                  ABTI_thread_get_id(p_thread), p_thread->p_last_xstream->rank);

        take_fcontext(&p_fctx->fctx, p_fctx->p_link->fctx, NULL,
                      ABTD_FCONTEXT_PRESERVE_FPU);
    } else {
        ABT_thread_yield();
    }
#else
#error "Not implemented yet"
#endif
}

