/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#ifndef TIMER_H_INCLUDED
#define TIMER_H_INCLUDED

/* Inlined functions for Timer */

static inline
ABTI_timer *ABTI_timer_get_ptr(ABT_timer timer)
{
#ifndef ABT_CONFIG_DISABLE_ERROR_CHECK
    ABTI_timer *p_timer;
    if (timer == ABT_TIMER_NULL) {
        p_timer = NULL;
    } else {
        p_timer = (ABTI_timer *)timer;
    }
    return p_timer;
#else
    return (ABTI_timer *)timer;
#endif
}

static inline
ABT_timer ABTI_timer_get_handle(ABTI_timer *p_timer)
{
#ifndef ABT_CONFIG_DISABLE_ERROR_CHECK
    ABT_timer h_timer;
    if (p_timer == NULL) {
        h_timer = ABT_TIMER_NULL;
    } else {
        h_timer = (ABT_timer)p_timer;
    }
    return h_timer;
#else
    return (ABT_timer)p_timer;
#endif
}

#endif /* TIMER_H_INCLUDED */

