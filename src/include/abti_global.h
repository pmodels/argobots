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
long ABTI_global_get_sched_sleep_nsec(void)
{
    return gp_ABTI_global->sched_sleep_nsec;
}

static inline
ABTI_thread *ABTI_global_get_main(void)
{
    return gp_ABTI_global->p_thread_main;
}

static inline
uint32_t ABTI_global_get_mutex_max_handovers(void)
{
    return gp_ABTI_global->mutex_max_handovers;
}

static inline
uint32_t ABTI_global_get_mutex_max_wakeups(void)
{
    return gp_ABTI_global->mutex_max_wakeups;
}

#endif /* GLOBAL_H_INCLUDED */

