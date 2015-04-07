/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#ifndef MUTEX_H_INCLUDED
#define MUTEX_H_INCLUDED

/* Inlined functions for Mutex */

static inline
ABTI_mutex *ABTI_mutex_get_ptr(ABT_mutex mutex)
{
#ifndef UNSAFE_MODE
    ABTI_mutex *p_mutex;
    if (mutex == ABT_MUTEX_NULL) {
        p_mutex = NULL;
    } else {
        p_mutex = (ABTI_mutex *)mutex;
    }
    return p_mutex;
#else
    return (ABTI_mutex *)mutex;
#endif
}

static inline
ABT_mutex ABTI_mutex_get_handle(ABTI_mutex *p_mutex)
{
#ifndef UNSAFE_MODE
    ABT_mutex h_mutex;
    if (p_mutex == NULL) {
        h_mutex = ABT_MUTEX_NULL;
    } else {
        h_mutex = (ABT_mutex)p_mutex;
    }
    return h_mutex;
#else
    return (ABT_mutex)p_mutex;
#endif
}

#endif /* MUTEX_H_INCLUDED */

