/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"

static inline void ABTD_thread_terminate(ABTI_xstream *p_local_xstream,
                                         ABTI_thread *p_thread);

void ABTD_thread_func_wrapper(void *p_arg)
{
    ABTD_thread_context *p_ctx = (ABTD_thread_context *)p_arg;
    ABTI_thread *p_thread = ABTI_thread_context_get_thread(p_ctx);
    ABTI_xstream *p_local_xstream = p_thread->p_last_xstream;
    ABTI_tool_event_thread_run(p_local_xstream, p_thread,
                               p_local_xstream->p_unit, p_thread->p_parent);
    p_local_xstream->p_unit = p_thread;

    p_thread->f_unit(p_thread->p_arg);

    /* This ABTI_local_get_xstream() is controversial since it is called after
     * the context-switchable function (i.e., thread_func()).  We assume that
     * the compiler does not load TLS offset etc before thread_func(). */
    p_local_xstream = ABTI_local_get_xstream();
    ABTD_thread_terminate(p_local_xstream, p_thread);
}

void ABTD_thread_exit(ABTI_xstream *p_local_xstream, ABTI_thread *p_thread)
{
    ABTD_thread_terminate(p_local_xstream, p_thread);
}

static inline void ABTD_thread_terminate(ABTI_xstream *p_local_xstream,
                                         ABTI_thread *p_thread)
{
    ABTD_thread_context *p_ctx = &p_thread->ctx.ctx;
    ABTD_thread_context *p_link =
        ABTD_atomic_acquire_load_thread_context_ptr(&p_ctx->p_link);
    if (p_link) {
        /* If p_link is set, it means that other ULT has called the join. */
        ABTI_thread *p_joiner = ABTI_thread_context_get_thread(p_link);
        if (p_thread->p_last_xstream == p_joiner->p_last_xstream) {
            /* Only when the current ULT is on the same ES as p_joiner's,
             * we can jump to the joiner ULT. */
            ABTD_atomic_release_store_int(&p_thread->state,
                                          ABTI_UNIT_STATE_TERMINATED);
            LOG_DEBUG("[U%" PRIu64 ":E%d] terminated\n",
                      ABTI_thread_get_id(p_thread),
                      p_thread->p_last_xstream->rank);

            /* Note that a parent ULT cannot be a joiner. */
            ABTI_tool_event_thread_resume(p_local_xstream, p_joiner, p_thread);
            ABTI_thread_finish_context_to_sibling(p_local_xstream, p_thread,
                                                  p_joiner);
            return;
        } else {
            /* If the current ULT's associated ES is different from p_joiner's,
             * we can't directly jump to p_joiner.  Instead, we wake up
             * p_joiner here so that p_joiner's scheduler can resume it. */
            ABTI_thread_set_ready(p_local_xstream, p_joiner);

            /* We don't need to use the atomic OR operation here because the ULT
             * will be terminated regardless of other requests. */
            ABTD_atomic_release_store_uint32(&p_thread->request,
                                             ABTI_UNIT_REQ_TERMINATE);
        }
    } else {
        uint32_t req = ABTD_atomic_fetch_or_uint32(&p_thread->request,
                                                   ABTI_UNIT_REQ_JOIN |
                                                       ABTI_UNIT_REQ_TERMINATE);
        if (req & ABTI_UNIT_REQ_JOIN) {
            /* This case means there has been a join request and the joiner has
             * blocked.  We have to wake up the joiner ULT. */
            do {
                p_link =
                    ABTD_atomic_acquire_load_thread_context_ptr(&p_ctx->p_link);
            } while (!p_link);
            ABTI_thread_set_ready(p_local_xstream,
                                  ABTI_thread_context_get_thread(p_link));
        }
    }

    /* No other ULT is waiting or blocked for this ULT. Since a context does not
     * switch to another context when it finishes, we need to explicitly switch
     * to the parent. */
    ABTI_thread_finish_context_to_parent(p_local_xstream, p_thread);
}

#if ABT_CONFIG_THREAD_TYPE == ABT_THREAD_TYPE_DYNAMIC_PROMOTION
void ABTD_thread_terminate_no_arg()
{
    ABTI_xstream *p_local_xstream = ABTI_local_get_xstream();
    /* This function is called by `return` in ABTD_thread_context_make_and_call,
     * so it cannot take the argument. We get the thread descriptor from TLS. */
    ABTI_thread *p_unit = p_local_xstream->p_unit;
    ABTI_ASSERT(ABTI_unit_type_is_thread(p_unit->type));
    ABTD_thread_terminate(p_local_xstream, ABTI_unit_get_thread(p_unit));
}
#endif

void ABTD_thread_cancel(ABTI_xstream *p_local_xstream, ABTI_thread *p_thread)
{
    /* When we cancel a ULT, if other ULT is blocked to join the canceled ULT,
     * we have to wake up the joiner ULT.  However, unlike the case when the
     * ULT has finished its execution and calls ABTD_thread_terminate/exit,
     * this function is called by the scheduler.  Therefore, we should not
     * context switch to the joiner ULT and need to always wake it up. */
    ABTD_thread_context *p_ctx = &p_thread->ctx.ctx;

    if (ABTD_atomic_acquire_load_thread_context_ptr(&p_ctx->p_link)) {
        /* If p_link is set, it means that other ULT has called the join. */
        ABTI_thread *p_joiner = ABTI_thread_context_get_thread(
            ABTD_atomic_relaxed_load_thread_context_ptr(&p_ctx->p_link));
        ABTI_thread_set_ready(p_local_xstream, p_joiner);
    } else {
        uint32_t req = ABTD_atomic_fetch_or_uint32(&p_thread->request,
                                                   ABTI_UNIT_REQ_JOIN |
                                                       ABTI_UNIT_REQ_TERMINATE);
        if (req & ABTI_UNIT_REQ_JOIN) {
            /* This case means there has been a join request and the joiner has
             * blocked.  We have to wake up the joiner ULT. */
            while (ABTD_atomic_acquire_load_thread_context_ptr(
                       &p_ctx->p_link) == NULL)
                ;
            ABTI_thread *p_joiner = ABTI_thread_context_get_thread(
                ABTD_atomic_relaxed_load_thread_context_ptr(&p_ctx->p_link));
            ABTI_thread_set_ready(p_local_xstream, p_joiner);
        }
    }
    ABTI_tool_event_thread_cancel(p_local_xstream, p_thread);
}

void ABTD_thread_print_context(ABTI_thread *p_thread, FILE *p_os, int indent)
{
    char *prefix = ABTU_get_indent_str(indent);
    ABTD_thread_context *p_ctx = &p_thread->ctx.ctx;
    fprintf(p_os, "%sp_ctx    : %p\n", prefix, p_ctx->p_ctx);
    fprintf(p_os, "%sp_link   : %p\n", prefix,
            (void *)ABTD_atomic_acquire_load_thread_context_ptr(
                &p_ctx->p_link));
    fflush(p_os);
    ABTU_free(prefix);
}
