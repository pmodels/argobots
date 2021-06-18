/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#ifndef ABTI_YTHREAD_H_INCLUDED
#define ABTI_YTHREAD_H_INCLUDED

/* Inlined functions for yieldable threads */

static inline ABTI_ythread *ABTI_ythread_get_ptr(ABT_thread thread)
{
#ifndef ABT_CONFIG_DISABLE_ERROR_CHECK
    ABTI_ythread *p_ythread;
    if (thread == ABT_THREAD_NULL) {
        p_ythread = NULL;
    } else {
        p_ythread = (ABTI_ythread *)thread;
    }
    return p_ythread;
#else
    return (ABTI_ythread *)thread;
#endif
}

static inline ABT_thread ABTI_ythread_get_handle(ABTI_ythread *p_ythread)
{
#ifndef ABT_CONFIG_DISABLE_ERROR_CHECK
    ABT_thread h_thread;
    if (p_ythread == NULL) {
        h_thread = ABT_THREAD_NULL;
    } else {
        h_thread = (ABT_thread)p_ythread;
    }
    return h_thread;
#else
    return (ABT_thread)p_ythread;
#endif
}

static inline ABTI_ythread *
ABTI_ythread_context_get_ythread(ABTD_ythread_context *p_ctx)
{
    return (ABTI_ythread *)(((char *)p_ctx) - offsetof(ABTI_ythread, ctx));
}

static inline ABTI_ythread *ABTI_ythread_context_switch_to_sibling_internal(
    ABTI_xstream **pp_local_xstream, ABTI_ythread *p_old, ABTI_ythread *p_new,
    ABT_bool is_finish)
{
    p_new->thread.p_parent = p_old->thread.p_parent;
    if (is_finish) {
        ABTI_tool_event_thread_finish(*pp_local_xstream, &p_old->thread,
                                      p_old->thread.p_parent);
        ABTD_ythread_finish_context(&p_old->ctx, &p_new->ctx);
        ABTU_unreachable();
    } else {
        ABTD_ythread_context_switch(&p_old->ctx, &p_new->ctx);
        ABTI_local *p_local = ABTI_local_get_local_uninlined();
        ABTI_xstream *p_local_xstream = ABTI_local_get_xstream(p_local);
        *pp_local_xstream = p_local_xstream;
        ABTI_thread *p_prev = p_local_xstream->p_thread;
        p_local_xstream->p_thread = &p_old->thread;
        ABTI_ASSERT(p_prev->type & ABTI_THREAD_TYPE_YIELDABLE);
        return ABTI_thread_get_ythread(p_prev);
    }
}

static inline ABTI_ythread *ABTI_ythread_context_switch_to_parent_internal(
    ABTI_xstream **pp_local_xstream, ABTI_ythread *p_old, ABT_bool is_finish,
    ABT_sync_event_type sync_event_type, void *p_sync)
{
    ABTI_ythread *p_new = ABTI_thread_get_ythread(p_old->thread.p_parent);
    if (is_finish) {
        ABTI_tool_event_thread_finish(*pp_local_xstream, &p_old->thread,
                                      p_old->thread.p_parent);
        ABTD_ythread_finish_context(&p_old->ctx, &p_new->ctx);
        ABTU_unreachable();
    } else {
        ABTI_tool_event_ythread_yield(*pp_local_xstream, p_old,
                                      p_old->thread.p_parent, sync_event_type,
                                      p_sync);
        ABTD_ythread_context_switch(&p_old->ctx, &p_new->ctx);
        ABTI_local *p_local = ABTI_local_get_local_uninlined();
        ABTI_xstream *p_local_xstream = ABTI_local_get_xstream(p_local);
        *pp_local_xstream = p_local_xstream;
        ABTI_thread *p_prev = p_local_xstream->p_thread;
        p_local_xstream->p_thread = &p_old->thread;
        ABTI_ASSERT(p_prev->type & ABTI_THREAD_TYPE_YIELDABLE);
        /* Invoke an event of thread run. */
        ABTI_tool_event_thread_run(p_local_xstream, &p_old->thread, p_prev,
                                   p_old->thread.p_parent);
        return ABTI_thread_get_ythread(p_prev);
    }
}

