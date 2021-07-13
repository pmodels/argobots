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

static inline void ABTI_ythread_set_ready(ABTI_local *p_local,
                                          ABTI_ythread *p_ythread)
{
    /* The ULT must be in BLOCKED state. */
    ABTI_ASSERT(ABTD_atomic_acquire_load_int(&p_ythread->thread.state) ==
                ABT_THREAD_STATE_BLOCKED);

    ABTI_event_ythread_resume(p_local, p_ythread,
                              ABTI_local_get_xstream_or_null(p_local)
                                  ? ABTI_local_get_xstream(p_local)->p_thread
                                  : NULL);
    /* p_ythread->thread.p_pool is loaded before ABTI_POOL_ADD_THREAD to keep
     * num_blocked consistent. Otherwise, other threads might pop p_ythread
     * that has been pushed in ABTI_POOL_ADD_THREAD and change
     * p_ythread->thread.p_pool by ABT_unit_set_associated_pool. */
    ABTI_pool *p_pool = p_ythread->thread.p_pool;

    /* Add the ULT to its associated pool */
    ABTI_pool_add_thread(&p_ythread->thread);

    /* Decrease the number of blocked threads */
    ABTI_pool_dec_num_blocked(p_pool);
}

static inline ABTI_ythread *
ABTI_ythread_context_get_ythread(ABTD_ythread_context *p_ctx)
{
    return (ABTI_ythread *)(((char *)p_ctx) - offsetof(ABTI_ythread, ctx));
}

ABTU_noreturn static inline void ABTI_ythread_context_jump(ABTI_ythread *p_new)
{
    ABTD_ythread_context_jump(&p_new->ctx);
    ABTU_unreachable();
}

static inline void ABTI_ythread_context_switch(ABTI_ythread *p_old,
                                               ABTI_ythread *p_new)
{
    ABTD_ythread_context_switch(&p_old->ctx, &p_new->ctx);
    /* Return the previous thread. */
}

ABTU_noreturn static inline void
ABTI_ythread_context_jump_with_call(ABTI_ythread *p_new, void (*f_cb)(void *),
                                    void *cb_arg)
{
    ABTD_ythread_context_jump_with_call(&p_new->ctx, f_cb, cb_arg);
    ABTU_unreachable();
}

static inline void ABTI_ythread_context_switch_with_call(ABTI_ythread *p_old,
                                                         ABTI_ythread *p_new,
                                                         void (*f_cb)(void *),
                                                         void *cb_arg)
{
    ABTD_ythread_context_switch_with_call(&p_old->ctx, &p_new->ctx, f_cb,
                                          cb_arg);
    /* Return the previous thread. */
}

ABTU_noreturn static inline void
ABTI_ythread_jump_to_primary(ABTI_xstream *p_local_xstream, ABTI_ythread *p_new)
{
    p_local_xstream->p_thread = &p_new->thread;
    p_new->thread.p_last_xstream = p_local_xstream;
    ABTI_ythread_context_jump(p_new);
    ABTU_unreachable();
}

static inline void
ABTI_ythread_switch_to_child_internal(ABTI_xstream **pp_local_xstream,
                                      ABTI_ythread *p_old, ABTI_ythread *p_new)
{
    p_new->thread.p_parent = &p_old->thread;
    ABTI_xstream *p_local_xstream = *pp_local_xstream;
    ABTI_event_thread_run(p_local_xstream, &p_new->thread, &p_old->thread,
                          p_new->thread.p_parent);
    p_local_xstream->p_thread = &p_new->thread;
    p_new->thread.p_last_xstream = p_local_xstream;
    /* Context switch starts. */
    ABTI_ythread_context_switch(p_old, p_new);
    /* Context switch finishes. */
    *pp_local_xstream = p_old->thread.p_last_xstream;
}

