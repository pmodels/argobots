/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"

int ABT_Scheduler_create(ABT_Pool pool,
                         const ABT_Scheduler_funcs *funcs,
                         ABT_Scheduler *newsched)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_Scheduler *p_sched;

    p_sched = (ABTI_Scheduler *)ABTU_Malloc(sizeof(ABTI_Scheduler));
    if (!p_sched) {
        HANDLE_ERROR("ABTU_Malloc");
        *newsched = NULL;
        abt_errno = ABT_ERR_MEM;
        goto fn_fail;
    }

    p_sched->type = ABTI_SCHEDULER_TYPE_USER;
    p_sched->pool = pool;

    p_sched->u_get_type = funcs->u_get_type;
    p_sched->u_get_thread = funcs->u_get_thread;
    p_sched->u_get_task = funcs->u_get_task;
    p_sched->u_create_from_thread = funcs->u_create_from_thread;
    p_sched->u_create_from_task = funcs->u_create_from_task;
    p_sched->u_free = funcs->u_free;

    p_sched->p_get_size = funcs->p_get_size;
    p_sched->p_push   = funcs->p_push;
    p_sched->p_pop    = funcs->p_pop;
    p_sched->p_remove = funcs->p_remove;

    *newsched = ABTI_Scheduler_get_handle(p_sched);

  fn_exit:
    return abt_errno;

  fn_fail:
    goto fn_exit;
}

int ABT_Scheduler_free(ABT_Scheduler sched)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_Scheduler *p_sched = ABTI_Scheduler_get_ptr(sched);

    /* If sched is a runtime-provided one, it should free its pool here.
     * Otherwise, freeing the pool is the user's reponsibility. */
    if (p_sched->type == ABTI_SCHEDULER_TYPE_BASE) {
        ABTI_Pool *p_pool = ABTI_Pool_get_ptr(p_sched->pool);
        abt_errno = ABTI_Pool_free(p_pool);
        if (abt_errno != ABT_SUCCESS) {
            HANDLE_ERROR("ABTI_Pool_free");
            goto fn_fail;
        }
    }

    free(p_sched);

  fn_exit:
    return abt_errno;

  fn_fail:
    goto fn_exit;
}


/* Internal functions */
int ABTI_Scheduler_create_default(ABTI_Scheduler **newsched)
{
    int abt_errno = ABT_SUCCESS;
    ABT_Scheduler sched;
    ABT_Pool pool;
    ABT_Scheduler_funcs funcs;

    /* Create a work unit pool */
    ABTI_Pool *p_pool;
    abt_errno = ABTI_Pool_create(&p_pool);
    if (abt_errno != ABT_SUCCESS) {
        HANDLE_ERROR("ABTI_Pool_create");
        goto fn_fail;
    }
    pool = ABTI_Pool_get_handle(p_pool);

    /* Set up the scheduler functions */
    funcs.u_get_type = ABTI_Unit_get_type;
    funcs.u_get_thread = ABTI_Unit_get_thread;
    funcs.u_get_task = ABTI_Unit_get_task;
    funcs.u_create_from_thread = ABTI_Unit_create_from_thread;
    funcs.u_create_from_task = ABTI_Unit_create_from_task;
    funcs.u_free = ABTI_Unit_free;
    funcs.p_get_size = ABTI_Pool_get_size;
    funcs.p_push   = ABTI_Pool_push;
    funcs.p_pop    = ABTI_Pool_pop;
    funcs.p_remove = ABTI_Pool_remove;

    /* Create a scheduler */
    abt_errno = ABT_Scheduler_create(pool, &funcs, &sched);
    if (abt_errno != ABT_SUCCESS) {
        HANDLE_ERROR("ABT_Scheduler_create");
        goto fn_fail;
    }

    /* Mark this as a runtime-provided scheduler */
    ABTI_Scheduler *p_sched = ABTI_Scheduler_get_ptr(sched);
    p_sched->type = ABTI_SCHEDULER_TYPE_BASE;

    /* Return value */
    *newsched = p_sched;

  fn_exit:
    return abt_errno;

  fn_fail:
    goto fn_exit;
}

