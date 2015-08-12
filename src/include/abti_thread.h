/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#ifndef THREAD_H_INCLUDED
#define THREAD_H_INCLUDED

/* Inlined functions for User-level Thread (ULT) */

static inline
ABTI_thread *ABTI_thread_get_ptr(ABT_thread thread)
{
#ifndef UNSAFE_MODE
    ABTI_thread *p_thread;
    if (thread == ABT_THREAD_NULL) {
        p_thread = NULL;
    } else {
        p_thread = (ABTI_thread *)thread;
    }
    return p_thread;
#else
    return (ABTI_thread *)thread;
#endif
}

static inline
ABT_thread ABTI_thread_get_handle(ABTI_thread *p_thread)
{
#ifndef UNSAFE_MODE
    ABT_thread h_thread;
    if (p_thread == NULL) {
        h_thread = ABT_THREAD_NULL;
    } else {
        h_thread = (ABT_thread)p_thread;
    }
    return h_thread;
#else
    return (ABT_thread)p_thread;
#endif
}

static inline
void ABTI_thread_set_request(ABTI_thread *p_thread, uint32_t req)
{
    ABTD_atomic_fetch_or_uint32(&p_thread->request, req);
}

static inline
void ABTI_thread_unset_request(ABTI_thread *p_thread, uint32_t req)
{
    ABTD_atomic_fetch_and_uint32(&p_thread->request, ~req);
}

#endif /* THREAD_H_INCLUDED */