ABTU_noreturn static inline void
ABTI_ythread_jump_to_sibling_internal(ABTI_xstream *p_local_xstream,
                                      ABTI_ythread *p_old, ABTI_ythread *p_new,
                                      void (*f_cb)(void *), void *cb_arg)
{
    p_new->thread.p_parent = p_old->thread.p_parent;
    ABTI_event_thread_run(p_local_xstream, &p_new->thread, &p_old->thread,
                          p_new->thread.p_parent);
    p_local_xstream->p_thread = &p_new->thread;
    p_new->thread.p_last_xstream = p_local_xstream;
    ABTI_ythread_context_jump_with_call(p_new, f_cb, cb_arg);
    ABTU_unreachable();
}

static inline void ABTI_ythread_switch_to_sibling_internal(
    ABTI_xstream **pp_local_xstream, ABTI_ythread *p_old, ABTI_ythread *p_new,
    void (*f_cb)(void *), void *cb_arg)
{
    p_new->thread.p_parent = p_old->thread.p_parent;
    ABTI_xstream *p_local_xstream = *pp_local_xstream;
    ABTI_event_thread_run(p_local_xstream, &p_new->thread, &p_old->thread,
                          p_new->thread.p_parent);
    p_local_xstream->p_thread = &p_new->thread;
    p_new->thread.p_last_xstream = p_local_xstream;
    /* Context switch starts. */
    ABTI_ythread_context_switch_with_call(p_old, p_new, f_cb, cb_arg);
    /* Context switch finishes. */
    *pp_local_xstream = p_old->thread.p_last_xstream;
}

ABTU_noreturn static inline void
ABTI_ythread_jump_to_parent_internal(ABTI_xstream *p_local_xstream,
                                     ABTI_ythread *p_old, void (*f_cb)(void *),
                                     void *cb_arg)
{
    ABTI_ythread *p_new = ABTI_thread_get_ythread(p_old->thread.p_parent);
    p_local_xstream->p_thread = &p_new->thread;
    ABTI_ASSERT(p_new->thread.p_last_xstream == p_local_xstream);
    ABTI_ythread_context_jump_with_call(p_new, f_cb, cb_arg);
    ABTU_unreachable();
}

static inline void
ABTI_ythread_switch_to_parent_internal(ABTI_xstream **pp_local_xstream,
                                       ABTI_ythread *p_old,
                                       void (*f_cb)(void *), void *cb_arg)
{
    ABTI_ythread *p_new = ABTI_thread_get_ythread(p_old->thread.p_parent);
    ABTI_xstream *p_local_xstream = *pp_local_xstream;
    p_local_xstream->p_thread = &p_new->thread;
    ABTI_ASSERT(p_new->thread.p_last_xstream == p_local_xstream);
    /* Context switch starts. */
    ABTI_ythread_context_switch_with_call(p_old, p_new, f_cb, cb_arg);
    /* Context switch finishes. */
    *pp_local_xstream = p_old->thread.p_last_xstream;
}

static inline ABT_bool ABTI_ythread_context_peek(ABTI_ythread *p_ythread,
                                                 void (*f_peek)(void *),
                                                 void *arg)
{
    return ABTD_ythread_context_peek(&p_ythread->ctx, f_peek, arg);
}

static inline void ABTI_ythread_run_child(ABTI_xstream **pp_local_xstream,
                                          ABTI_ythread *p_parent,
                                          ABTI_ythread *p_child)
{
    ABTD_atomic_release_store_int(&p_child->thread.state,
                                  ABT_THREAD_STATE_RUNNING);
    ABTI_ythread_switch_to_child_internal(pp_local_xstream, p_parent, p_child);
}

void ABTI_ythread_callback_yield(void *arg);

static inline void ABTI_ythread_yield(ABTI_xstream **pp_local_xstream,
                                      ABTI_ythread *p_ythread,
                                      ABT_sync_event_type sync_event_type,
                                      void *p_sync)
{
    ABTI_event_ythread_yield(*pp_local_xstream, p_ythread,
                             p_ythread->thread.p_parent, sync_event_type,
                             p_sync);
    ABTI_ythread_switch_to_parent_internal(pp_local_xstream, p_ythread,
                                           ABTI_ythread_callback_yield,
                                           (void *)p_ythread);
}

