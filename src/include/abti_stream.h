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
#ifndef UNSAFE_MODE
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
#ifndef UNSAFE_MODE
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

/* Get the scheduling context */
static inline
ABTD_thread_context *ABTI_xstream_get_sched_ctx(ABTI_xstream *p_xstream)
{
    ABTI_sched *p_sched = ABTI_xstream_get_top_sched(p_xstream);
    return p_sched->p_ctx;
}

/* Remove the top scheduler from the sched stack (field scheds) */
static inline
int ABTI_xstream_pop_sched(ABTI_xstream *p_xstream)
{
    int abt_errno = ABT_SUCCESS;

    p_xstream->num_scheds--;

    if (p_xstream->num_scheds < 0) {
        abt_errno = ABT_ERR_XSTREAM;
        goto fn_fail;
    }

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABTI_xstream_pop_sched", abt_errno);
    goto fn_exit;
}

/* Add the specified scheduler to the sched stack (field scheds) */
static inline
int ABTI_xstream_push_sched(ABTI_xstream *p_xstream, ABTI_sched *p_sched)
{
    int abt_errno = ABT_SUCCESS;

    ABTI_CHECK_NULL_XSTREAM_PTR(p_xstream);

    if (p_xstream->num_scheds == p_xstream->max_scheds) {
        int max_size = p_xstream->max_scheds+10;
        void *temp;
        temp = ABTU_realloc(p_xstream->scheds, max_size*sizeof(ABTI_sched *));
        p_xstream->scheds = (ABTI_sched **)temp;
        p_xstream->max_scheds = max_size;
    }

    p_xstream->scheds[p_xstream->num_scheds++] = p_sched;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABTI_xstream_push_sched", abt_errno);
    goto fn_exit;
}

static inline
int ABTI_xstream_terminate_thread(ABTI_thread *p_thread)
{
    int abt_errno;
    if (p_thread->refcount == 0) {
        p_thread->state = ABT_THREAD_STATE_TERMINATED;
        abt_errno = ABTI_thread_free(p_thread);
    } else {
        abt_errno = ABTI_xstream_keep_thread(p_thread);
    }
    return abt_errno;
}

static inline
int ABTI_xstream_terminate_task(ABTI_task *p_task)
{
    int abt_errno;
    if (p_task->refcount == 0) {
        p_task->state = ABT_TASK_STATE_TERMINATED;
        abt_errno = ABTI_task_free(p_task);
    } else {
        abt_errno = ABTI_xstream_keep_task(p_task);
    }
    return abt_errno;
}


#endif /* XSTREAM_H_INCLUDED */
