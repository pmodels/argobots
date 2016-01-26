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

static inline
void ABTI_thread_yield(ABTI_thread *p_thread)
{
    ABTI_sched *p_sched;

    LOG_EVENT("[U%" PRIu64 ":E%" PRIu64 "] yield\n",
              ABTI_thread_get_id(p_thread), p_thread->p_last_xstream->rank);

    /* Change the state of current running thread */
    p_thread->state = ABT_THREAD_STATE_READY;

    /* Switch to the top scheduler */
    p_sched = ABTI_xstream_get_top_sched(p_thread->p_last_xstream);
    ABTI_LOG_SET_SCHED(p_sched);
    ABTD_thread_context_switch(&p_thread->ctx, p_sched->p_ctx);

    /* Back to the original thread */
    LOG_EVENT("[U%" PRIu64 ":E%" PRIu64 "] resume after yield\n",
              ABTI_thread_get_id(p_thread), p_thread->p_last_xstream->rank);
}

#endif /* THREAD_H_INCLUDED */

