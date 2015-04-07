/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#ifndef TASK_H_INCLUDED
#define TASK_H_INCLUDED

/* Inlined functions for Tasklet  */

static inline
ABTI_task *ABTI_task_get_ptr(ABT_task task)
{
#ifndef UNSAFE_MODE
    ABTI_task *p_task;
    if (task == ABT_TASK_NULL) {
        p_task = NULL;
    } else {
        p_task = (ABTI_task *)task;
    }
    return p_task;
#else
    return (ABTI_task *)task;
#endif
}

static inline
ABT_task ABTI_task_get_handle(ABTI_task *p_task)
{
#ifndef UNSAFE_MODE
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

#endif /* TASK_H_INCLUDED */