static inline void ABTI_ythread_yield_to(ABTI_xstream **pp_local_xstream,
                                         ABTI_ythread *p_self,
                                         ABTI_ythread *p_ythread,
                                         ABT_sync_event_type sync_event_type,
                                         void *p_sync)
{
    ABTI_event_ythread_yield(*pp_local_xstream, p_self, p_self->thread.p_parent,
                             sync_event_type, p_sync);
    ABTD_atomic_release_store_int(&p_ythread->thread.state,
                                  ABT_THREAD_STATE_RUNNING);
    ABTI_ythread_switch_to_sibling_internal(pp_local_xstream, p_self, p_ythread,
                                            ABTI_ythread_callback_yield,
                                            (void *)p_self);
}

/* Old interface used for ABT_thread_yield_to() */
void ABTI_ythread_callback_thread_yield_to(void *arg);

static inline void
ABTI_ythread_thread_yield_to(ABTI_xstream **pp_local_xstream,
                             ABTI_ythread *p_self, ABTI_ythread *p_ythread,
                             ABT_sync_event_type sync_event_type, void *p_sync)
{
    ABTI_event_ythread_yield(*pp_local_xstream, p_self, p_self->thread.p_parent,
                             sync_event_type, p_sync);
    ABTD_atomic_release_store_int(&p_ythread->thread.state,
                                  ABT_THREAD_STATE_RUNNING);
    ABTI_ythread_switch_to_sibling_internal(
        pp_local_xstream, p_self, p_ythread,
        ABTI_ythread_callback_thread_yield_to, (void *)p_self);
}

void ABTI_ythread_callback_suspend(void *arg);

static inline void ABTI_ythread_suspend(ABTI_xstream **pp_local_xstream,
                                        ABTI_ythread *p_ythread,
                                        ABT_sync_event_type sync_event_type,
                                        void *p_sync)
{
    ABTI_event_ythread_suspend(*pp_local_xstream, p_ythread,
                               p_ythread->thread.p_parent, sync_event_type,
                               p_sync);
    ABTI_ythread_switch_to_parent_internal(pp_local_xstream, p_ythread,
                                           ABTI_ythread_callback_suspend,
                                           (void *)p_ythread);
}

static inline void ABTI_ythread_suspend_to(ABTI_xstream **pp_local_xstream,
                                           ABTI_ythread *p_self,
                                           ABTI_ythread *p_ythread,
                                           ABT_sync_event_type sync_event_type,
                                           void *p_sync)
{
    ABTI_event_ythread_suspend(*pp_local_xstream, p_self,
                               p_self->thread.p_parent, sync_event_type,
                               p_sync);
    ABTI_ythread_switch_to_sibling_internal(pp_local_xstream, p_self, p_ythread,
                                            ABTI_ythread_callback_suspend,
                                            (void *)p_self);
}

void ABTI_ythread_callback_terminate(void *arg);

ABTU_noreturn static inline void
ABTI_ythread_terminate(ABTI_xstream *p_local_xstream, ABTI_ythread *p_ythread)
{
    ABTI_event_thread_finish(p_local_xstream, &p_ythread->thread,
                             p_ythread->thread.p_parent);
    ABTI_ythread_jump_to_parent_internal(p_local_xstream, p_ythread,
                                         ABTI_ythread_callback_terminate,
                                         (void *)p_ythread);
    ABTU_unreachable();
}

