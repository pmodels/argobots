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

    if (newsched == NULL) {
        HANDLE_ERROR("newsched is NULL");
        abt_errno = ABT_ERR_SCHEDULER;
        goto fn_fail;
    }

    p_sched = (ABTI_Scheduler *)ABTU_Malloc(sizeof(ABTI_Scheduler));
    if (!p_sched) {
        HANDLE_ERROR("ABTU_Malloc");
        *newsched = ABT_SCHEDULER_NULL;
        abt_errno = ABT_ERR_MEM;
        goto fn_fail;
    }

    p_sched->type = ABTI_SCHEDULER_TYPE_USER;
    p_sched->pool = pool;

    /* Create a mutex */
    abt_errno = ABT_Mutex_create(&p_sched->mutex);
    ABTI_CHECK_ERROR(abt_errno);

    /* Create a context */
    abt_errno = ABTD_Thread_context_create(NULL, NULL, NULL,
            0, NULL, &p_sched->ctx);
    ABTI_CHECK_ERROR(abt_errno);

    /* Scheduling functions */
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

    /* Return value */
    *newsched = ABTI_Scheduler_get_handle(p_sched);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_Scheduler_create", abt_errno);
    goto fn_exit;
}

int ABT_Scheduler_free(ABT_Scheduler *sched)
{
    int abt_errno = ABT_SUCCESS;
    ABT_Scheduler h_sched = *sched;
    ABTI_Scheduler *p_sched = ABTI_Scheduler_get_ptr(h_sched);
    if (p_sched == NULL) {
        HANDLE_ERROR("NULL SCHEDULER");
        abt_errno = ABT_ERR_INV_SCHEDULER;
        goto fn_fail;
    }

    /* If sched is a default provided one, it should free its pool here.
     * Otherwise, freeing the pool is the user's reponsibility. */
    if (p_sched->type == ABTI_SCHEDULER_TYPE_DEFAULT) {
        abt_errno = ABTI_Pool_free(&p_sched->pool);
        ABTI_CHECK_ERROR(abt_errno);
    }

    /* Free the context */
    abt_errno = ABTD_Thread_context_free(&p_sched->ctx);
    ABTI_CHECK_ERROR(abt_errno);

    /* Free the mutex */
    abt_errno = ABT_Mutex_free(&p_sched->mutex);
    ABTI_CHECK_ERROR(abt_errno);

    ABTU_Free(p_sched);

    /* Return value */
    *sched = ABT_SCHEDULER_NULL;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_Scheduler_free", abt_errno);
    goto fn_exit;
}


/* Private APIs */
ABTI_Scheduler *ABTI_Scheduler_get_ptr(ABT_Scheduler sched)
{
    ABTI_Scheduler *p_sched;
    if (sched == ABT_SCHEDULER_NULL) {
        p_sched = NULL;
    } else {
        p_sched = (ABTI_Scheduler *)sched;
    }
    return p_sched;
}

ABT_Scheduler ABTI_Scheduler_get_handle(ABTI_Scheduler *p_sched)
{
    ABT_Scheduler h_sched;
    if (p_sched == NULL) {
        h_sched = ABT_SCHEDULER_NULL;
    } else {
        h_sched = (ABT_Scheduler)p_sched;
    }
    return h_sched;
}

int ABTI_Scheduler_create_default(ABTI_Scheduler **newsched)
{
    int abt_errno = ABT_SUCCESS;
    ABT_Scheduler sched;
    ABT_Pool pool;
    ABT_Scheduler_funcs funcs;

    /* Create a work unit pool */
    abt_errno = ABTI_Pool_create(&pool);
    ABTI_CHECK_ERROR(abt_errno);

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
    ABTI_CHECK_ERROR(abt_errno);

    /* Mark this as a runtime-provided scheduler */
    ABTI_Scheduler *p_sched = ABTI_Scheduler_get_ptr(sched);
    p_sched->type = ABTI_SCHEDULER_TYPE_DEFAULT;

    /* Return value */
    *newsched = p_sched;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABTI_Scheduler_create_default", abt_errno);
    goto fn_exit;
}

void ABTI_Scheduler_push(ABTI_Scheduler *p_sched, ABT_Unit unit)
{
    ABTI_Mutex_waitlock(p_sched->mutex);
    p_sched->p_push(p_sched->pool, unit);
    ABT_Mutex_unlock(p_sched->mutex);
}

ABT_Unit ABTI_Scheduler_pop(ABTI_Scheduler *p_sched)
{
    ABTI_Mutex_waitlock(p_sched->mutex);
    ABT_Unit unit = p_sched->p_pop(p_sched->pool);
    ABT_Mutex_unlock(p_sched->mutex);
    return unit;
}

void ABTI_Scheduler_remove(ABTI_Scheduler *p_sched, ABT_Unit unit)
{
    ABTI_Mutex_waitlock(p_sched->mutex);
    p_sched->p_remove(p_sched->pool, unit);
    ABT_Mutex_unlock(p_sched->mutex);
}

int ABTI_Scheduler_print(ABTI_Scheduler *p_sched)
{
    int abt_errno = ABT_SUCCESS;
    if (p_sched == NULL) {
        printf("NULL SCHEDULER\n");
        goto fn_exit;
    }

    printf("== SCHEDULER (%p) ==\n", p_sched);
    printf("type: ");
    switch (p_sched->type) {
        case ABTI_SCHEDULER_TYPE_DEFAULT: printf("DEFAULT\n"); break;
        case ABTI_SCHEDULER_TYPE_USER:    printf("USER\n"); break;
        default: printf("UNKNOWN\n"); break;
    }
    printf("pool: ");
    abt_errno = ABTI_Pool_print(p_sched->pool);
    ABTI_CHECK_ERROR(abt_errno);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABTI_Scheduler_print", abt_errno);
    goto fn_exit;
}