static inline ABTI_ythread *ABTI_ythread_context_switch_to_child_internal(
    ABTI_xstream **pp_local_xstream, ABTI_ythread *p_old, ABTI_ythread *p_new)
{
    ABTI_xstream *p_local_xstream;
    p_new->thread.p_parent = &p_old->thread;
    ABTD_ythread_context_switch(&p_old->ctx, &p_new->ctx);
    ABTI_local *p_local = ABTI_local_get_local_uninlined();
    p_local_xstream = ABTI_local_get_xstream(p_local);
    *pp_local_xstream = p_local_xstream;
    ABTI_thread *p_prev = p_local_xstream->p_thread;
    p_local_xstream->p_thread = &p_old->thread;
    ABTI_ASSERT(p_prev->type & ABTI_THREAD_TYPE_YIELDABLE);
    /* p_old keeps running as a parent, so no thread-run event incurs. */
    return ABTI_thread_get_ythread(p_prev);
}

#ifdef ABT_CONFIG_ENABLE_PEEK_CONTEXT
static inline void ABTI_ythread_context_peek(ABTI_ythread *p_ythread,
                                             void (*peek_func)(void *),
                                             void *arg)
{
    ABTD_ythread_context_peek(&p_ythread->ctx, peek_func, arg);
}
#endif

/* Return the previous thread. */
static inline ABTI_ythread *
ABTI_ythread_context_switch_to_sibling(ABTI_xstream **pp_local_xstream,
                                       ABTI_ythread *p_old, ABTI_ythread *p_new)
{
    return ABTI_ythread_context_switch_to_sibling_internal(pp_local_xstream,
                                                           p_old, p_new,
                                                           ABT_FALSE);
}

static inline ABTI_ythread *ABTI_ythread_context_switch_to_parent(
    ABTI_xstream **pp_local_xstream, ABTI_ythread *p_old,
    ABT_sync_event_type sync_event_type, void *p_sync)
{
    return ABTI_ythread_context_switch_to_parent_internal(pp_local_xstream,
                                                          p_old, ABT_FALSE,
                                                          sync_event_type,
                                                          p_sync);
}

static inline ABTI_ythread *
ABTI_ythread_context_switch_to_child(ABTI_xstream **pp_local_xstream,
                                     ABTI_ythread *p_old, ABTI_ythread *p_new)
{
    return ABTI_ythread_context_switch_to_child_internal(pp_local_xstream,
                                                         p_old, p_new);
}

ABTU_noreturn static inline void
ABTI_ythread_finish_context_to_sibling(ABTI_xstream *p_local_xstream,
                                       ABTI_ythread *p_old, ABTI_ythread *p_new)
{
    ABTI_ythread_context_switch_to_sibling_internal(&p_local_xstream, p_old,
                                                    p_new, ABT_TRUE);
    ABTU_unreachable();
}

ABTU_noreturn static inline void
ABTI_ythread_finish_context_to_parent(ABTI_xstream *p_local_xstream,
                                      ABTI_ythread *p_old)
{
    ABTI_ythread_context_switch_to_parent_internal(&p_local_xstream, p_old,
                                                   ABT_TRUE,
                                                   ABT_SYNC_EVENT_TYPE_UNKNOWN,
                                                   NULL);
    ABTU_unreachable();
}

static inline void ABTI_ythread_yield(ABTI_xstream **pp_local_xstream,
                                      ABTI_ythread *p_ythread,
                                      ABT_sync_event_type sync_event_type,
                                      void *p_sync)
{
    LOG_DEBUG("[U%" PRIu64 ":E%d] yield\n",
              ABTI_thread_get_id(&p_ythread->thread),
              p_ythread->thread.p_last_xstream->rank);

    /* Change the state of current running thread */
    ABTD_atomic_release_store_int(&p_ythread->thread.state,
                                  ABT_THREAD_STATE_READY);

    /* Switch to the top scheduler */
    ABTI_ythread_context_switch_to_parent(pp_local_xstream, p_ythread,
                                          sync_event_type, p_sync);

    /* Back to the original thread */
    LOG_DEBUG("[U%" PRIu64 ":E%d] resume after yield\n",
              ABTI_thread_get_id(&p_ythread->thread),
              p_ythread->thread.p_last_xstream->rank);
}

#endif /* ABTI_YTHREAD_H_INCLUDED */
