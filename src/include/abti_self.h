/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#ifndef ABTI_SELF_H_INCLUDED
#define ABTI_SELF_H_INCLUDED

static inline
ABTI_unit *ABTI_self_get_unit(ABTI_local *p_local)
{
    ABTI_ASSERT(gp_ABTI_global);

    ABTI_unit *p_unit;
    ABTI_thread *p_thread;
    ABTI_task *p_task;

#ifndef ABT_CONFIG_DISABLE_EXT_THREAD
    /* This is when an external thread called this routine. */
    if (p_local == NULL) {
        ABTD_xstream_context ctx;
        ABTD_xstream_context_self(&ctx);
        p_unit = (ABTI_unit *)ctx;
        return p_unit;
    }
#endif

    if ((p_thread = p_local->p_thread)) {
        p_unit = &p_thread->unit_def;
    } else if ((p_task = p_local->p_task)) {
        p_unit = &p_task->unit_def;
    } else {
        /* should not reach here */
        p_unit = NULL;
        ABTI_ASSERT(0);
    }

    return p_unit;
}

static inline
ABT_unit_type ABTI_self_get_type(ABTI_local *p_local)
{
    ABTI_ASSERT(gp_ABTI_global);

#ifndef ABT_CONFIG_DISABLE_EXT_THREAD
    /* This is when an external thread called this routine. */
    if (p_local == NULL) {
        return ABT_UNIT_TYPE_EXT;
    }
#endif

    if (p_local->p_task != NULL) {
        return ABT_UNIT_TYPE_TASK;
    } else {
        /* Since p_local->p_thread can return NULL during executing
         * ABTI_init(), it should always be safe to say that the type of caller
         * is ULT if the control reaches here. */
        return ABT_UNIT_TYPE_THREAD;
    }
}

#endif /* ABTI_SELF_H_INCLUDED */

