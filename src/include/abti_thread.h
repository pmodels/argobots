/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#ifndef ABTI_THREAD_H_INCLUDED
#define ABTI_THREAD_H_INCLUDED

/* Inlined functions for User-level Thread (ULT) */

static inline ABT_thread_state
ABTI_thread_state_get_thread_state(ABTI_thread_state state)
{
    switch (state) {
        case ABTI_THREAD_STATE_READY:
            return ABT_THREAD_STATE_READY;
        case ABTI_THREAD_STATE_RUNNING:
            return ABT_THREAD_STATE_RUNNING;
        case ABTI_THREAD_STATE_BLOCKED:
            return ABT_THREAD_STATE_BLOCKED;
        case ABTI_THREAD_STATE_TERMINATED:
            return ABT_THREAD_STATE_TERMINATED;
        default:
            ABTI_ASSERT(0);
            ABTU_unreachable();
    }
}

static inline ABT_task_state
ABTI_thread_state_get_task_state(ABTI_thread_state state)
{
    switch (state) {
        case ABTI_THREAD_STATE_READY:
            return ABT_TASK_STATE_READY;
        case ABTI_THREAD_STATE_RUNNING:
            return ABT_TASK_STATE_RUNNING;
        case ABTI_THREAD_STATE_TERMINATED:
            return ABT_TASK_STATE_TERMINATED;
        case ABTI_THREAD_STATE_BLOCKED:
        default:
            ABTI_ASSERT(0);
            ABTU_unreachable();
    }
}

static inline ABT_bool ABTI_thread_type_is_thread(ABTI_thread_type type)
{
    return (type & ABTI_THREAD_TYPE_THREAD) ? ABT_TRUE : ABT_FALSE;
}

static inline ABT_bool ABTI_thread_type_is_task(ABTI_thread_type type)
{
    return (!(type & (ABTI_THREAD_TYPE_THREAD | ABTI_THREAD_TYPE_EXT)))
               ? ABT_TRUE
               : ABT_FALSE;
}

static inline ABT_bool ABTI_thread_type_is_ext(ABTI_thread_type type)
{
    return (type & ABTI_THREAD_TYPE_EXT) ? ABT_TRUE : ABT_FALSE;
}

static inline ABT_bool ABTI_thread_type_is_thread_user(ABTI_thread_type type)
{
    return (type & ABTI_THREAD_TYPE_THREAD_TYPE_USER) ? ABT_TRUE : ABT_FALSE;
}

static inline ABT_bool ABTI_thread_type_is_thread_main(ABTI_thread_type type)
{
    return (type & ABTI_THREAD_TYPE_THREAD_TYPE_MAIN) ? ABT_TRUE : ABT_FALSE;
}

static inline ABT_bool
ABTI_thread_type_is_thread_main_sched(ABTI_thread_type type)
{
    return (type & ABTI_THREAD_TYPE_THREAD_TYPE_MAIN_SCHED) ? ABT_TRUE
                                                            : ABT_FALSE;
}

static inline ABT_unit_type ABTI_thread_type_get_type(ABTI_thread_type type)
{
    if (ABTI_thread_type_is_thread(type)) {
        return ABT_UNIT_TYPE_THREAD;
    } else if (ABTI_thread_type_is_task(type)) {
        return ABT_UNIT_TYPE_TASK;
    } else {
        ABTI_ASSERT(ABTI_thread_type_is_ext(type));
        return ABT_UNIT_TYPE_EXT;
    }
}

static inline ABTI_ythread *ABTI_thread_get_ythread(ABTI_thread *p_thread)
{
    return (ABTI_ythread *)(((char *)p_thread) -
                            offsetof(ABTI_ythread, thread));
}

static inline void ABTI_thread_set_request(ABTI_thread *p_thread, uint32_t req)
{
    ABTD_atomic_fetch_or_uint32(&p_thread->request, req);
}

static inline void ABTI_thread_unset_request(ABTI_thread *p_thread,
                                             uint32_t req)
{
    ABTD_atomic_fetch_and_uint32(&p_thread->request, ~req);
}

#endif /* ABTI_THREAD_H_INCLUDED */
