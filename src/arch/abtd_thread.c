/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"

static inline void ABTD_thread_terminate_thread(ABTI_local *p_local,
                                                ABTI_thread *p_thread);
static inline void ABTD_thread_terminate_sched(ABTI_local *p_local,
                                               ABTI_thread *p_thread);

void ABTD_thread_func_wrapper_thread(void *p_arg)
{
    ABTD_thread_context *p_ctx = (ABTD_thread_context *)p_arg;
    void (*thread_func)(void *) = p_ctx->f_thread;

    thread_func(p_ctx->p_arg);

    /* NOTE: ctx is located in the beginning of ABTI_thread */
    ABTI_thread *p_thread = (ABTI_thread *)p_ctx;
#ifndef ABT_CONFIG_DISABLE_STACKABLE_SCHED
    ABTI_ASSERT(p_thread->is_sched == NULL);
#endif

    ABTI_local *p_local = ABTI_local_get_local();
    ABTD_thread_terminate_thread(p_local, p_thread);
}

void ABTD_thread_func_wrapper_sched(void *p_arg)
{
    ABTD_thread_context *p_ctx = (ABTD_thread_context *)p_arg;
    void (*thread_func)(void *) = p_ctx->f_thread;

    thread_func(p_ctx->p_arg);

    /* NOTE: ctx is located in the beginning of ABTI_thread */
    ABTI_thread *p_thread = (ABTI_thread *)p_ctx;
#ifndef ABT_CONFIG_DISABLE_STACKABLE_SCHED
    ABTI_ASSERT(p_thread->is_sched != NULL);
#endif

    ABTI_local *p_local = ABTI_local_get_local();
    ABTD_thread_terminate_sched(p_local, p_thread);
}

void ABTD_thread_exit(ABTI_local *p_local, ABTI_thread *p_thread)
{
#ifndef ABT_CONFIG_DISABLE_STACKABLE_SCHED
    if (p_thread->is_sched) {
        ABTD_thread_terminate_sched(p_local, p_thread);
    } else {
#endif
        ABTD_thread_terminate_thread(p_local, p_thread);
#ifndef ABT_CONFIG_DISABLE_STACKABLE_SCHED
    }
#endif
}

static inline void ABTDI_thread_terminate(ABTI_local *p_local,
                                          ABTI_thread *p_thread,
                                          ABT_bool is_sched)
{
    ABTD_thread_context *p_ctx = &p_thread->ctx;
    ABTD_thread_context *p_link =
        (ABTD_thread_context *)ABTD_atomic_acquire_load_ptr((void **)&p_ctx->p_link);
    if (p_link) {
        /* If p_link is set, it means that other ULT has called the join. */
        ABTI_thread *p_joiner = (ABTI_thread *)p_link;
        if (p_thread->p_last_xstream == p_joiner->p_last_xstream) {
            /* Only when the current ULT is on the same ES as p_joiner's,
             * we can jump to the joiner ULT. */
            ABTD_atomic_release_store_int((int *)&p_thread->state,
                                     ABT_THREAD_STATE_TERMINATED);
            LOG_EVENT("[U%" PRIu64 ":E%d] terminated\n",
                      ABTI_thread_get_id(p_thread),
                      p_thread->p_last_xstream->rank);

            /* Note that a scheduler-type ULT cannot be a joiner. If a scheduler
             * type ULT would be a joiner (=suspend), no scheduler is available
             * when a running ULT needs suspension. Hence, it always jumps to a
             * non-scheduler-type ULT. */
#ifndef ABT_CONFIG_DISABLE_STACKABLE_SCHED
            if (is_sched) {
                ABTI_thread_finish_context_sched_to_thread(p_local,
                                                           p_thread->is_sched,
                                                           p_joiner);
            } else {
#endif
                ABTI_thread_finish_context_thread_to_thread(p_local, p_thread,
                                                            p_joiner);
#ifndef ABT_CONFIG_DISABLE_STACKABLE_SCHED
            }
#endif
            return;
        } else {
            /* If the current ULT's associated ES is different from p_joiner's,
             * we can't directly jump to p_joiner.  Instead, we wake up
             * p_joiner here so that p_joiner's scheduler can resume it. */
            ABTI_thread_set_ready(p_local, p_joiner);

            /* We don't need to use the atomic OR operation here because the ULT
             * will be terminated regardless of other requests. */
            ABTD_atomic_release_store_uint32(&p_thread->request,
                                     ABTI_THREAD_REQ_TERMINATE);
        }
    } else {
        uint32_t req =
            ABTD_atomic_fetch_or_uint32(&p_thread->request,
                                        ABTI_THREAD_REQ_JOIN |
                                            ABTI_THREAD_REQ_TERMINATE);
        if (req & ABTI_THREAD_REQ_JOIN) {
            /* This case means there has been a join request and the joiner has
             * blocked.  We have to wake up the joiner ULT. */
            do {
                p_link = (ABTD_thread_context *)ABTD_atomic_acquire_load_ptr(
                    (void **)&p_ctx->p_link);
            } while (!p_link);
            ABTI_thread_set_ready(p_local, (ABTI_thread *)p_link);
        }
    }

    /* No other ULT is waiting or blocked for this ULT. Since a context does not
     * switch to another context when it finishes, we need to explicitly switch
     * to the scheduler. */
    ABTI_sched *p_sched;
#ifndef ABT_CONFIG_DISABLE_STACKABLE_SCHED
    if (p_thread->is_sched) {
        /* If p_thread is a scheduler ULT, we have to context switch to
         * the parent scheduler. */
        p_sched = ABTI_xstream_get_parent_sched(p_thread->p_last_xstream);
    } else {
#endif
        p_sched = ABTI_xstream_get_top_sched(p_thread->p_last_xstream);
#ifndef ABT_CONFIG_DISABLE_STACKABLE_SCHED
    }
#endif
#ifndef ABT_CONFIG_DISABLE_STACKABLE_SCHED
    if (is_sched) {
        ABTI_thread_finish_context_sched_to_sched(p_thread->is_sched, p_sched);
    } else {
#endif
        ABTI_thread_finish_context_thread_to_sched(p_thread, p_sched);
#ifndef ABT_CONFIG_DISABLE_STACKABLE_SCHED
    }
#endif
}

