/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#ifndef ABTI_UNIT_H_INCLUDED
#define ABTI_UNIT_H_INCLUDED

/* Inlined functions for ABTI_thread */

static inline ABTI_ktable *
ABTI_ktable_ensure_allocation(ABTI_xstream *p_local_xstream,
                              ABTD_atomic_ptr *pp_ktable);
static inline void ABTI_ktable_set(ABTI_xstream *p_local_xstream,
                                   ABTI_ktable *p_ktable, ABTI_key *p_key,
                                   void *value);
static inline void *ABTI_ktable_get(ABTI_ktable *p_ktable, ABTI_key *p_key);
static inline int ABTI_ktable_is_valid(ABTI_ktable *p_ktable);

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
    ABTI_STATIC_ASSERT(!(ABTI_THREAD_TYPE_TASK & 0x1));
    ABTI_STATIC_ASSERT(!(ABTI_THREAD_TYPE_EXT & 0x1));
    ABTI_STATIC_ASSERT(ABTI_THREAD_TYPE_THREAD_MAIN_SCHED & 0x1);
    ABTI_STATIC_ASSERT(ABTI_THREAD_TYPE_THREAD_USER & 0x1);
    ABTI_STATIC_ASSERT(ABTI_THREAD_TYPE_THREAD_MAIN & 0x1);
    return (type & 0x1) ? ABT_TRUE : ABT_FALSE;
}

static inline ABT_unit_type ABTI_thread_type_get_type(ABTI_thread_type type)
{
    if (ABTI_thread_type_is_thread(type)) {
        return ABT_UNIT_TYPE_THREAD;
    } else if (type == ABTI_THREAD_TYPE_TASK) {
        return ABT_UNIT_TYPE_TASK;
    } else {
        ABTI_ASSERT(type == ABTI_THREAD_TYPE_EXT);
        return ABT_UNIT_TYPE_EXT;
    }
}

static inline void ABTI_thread_set_specific(ABTI_xstream *p_local_xstream,
                                            ABTI_thread *p_thread,
                                            ABTI_key *p_key, void *value)
{
    ABTI_ktable *p_ktable =
        ABTI_ktable_ensure_allocation(p_local_xstream, &p_thread->p_keytable);
    /* Save the value in the key-value table */
    ABTI_ktable_set(p_local_xstream, p_ktable, p_key, value);
}

static inline void *ABTI_thread_get_specific(ABTI_thread *p_thread,
                                             ABTI_key *p_key)
{
    ABTI_ktable *p_ktable = ABTD_atomic_acquire_load_ptr(&p_thread->p_keytable);
    if (ABTI_ktable_is_valid(p_ktable)) {
        /* Retrieve the value from the key-value table */
        return ABTI_ktable_get(p_ktable, p_key);
    }
    return NULL;
}

#endif /* ABTI_UNIT_H_INCLUDED */
