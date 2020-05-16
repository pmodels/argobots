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
    return (ABTI_native_thread_id)p_local_xstream->p_xstream;
}

static inline ABTI_unit_id ABTI_self_get_unit_id(ABTI_xstream *p_local_xstream)
{
#ifndef ABT_CONFIG_DISABLE_EXT_THREAD
    /* This is when an external thread called this routine. */
    if (p_local_xstream == NULL) {
        /* A pointer to a thread local variable is unique to an external thread
         * and its value is different from pointers to ULTs and tasks. */
        return (ABTI_unit_id)ABTI_local_get_local_ptr();
    }
#endif
    ABTI_unit_id id;
    if (p_local_xstream->p_thread) {
        id = (ABTI_unit_id)p_local_xstream->p_thread;
    } else if (p_local_xstream->p_task) {
        id = (ABTI_unit_id)p_local_xstream->p_task;
    } else {
        /* should not reach here */
        id = 0;
        ABTI_ASSERT(0);
    }
    return id;
}

static inline ABT_unit_type ABTI_self_get_type(ABTI_xstream *p_local_xstream)
{
    ABTI_ASSERT(gp_ABTI_global);

#ifndef ABT_CONFIG_DISABLE_EXT_THREAD
    /* This is when an external thread called this routine. */
    if (p_local_xstream == NULL) {
        return ABT_UNIT_TYPE_EXT;
    }
#endif

    if (p_local_xstream->p_task != NULL) {
        return ABT_UNIT_TYPE_TASK;
    } else {
        /* Since p_local_xstream->p_thread can return NULL during executing
         * ABTI_init(), it should always be safe to say that the type of caller
         * is ULT if the control reaches here. */
        return ABT_UNIT_TYPE_THREAD;
    }
}

#endif /* ABTI_SELF_H_INCLUDED */
