/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#ifndef XSTREAM_H_INCLUDED
#define XSTREAM_H_INCLUDED

/* Inlined functions for Execution Stream (ES) */

static inline
ABTI_xstream *ABTI_xstream_get_ptr(ABT_xstream xstream)
{
#ifndef ABT_CONFIG_DISABLE_ERROR_CHECK
    ABTI_xstream *p_xstream;
    if (xstream == ABT_XSTREAM_NULL) {
        p_xstream = NULL;
    } else {
        p_xstream = (ABTI_xstream *)xstream;
    }
    return p_xstream;
#else
    return (ABTI_xstream *)xstream;
#endif
}

static inline
ABT_xstream ABTI_xstream_get_handle(ABTI_xstream *p_xstream)
{
#ifndef ABT_CONFIG_DISABLE_ERROR_CHECK
    ABT_xstream h_xstream;
    if (p_xstream == NULL) {
        h_xstream = ABT_XSTREAM_NULL;
    } else {
        h_xstream = (ABT_xstream)p_xstream;
    }
    return h_xstream;
#else
    return (ABT_xstream)p_xstream;
#endif
}

static inline
void ABTI_xstream_set_request(ABTI_xstream *p_xstream, uint32_t req)
{
    ABTD_atomic_fetch_or_uint32(&p_xstream->request, req);
}

static inline
void ABTI_xstream_unset_request(ABTI_xstream *p_xstream, uint32_t req)
{
    ABTD_atomic_fetch_and_uint32(&p_xstream->request, ~req);
}

static inline
ABTI_xstream *ABTI_xstream_self(void)
{
    ABTI_xstream *p_xstream;
    if (lp_ABTI_local != NULL) {
        p_xstream = ABTI_local_get_xstream();
    } else {
        /* We allow external threads to call Argobots APIs. However, since it
         * is not trivial to identify them, we use ABTD_xstream_context to
         * distinguish them for now. */
        ABTD_xstream_context tmp_ctx;
        ABTD_xstream_context_self(&tmp_ctx);
        p_xstream = (ABTI_xstream *)tmp_ctx;
    }
    return p_xstream;
}

/* Get the top scheduler from the sched stack (field scheds) */
static inline
ABTI_sched *ABTI_xstream_get_top_sched(ABTI_xstream *p_xstream)
{
    return p_xstream->scheds[p_xstream->num_scheds-1];
}

/* Get the parent scheduler of the current scheduler */
static inline
ABTI_sched *ABTI_xstream_get_parent_sched(ABTI_xstream *p_xstream)
{
    ABTI_ASSERT(p_xstream->num_scheds >= 2);
    return p_xstream->scheds[p_xstream->num_scheds-2];
}

/* Get the scheduling context */
static inline
ABTD_thread_context *ABTI_xstream_get_sched_ctx(ABTI_xstream *p_xstream)
{
    ABTI_sched *p_sched = ABTI_xstream_get_top_sched(p_xstream);
    return p_sched->p_ctx;
}

/* Remove the top scheduler from the sched stack (field scheds) */
static inline
void ABTI_xstream_pop_sched(ABTI_xstream *p_xstream)
{
    p_xstream->num_scheds--;
    ABTI_ASSERT(p_xstream->num_scheds >= 0);
}

/* Replace the top scheduler of the sched stack (field scheds) with the target
 * scheduler */
static inline
void ABTI_xstream_replace_top_sched(ABTI_xstream *p_xstream, ABTI_sched *p_sched)
{
    p_xstream->scheds[p_xstream->num_scheds-1] = p_sched;
}

/* Add the specified scheduler to the sched stack (field scheds) */
static inline
void ABTI_xstream_push_sched(ABTI_xstream *p_xstream, ABTI_sched *p_sched)
{
    if (p_xstream->num_scheds == p_xstream->max_scheds) {
        int max_size = p_xstream->max_scheds+10;
        void *temp;
        temp = ABTU_realloc(p_xstream->scheds, max_size*sizeof(ABTI_sched *));
        p_xstream->scheds = (ABTI_sched **)temp;
        p_xstream->max_scheds = max_size;
    }

    p_xstream->scheds[p_xstream->num_scheds++] = p_sched;
}

static inline
void ABTI_xstream_terminate_thread(ABTI_thread *p_thread)
{
    LOG_EVENT("[U%" PRIu64 ":E%" PRIu64 "] terminated\n",
              ABTI_thread_get_id(p_thread), p_thread->p_last_xstream->rank);
    ABTI_EVENT_INC_UNIT_CNT(p_thread->p_last_xstream, ABT_UNIT_TYPE_THREAD);
    if (p_thread->refcount == 0) {
        p_thread->state = ABT_THREAD_STATE_TERMINATED;
        ABTI_thread_free(p_thread);
#ifndef ABT_CONFIG_DISABLE_STACKABLE_SCHED
    } else if (p_thread->is_sched) {
        /* NOTE: p_thread itself will be freed in ABTI_sched_free. */
        p_thread->state = ABT_THREAD_STATE_TERMINATED;
        ABTI_sched_discard_and_free(p_thread->is_sched);
#endif
    } else {
        /* NOTE: We set the ULT's state as TERMINATED after checking refcount
         * because the ULT can be freed on a different ES.  In other words, we
         * must not access any field of p_thead after changing the state to
         * TERMINATED. */
        p_thread->state = ABT_THREAD_STATE_TERMINATED;
    }
}

static inline
void ABTI_xstream_terminate_task(ABTI_task *p_task)
{
    LOG_EVENT("[T%" PRIu64 ":E%" PRIu64 "] terminated\n",
              ABTI_task_get_id(p_task), p_task->p_xstream->rank);
    ABTI_EVENT_INC_UNIT_CNT(p_task->p_xstream, ABT_UNIT_TYPE_TASK);
    if (p_task->refcount == 0) {
        p_task->state = ABT_TASK_STATE_TERMINATED;
        ABTI_task_free(p_task);
#ifndef ABT_CONFIG_DISABLE_STACKABLE_SCHED
    } else if (p_task->is_sched) {
        /* NOTE: p_task itself will be freed in ABTI_sched_free. */
        p_task->state = ABT_TASK_STATE_TERMINATED;
        ABTI_sched_discard_and_free(p_task->is_sched);
#endif
    } else {
        /* NOTE: We set the task's state as TERMINATED after checking refcount
         * because the task can be freed on a different ES.  In other words, we
         * must not access any field of p_task after changing the state to
         * TERMINATED. */
        p_task->state = ABT_TASK_STATE_TERMINATED;
    }
}

#endif /* XSTREAM_H_INCLUDED */
