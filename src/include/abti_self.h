/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#ifndef ABTI_SELF_H_INCLUDED
#define ABTI_SELF_H_INCLUDED

static inline ABTI_native_thread_id
ABTI_self_get_native_thread_id(ABTI_xstream *p_local_xstream)
{
#ifndef ABT_CONFIG_DISABLE_EXT_THREAD
    /* This is when an external thread called this routine. */
    if (p_local_xstream == NULL) {
        /* A pointer to a thread local variable can distinguish all external
         * threads and execution streams. */
        return (ABTI_native_thread_id)ABTI_local_get_local_ptr();
    }
#endif
    return (ABTI_native_thread_id)p_local_xstream;
}

static inline ABTI_thread_id
ABTI_self_get_thread_id(ABTI_xstream *p_local_xstream)
{
#ifndef ABT_CONFIG_DISABLE_EXT_THREAD
    /* This is when an external thread called this routine. */
    if (p_local_xstream == NULL) {
        /* A pointer to a thread local variable is unique to an external thread
         * and its value is different from pointers to ULTs and tasks. */
        return (ABTI_thread_id)ABTI_local_get_local_ptr();
    }
#endif
    return (ABTI_thread_id)p_local_xstream->p_thread;
}

static inline ABTI_thread_type ABTI_self_get_type(ABTI_xstream *p_local_xstream)
{
    ABTI_ASSERT(gp_ABTI_global);

#ifndef ABT_CONFIG_DISABLE_EXT_THREAD
    /* This is when an external thread called this routine. */
    if (p_local_xstream == NULL) {
        return ABTI_THREAD_TYPE_EXT;
    }
#endif
    if (p_local_xstream->p_thread) {
        return p_local_xstream->p_thread->type;
    } else {
        /* Since p_local_xstream->p_thread can return NULL during executing
         * ABTI_init(), it should always be safe to say that the type of caller
         * is ULT if the control reaches here. */
        return ABTI_THREAD_TYPE_THREAD_MAIN;
    }
}

#endif /* ABTI_SELF_H_INCLUDED */
