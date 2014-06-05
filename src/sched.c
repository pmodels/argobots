/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"

/** @defgroup SCHED Scheduler
 * This group is for Scheduler.
 */


/**
 * @ingroup SCHED
 * @brief   Create a new scheduler and return its handle through newsched.
 *
 * newsched must be used for a single stream because pool cannot be shared
 * between different streams.
 *
 * @param[in]  pool      a user-defined data structure containing work units
 *                       scheduled by a new scheduler
 * @param[in]  funcs     functions required for scheduler creation
 * @param[out] newsched  handle to a new scheduler
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_scheduler_create(ABT_pool pool,
                         const ABT_scheduler_funcs *funcs,
                         ABT_scheduler *newsched)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_scheduler *p_sched;

    if (newsched == NULL) {
        HANDLE_ERROR("newsched is NULL");
        abt_errno = ABT_ERR_SCHEDULER;
        goto fn_fail;
    }

    p_sched = (ABTI_scheduler *)ABTU_malloc(sizeof(ABTI_scheduler));
    if (!p_sched) {
        HANDLE_ERROR("ABTU_malloc");
        *newsched = ABT_SCHEDULER_NULL;
        abt_errno = ABT_ERR_MEM;
        goto fn_fail;
    }

    p_sched->type = ABTI_SCHEDULER_TYPE_USER;
    p_sched->pool = pool;

    /* Create a mutex */
    abt_errno = ABT_mutex_create(&p_sched->mutex);
    ABTI_CHECK_ERROR(abt_errno);

    /* Create a context */
    abt_errno = ABTD_thread_context_create(NULL, NULL, NULL,
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
    *newsched = ABTI_scheduler_get_handle(p_sched);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_scheduler_create", abt_errno);
    goto fn_exit;
}

/**
 * @ingroup SCHED
 * @brief   Release the scheduler object associated with sched handle.
 *
 * If this routine successfully returns, sched is set as ABT_SCHEDULER_NULL.
 *
 * @param[in,out] sched  handle to the target scheduler
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_scheduler_free(ABT_scheduler *sched)
{
    int abt_errno = ABT_SUCCESS;
    ABT_scheduler h_sched = *sched;
    ABTI_scheduler *p_sched = ABTI_scheduler_get_ptr(h_sched);
    if (p_sched == NULL) {
        HANDLE_ERROR("NULL SCHEDULER");
        abt_errno = ABT_ERR_INV_SCHEDULER;
        goto fn_fail;
    }

    /* If sched is a default provided one, it should free its pool here.
     * Otherwise, freeing the pool is the user's reponsibility. */
    if (p_sched->type == ABTI_SCHEDULER_TYPE_DEFAULT) {
        abt_errno = ABTI_pool_free(&p_sched->pool);
        ABTI_CHECK_ERROR(abt_errno);
    }

    /* Free the context */
    abt_errno = ABTD_thread_context_free(&p_sched->ctx);
    ABTI_CHECK_ERROR(abt_errno);

    /* Free the mutex */
    abt_errno = ABT_mutex_free(&p_sched->mutex);
    ABTI_CHECK_ERROR(abt_errno);

    ABTU_free(p_sched);

    /* Return value */
    *sched = ABT_SCHEDULER_NULL;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_scheduler_free", abt_errno);
    goto fn_exit;
}


/* Private APIs */
ABTI_scheduler *ABTI_scheduler_get_ptr(ABT_scheduler sched)
{
    ABTI_scheduler *p_sched;
    if (sched == ABT_SCHEDULER_NULL) {
        p_sched = NULL;
    } else {
        p_sched = (ABTI_scheduler *)sched;
    }
    return p_sched;
}

ABT_scheduler ABTI_scheduler_get_handle(ABTI_scheduler *p_sched)
{
    ABT_scheduler h_sched;
    if (p_sched == NULL) {
        h_sched = ABT_SCHEDULER_NULL;
    } else {
        h_sched = (ABT_scheduler)p_sched;
    }
    return h_sched;
}

int ABTI_scheduler_create_default(ABTI_scheduler **newsched)
{
    int abt_errno = ABT_SUCCESS;
    ABT_scheduler sched;
    ABT_pool pool;
    ABT_scheduler_funcs funcs;

    /* Create a work unit pool */
    abt_errno = ABTI_pool_create(&pool);
    ABTI_CHECK_ERROR(abt_errno);

    /* Set up the scheduler functions */
    funcs.u_get_type = ABTI_unit_get_type;
    funcs.u_get_thread = ABTI_unit_get_thread;
    funcs.u_get_task = ABTI_unit_get_task;
    funcs.u_create_from_thread = ABTI_unit_create_from_thread;
    funcs.u_create_from_task = ABTI_unit_create_from_task;
    funcs.u_free = ABTI_unit_free;
    funcs.p_get_size = ABTI_pool_get_size;
    funcs.p_push   = ABTI_pool_push;
    funcs.p_pop    = ABTI_pool_pop;
    funcs.p_remove = ABTI_pool_remove;

    /* Create a scheduler */
    abt_errno = ABT_scheduler_create(pool, &funcs, &sched);
    ABTI_CHECK_ERROR(abt_errno);

    /* Mark this as a runtime-provided scheduler */
    ABTI_scheduler *p_sched = ABTI_scheduler_get_ptr(sched);
    p_sched->type = ABTI_SCHEDULER_TYPE_DEFAULT;

    /* Return value */
    *newsched = p_sched;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABTI_scheduler_create_default", abt_errno);
    goto fn_exit;
}

void ABTI_scheduler_push(ABTI_scheduler *p_sched, ABT_unit unit)
{
    ABTI_mutex_waitlock(p_sched->mutex);
    p_sched->p_push(p_sched->pool, unit);
    ABT_mutex_unlock(p_sched->mutex);
}

ABT_unit ABTI_scheduler_pop(ABTI_scheduler *p_sched)
{
    ABTI_mutex_waitlock(p_sched->mutex);
    ABT_unit unit = p_sched->p_pop(p_sched->pool);
    ABT_mutex_unlock(p_sched->mutex);
    return unit;
}

void ABTI_scheduler_remove(ABTI_scheduler *p_sched, ABT_unit unit)
{
    ABTI_mutex_waitlock(p_sched->mutex);
    p_sched->p_remove(p_sched->pool, unit);
    ABT_mutex_unlock(p_sched->mutex);
}

int ABTI_scheduler_print(ABTI_scheduler *p_sched)
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
    abt_errno = ABTI_pool_print(p_sched->pool);
    ABTI_CHECK_ERROR(abt_errno);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABTI_scheduler_print", abt_errno);
    goto fn_exit;
}

