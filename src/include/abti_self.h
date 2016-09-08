/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#ifndef SELF_H_INCLUDED
#define SELF_H_INCLUDED

static inline
ABTI_unit *ABTI_self_get_unit(void)
{
    ABTI_ASSERT(gp_ABTI_global);

    ABTI_unit *p_unit;
    ABTI_thread *p_thread;
    ABTI_task *p_task;

#ifndef ABT_CONFIG_DISABLE_EXT_THREAD
    /* This is when an external thread called this routine. */
    if (lp_ABTI_local == NULL) {
        ABTD_xstream_context ctx;
        ABTD_xstream_context_self(&ctx);
        p_unit = (ABTI_unit *)ctx;
        return p_unit;
    }
#endif

    if ((p_thread = ABTI_local_get_thread())) {
        p_unit = &p_thread->unit_def;
    } else if ((p_task = ABTI_local_get_task())) {
        p_unit = &p_task->unit_def;
    } else {
        /* should not reach here */
        p_unit = NULL;
        ABTI_ASSERT(0);
    }

    return p_unit;
}

#endif /* SELF_H_INCLUDED */