static inline void ABTD_thread_terminate_thread(ABTI_local *p_local,
                                                ABTI_thread *p_thread)
{
    ABTDI_thread_terminate(p_local, p_thread, ABT_FALSE);
}

static inline void ABTD_thread_terminate_sched(ABTI_local *p_local,
                                               ABTI_thread *p_thread)
{
    ABTDI_thread_terminate(p_local, p_thread, ABT_TRUE);
}

#if ABT_CONFIG_THREAD_TYPE == ABT_THREAD_TYPE_DYNAMIC_PROMOTION
void ABTD_thread_terminate_thread_no_arg()
{
    ABTI_local *p_local = ABTI_local_get_local();
    /* This function is called by `return` in ABTD_thread_context_make_and_call,
     * so it cannot take the argument. We get the thread descriptor from TLS. */
    ABTI_thread *p_thread = p_local->p_thread;
    ABTD_thread_terminate_thread(p_local, p_thread);
}
#endif

void ABTD_thread_cancel(ABTI_local *p_local, ABTI_thread *p_thread)
{
    /* When we cancel a ULT, if other ULT is blocked to join the canceled ULT,
     * we have to wake up the joiner ULT.  However, unlike the case when the
     * ULT has finished its execution and calls ABTD_thread_terminate/exit,
     * this function is called by the scheduler.  Therefore, we should not
     * context switch to the joiner ULT and need to always wake it up. */
    ABTD_thread_context *p_ctx = &p_thread->ctx;

    /* acquire load is not needed here. */
    if (p_ctx->p_link) {
        /* If p_link is set, it means that other ULT has called the join. */
        ABTI_thread *p_joiner = (ABTI_thread *)p_ctx->p_link;
        ABTI_thread_set_ready(p_local, p_joiner);
    } else {
        uint32_t req =
            ABTD_atomic_fetch_or_uint32(&p_thread->request,
                                        ABTI_THREAD_REQ_JOIN |
                                            ABTI_THREAD_REQ_TERMINATE);
        if (req & ABTI_THREAD_REQ_JOIN) {
            /* This case means there has been a join request and the joiner has
             * blocked.  We have to wake up the joiner ULT. */
            while (ABTD_atomic_acquire_load_ptr((void **)&p_ctx->p_link) == NULL)
                ;
            ABTI_thread *p_joiner = (ABTI_thread *)p_ctx->p_link;
            ABTI_thread_set_ready(p_local, p_joiner);
        }
    }
}

void ABTD_thread_print_context(ABTI_thread *p_thread, FILE *p_os, int indent)
{
    char *prefix = ABTU_get_indent_str(indent);
    ABTD_thread_context *p_ctx = &p_thread->ctx;
    fprintf(p_os, "%sp_ctx    : %p\n", prefix, p_ctx->p_ctx);
    fprintf(p_os, "%sp_arg    : %p\n", prefix, p_ctx->p_arg);
    fprintf(p_os, "%sp_link   : %p\n", prefix, (void *)p_ctx->p_link);
    fflush(p_os);
    ABTU_free(prefix);
}
