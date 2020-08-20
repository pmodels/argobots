/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#ifndef ABTI_TASK_H_INCLUDED
#define ABTI_TASK_H_INCLUDED

/* Inlined functions for Tasklet  */

static inline ABTI_thread *ABTI_task_get_ptr(ABT_task task)
{
#ifndef ABT_CONFIG_DISABLE_ERROR_CHECK
    ABTI_thread *p_task;
    if (task == ABT_TASK_NULL) {
        p_task = NULL;
    } else {
        p_task = (ABTI_thread *)task;
    }
    return p_task;
#else
    return (ABTI_thread *)task;
#endif
}

static inline ABT_task ABTI_task_get_handle(ABTI_thread *p_task)
{
#ifndef ABT_CONFIG_DISABLE_ERROR_CHECK
    ABT_task h_task;
    if (p_task == NULL) {
        h_task = ABT_TASK_NULL;
    } else {
        h_task = (ABT_task)p_task;
    }
    return h_task;
#else
    return (ABT_task)p_task;
#endif
}

#endif /* ABTI_TASK_H_INCLUDED */
