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
    ABTD_Scheduler *sched_ptr;

    sched_ptr = (ABTD_Scheduler *)ABTU_Malloc(sizeof(ABTD_Scheduler));
    if (!sched_ptr) {
        HANDLE_ERROR("ABTU_Malloc");
        *newsched = NULL;
        abt_errno = ABT_ERR_MEM;
        goto fn_fail;
    }

    sched_ptr->type = ABT_SCHEDULER_TYPE_USER;
    sched_ptr->pool = pool;

    sched_ptr->u_get_type = funcs->u_get_type;
    sched_ptr->u_get_thread = funcs->u_get_thread;
    sched_ptr->u_get_task = funcs->u_get_task;
    sched_ptr->u_create_from_thread = funcs->u_create_from_thread;
    sched_ptr->u_create_from_task = funcs->u_create_from_task;
    sched_ptr->u_free = funcs->u_free;

    sched_ptr->p_get_size = funcs->p_get_size;
    sched_ptr->p_push   = funcs->p_push;
    sched_ptr->p_pop    = funcs->p_pop;
    sched_ptr->p_remove = funcs->p_remove;

    *newsched = ABTI_Scheduler_get_handle(sched_ptr);

  fn_exit:
    return abt_errno;

  fn_fail:
    goto fn_exit;
}

int ABT_Scheduler_free(ABT_Scheduler sched)
{
    int abt_errno = ABT_SUCCESS;
    ABTD_Scheduler *sched_ptr = ABTI_Scheduler_get_ptr(sched);

    /* If sched is a runtime-provided one, it should free its pool here.
     * Otherwise, freeing the pool is the user's reponsibility. */
    if (sched_ptr->type == ABT_SCHEDULER_TYPE_BASE) {
        ABTD_Pool *pool = ABTI_Pool_get_ptr(sched_ptr->pool);
        abt_errno = ABTI_Pool_free(pool);
        if (abt_errno != ABT_SUCCESS) {
            HANDLE_ERROR("ABTI_Pool_free");
            goto fn_fail;
        }
    }

    free(sched_ptr);

  fn_exit:
    return abt_errno;

  fn_fail:
    goto fn_exit;
}


/* Internal functions */
int ABTI_Scheduler_create_default(ABTD_Scheduler **newsched)
{
    int abt_errno = ABT_SUCCESS;
    ABT_Scheduler sched;
    ABT_Pool pool;
    ABT_Scheduler_funcs funcs;

    /* Create a work unit pool */
    ABTD_Pool *pool_ptr;
    abt_errno = ABTI_Pool_create(&pool_ptr);
    if (abt_errno != ABT_SUCCESS) {
        HANDLE_ERROR("ABTI_Pool_create");
        goto fn_fail;
    }
    pool = ABTI_Pool_get_handle(pool_ptr);

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
    ABTD_Scheduler *sched_ptr = ABTI_Scheduler_get_ptr(sched);
    sched_ptr->type = ABT_SCHEDULER_TYPE_BASE;

    /* Return value */
    *newsched = sched_ptr;

  fn_exit:
    return abt_errno;

  fn_fail:
    goto fn_exit;
}

