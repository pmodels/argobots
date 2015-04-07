/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#ifndef SCHED_H_INCLUDED
#define SCHED_H_INCLUDED

/* Inlined functions for Scheduler */

static inline
ABTI_sched *ABTI_sched_get_ptr(ABT_sched sched)
{
#ifndef UNSAFE_MODE
    ABTI_sched *p_sched;
    if (sched == ABT_SCHED_NULL) {
        p_sched = NULL;
    } else {
        p_sched = (ABTI_sched *)sched;
    }
    return p_sched;
#else
    return (ABTI_sched *)sched;
#endif
}

static inline
ABT_sched ABTI_sched_get_handle(ABTI_sched *p_sched)
{
#ifndef UNSAFE_MODE
    ABT_sched h_sched;
    if (p_sched == NULL) {
        h_sched = ABT_SCHED_NULL;
    } else {
        h_sched = (ABT_sched)p_sched;
    }
    return h_sched;
#else
    return (ABT_sched)p_sched;
#endif
}

#endif /* SCHED_H_INCLUDED */

