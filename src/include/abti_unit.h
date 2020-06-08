/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#ifndef ABTI_UNIT_H_INCLUDED
#define ABTI_UNIT_H_INCLUDED

/* Inlined functions for ABTI_unit */

static inline ABT_thread_state
ABTI_unit_state_get_thread_state(ABTI_unit_state state)
{
    switch (state) {
        case ABTI_UNIT_STATE_READY:
            return ABT_THREAD_STATE_READY;
        case ABTI_UNIT_STATE_RUNNING:
            return ABT_THREAD_STATE_RUNNING;
        case ABTI_UNIT_STATE_BLOCKED:
            return ABT_THREAD_STATE_BLOCKED;
        case ABTI_UNIT_STATE_TERMINATED:
            return ABT_THREAD_STATE_TERMINATED;
        default:
            ABTI_ASSERT(0);
            return (ABT_thread_state)-1;
    }
}

static inline ABT_task_state
ABTI_unit_state_get_task_state(ABTI_unit_state state)
{
    switch (state) {
        case ABTI_UNIT_STATE_READY:
            return ABT_TASK_STATE_READY;
        case ABTI_UNIT_STATE_RUNNING:
            return ABT_TASK_STATE_RUNNING;
        case ABTI_UNIT_STATE_TERMINATED:
            return ABT_TASK_STATE_TERMINATED;
        case ABTI_UNIT_STATE_BLOCKED:
        default:
            ABTI_ASSERT(0);
            return (ABT_task_state)-1;
    }
}

#endif /* ABTI_UNIT_H_INCLUDED */
