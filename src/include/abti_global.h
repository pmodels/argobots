/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#ifndef GLOBAL_H_INCLUDED
#define GLOBAL_H_INCLUDED

/* Inlined functions for Global Data */

static inline
size_t ABTI_global_get_thread_stacksize(void)
{
    return gp_ABTI_global->thread_stacksize;
}

static inline
size_t ABTI_global_get_sched_stacksize(void)
{
    return gp_ABTI_global->sched_stacksize;
}

static inline
size_t ABTI_global_get_sched_event_freq(void)
{
    return gp_ABTI_global->sched_event_freq;
}

static inline
ABTI_thread *ABTI_global_get_main(void)
{
    return gp_ABTI_global->p_thread_main;
}

#endif /* GLOBAL_H_INCLUDED */