ABTU_noreturn static inline void
ABTI_ythread_terminate_to(ABTI_xstream *p_local_xstream,
                          ABTI_ythread *p_ythread, ABTI_ythread *p_target)
{
    ABTI_event_thread_finish(p_local_xstream, &p_ythread->thread,
                             p_ythread->thread.p_parent);
    ABTD_atomic_release_store_int(&p_ythread->thread.state,
                                  ABT_THREAD_STATE_RUNNING);
    ABTI_ythread_jump_to_sibling_internal(p_local_xstream, p_ythread, p_target,
                                          ABTI_ythread_callback_terminate,
                                          (void *)p_ythread);
    ABTU_unreachable();
}

typedef struct {
    ABTI_ythread *p_prev;
    ABTD_spinlock *p_lock;
} ABTI_ythread_callback_suspend_unlock_arg;

void ABTI_ythread_callback_suspend_unlock(void *arg);

static inline void
ABTI_ythread_suspend_unlock(ABTI_xstream **pp_local_xstream,
                            ABTI_ythread *p_ythread, ABTD_spinlock *p_lock,
                            ABT_sync_event_type sync_event_type, void *p_sync)
{
    ABTI_event_ythread_suspend(*pp_local_xstream, p_ythread,
                               p_ythread->thread.p_parent, sync_event_type,
                               p_sync);
    ABTI_ythread_callback_suspend_unlock_arg arg = { p_ythread, p_lock };
    ABTI_ythread_switch_to_parent_internal(pp_local_xstream, p_ythread,
                                           ABTI_ythread_callback_suspend_unlock,
                                           (void *)&arg);
}

typedef struct {
    ABTI_ythread *p_prev;
    ABTI_ythread *p_target;
} ABTI_ythread_callback_suspend_join_arg;

void ABTI_ythread_callback_suspend_join(void *arg);

static inline void
ABTI_ythread_suspend_join(ABTI_xstream **pp_local_xstream, ABTI_ythread *p_self,
                          ABTI_ythread *p_target,
                          ABT_sync_event_type sync_event_type, void *p_sync)
{
    ABTI_event_ythread_suspend(*pp_local_xstream, p_self,
                               p_self->thread.p_parent, sync_event_type,
                               p_sync);
    ABTI_ythread_callback_suspend_join_arg arg = { p_self, p_target };
    ABTI_ythread_switch_to_parent_internal(pp_local_xstream, p_self,
                                           ABTI_ythread_callback_suspend_join,
                                           (void *)&arg);
}

typedef struct {
    ABTI_ythread *p_prev;
    ABTI_sched *p_main_sched;
} ABTI_ythread_callback_suspend_replace_sched_arg;

void ABTI_ythread_callback_suspend_replace_sched(void *arg);

static inline void ABTI_ythread_suspend_replace_sched(
    ABTI_xstream **pp_local_xstream, ABTI_ythread *p_ythread,
    ABTI_sched *p_main_sched, ABT_sync_event_type sync_event_type, void *p_sync)
{
    ABTI_event_ythread_suspend(*pp_local_xstream, p_ythread,
                               p_ythread->thread.p_parent, sync_event_type,
                               p_sync);
    ABTI_ythread_callback_suspend_replace_sched_arg arg = { p_ythread,
                                                            p_main_sched };
    ABTI_ythread_switch_to_parent_internal(
        pp_local_xstream, p_ythread,
        ABTI_ythread_callback_suspend_replace_sched, (void *)&arg);
}

void ABTI_ythread_callback_orphan(void *arg);

static inline void
ABTI_ythread_yield_orphan(ABTI_xstream **pp_local_xstream,
                          ABTI_ythread *p_ythread,
                          ABT_sync_event_type sync_event_type, void *p_sync)
{
    ABTI_event_ythread_suspend(*pp_local_xstream, p_ythread,
                               p_ythread->thread.p_parent, sync_event_type,
                               p_sync);
    ABTI_ythread_switch_to_parent_internal(pp_local_xstream, p_ythread,
                                           ABTI_ythread_callback_orphan,
                                           (void *)p_ythread);
}

#endif /* ABTI_YTHREAD_H_INCLUDED */
