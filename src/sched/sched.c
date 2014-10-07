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
 * newsched must be used for a single ES because pool cannot be shared
 * between different Execution Streams.
 *
 * @param[in]  pool      a user-defined data structure containing work units
 *                       scheduled by a new scheduler
 * @param[in]  funcs     functions required for scheduler creation
 * @param[out] newsched  handle to a new scheduler
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_sched_create(ABT_pool pool, const ABT_sched_funcs *funcs,
                     ABT_sched *newsched)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_sched *p_sched;

    if (newsched == NULL) {
        HANDLE_ERROR("newsched is NULL");
        abt_errno = ABT_ERR_SCHED;
        goto fn_fail;
    }

    p_sched = (ABTI_sched *)ABTU_malloc(sizeof(ABTI_sched));
    if (!p_sched) {
        HANDLE_ERROR("ABTU_malloc");
        *newsched = ABT_SCHED_NULL;
        abt_errno = ABT_ERR_MEM;
        goto fn_fail;
    }

    p_sched->p_xstream = NULL;
    p_sched->type = ABTI_SCHED_TYPE_USER;
    p_sched->pool = pool;
    p_sched->num_threads = 0;
    p_sched->num_tasks   = 0;
    p_sched->num_blocked = 0;

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
    *newsched = ABTI_sched_get_handle(p_sched);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_sched_create", abt_errno);
    goto fn_exit;
}

/**
 * @ingroup SCHED
 * @brief   Create a new scheduler with a provided scheduling policy.
 *
 * newsched must be used for a single stream because pool cannot be shared
 * between different streams.
 *
 * @param[in]  kind      scheduling policy provided by the Argobots library
 * @param[out] newsched  handle to a new scheduler
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_sched_create_basic(ABT_sched_kind kind, ABT_sched *newsched)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_sched *p_newsched;

    switch (kind) {
        case ABT_SCHED_FIFO:
            abt_errno = ABTI_sched_create_fifo(&p_newsched);
            break;
        case ABT_SCHED_LIFO:
            abt_errno = ABTI_sched_create_lifo(&p_newsched);
            break;
        case ABT_SCHED_PRIO:
            abt_errno = ABTI_sched_create_prio(&p_newsched);
            break;
        default:
            abt_errno = ABT_ERR_INV_SCHED_KIND;
            break;
    }
    if (abt_errno != ABT_SUCCESS) {
        *newsched = ABT_SCHED_NULL;
        goto fn_fail;
    }

    /* Return value */
    *newsched = ABTI_sched_get_handle(p_newsched);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_sched_create_basic", abt_errno);
    goto fn_exit;
}

/**
 * @ingroup SCHED
 * @brief   Release the scheduler object associated with sched handle.
 *
 * If this routine successfully returns, sched is set as ABT_SCHED_NULL.
 *
 * @param[in,out] sched  handle to the target scheduler
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_sched_free(ABT_sched *sched)
{
    int abt_errno = ABT_SUCCESS;
    ABT_sched h_sched = *sched;
    ABTI_sched *p_sched = ABTI_sched_get_ptr(h_sched);
    ABTI_CHECK_NULL_SCHED_PTR(p_sched);

    /* Disconnect this scheduler from ES */
    p_sched->p_xstream->p_sched = NULL;

    /* If sched is a default provided one, it should free its pool here.
     * Otherwise, freeing the pool is the user's reponsibility. */
    if (p_sched->type == ABTI_SCHED_TYPE_DEFAULT) {
        abt_errno = ABTI_pool_free(&p_sched->pool);
        ABTI_CHECK_ERROR(abt_errno);
    } else if (p_sched->type == ABTI_SCHED_TYPE_BASIC) {
        abt_errno = ABTI_sched_free_basic(p_sched);
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
    *sched = ABT_SCHED_NULL;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_sched_free", abt_errno);
    goto fn_exit;
}

/**
 * @ingroup SCHED
 * @brief   Get the minimum priority of a scheduler kind.
 *
 * For ABT_SCHED_PRIO, the range is defined in enum ABT_sched_prio. For
 * ABT_SCHED_FIFO and ABT_SCHED_LIFO, both ABT_sched_get_prio_min and
 * ABT_sched_get_prio_max will set prio to ABT_SCHED_PRIO_NORMAL.
 *
 * @param[in]  kind  scheduling policy provided by the Argobots library
 * @param[out] prio  the minimum priority value that can be used with
 *                   the scheduling algorithm
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_sched_get_prio_min(ABT_sched_kind kind, ABT_sched_prio *prio)
{
    int abt_errno = ABT_SUCCESS;

    switch (kind) {
        case ABT_SCHED_FIFO:
        case ABT_SCHED_LIFO:
            *prio = ABT_SCHED_PRIO_NORMAL;
            break;
        case ABT_SCHED_PRIO:
            *prio = ABT_SCHED_PRIO_LOW;
            break;
        default:
            abt_errno = ABT_ERR_INV_SCHED_KIND;
            break;
    }
    ABTI_CHECK_ERROR(abt_errno);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_sched_get_prio_min", abt_errno);
    goto fn_exit;
}

/**
 * @ingroup SCHED
 * @brief   Get the maximum priority of a scheduler kind.
 *
 * For ABT_SCHED_PRIO, the range is defined in enum ABT_sched_prio. For
 * ABT_SCHED_FIFO and ABT_SCHED_LIFO, both ABT_sched_get_prio_min and
 * ABT_sched_get_prio_max will set prio to ABT_SCHED_PRIO_NORMAL.
 *
 * @param[in]  kind  scheduling policy provided by the Argobots library
 * @param[out] prio  the maximum priority value that can be used with
 *                   the scheduling algorithm
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_sched_get_prio_max(ABT_sched_kind kind, ABT_sched_prio *prio)
{
    int abt_errno = ABT_SUCCESS;

    switch (kind) {
        case ABT_SCHED_FIFO:
        case ABT_SCHED_LIFO:
            *prio = ABT_SCHED_PRIO_NORMAL;
            break;
        case ABT_SCHED_PRIO:
            *prio = ABT_SCHED_PRIO_HIGH;
            break;
        default:
            abt_errno = ABT_ERR_INV_SCHED_KIND;
            break;
    }
    ABTI_CHECK_ERROR(abt_errno);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_sched_get_prio_max", abt_errno);
    goto fn_exit;
}


/*****************************************************************************/
/* Private APIs                                                              */
/*****************************************************************************/

ABTI_sched *ABTI_sched_get_ptr(ABT_sched sched)
{
    ABTI_sched *p_sched;
    if (sched == ABT_SCHED_NULL) {
        p_sched = NULL;
    } else {
        p_sched = (ABTI_sched *)sched;
    }
    return p_sched;
}

ABT_sched ABTI_sched_get_handle(ABTI_sched *p_sched)
{
    ABT_sched h_sched;
    if (p_sched == NULL) {
        h_sched = ABT_SCHED_NULL;
    } else {
        h_sched = (ABT_sched)p_sched;
    }
    return h_sched;
}

int ABTI_sched_create_default(ABTI_sched **newsched)
{
    int abt_errno = ABT_SUCCESS;
    ABT_sched sched;
    ABT_pool pool;
    ABT_sched_funcs funcs;

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
    abt_errno = ABT_sched_create(pool, &funcs, &sched);
    ABTI_CHECK_ERROR(abt_errno);

    /* Mark this as a runtime-provided scheduler */
    ABTI_sched *p_sched = ABTI_sched_get_ptr(sched);
    p_sched->type = ABTI_SCHED_TYPE_DEFAULT;

    /* Return value */
    *newsched = p_sched;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABTI_sched_create_default", abt_errno);
    goto fn_exit;
}

int ABTI_sched_free_basic(ABTI_sched *p_sched)
{
    int abt_errno = ABT_SUCCESS;
    switch (p_sched->kind) {
        case ABT_SCHED_FIFO:
            abt_errno = ABTI_sched_free_fifo(p_sched);
            break;
        case ABT_SCHED_LIFO:
            abt_errno = ABTI_sched_free_lifo(p_sched);
            break;
        case ABT_SCHED_PRIO:
            abt_errno = ABTI_sched_free_prio(p_sched);
            break;
        default:
            abt_errno = ABT_ERR_INV_SCHED;
            break;
    }
    ABTI_CHECK_ERROR(abt_errno);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABTI_sched_free_basic", abt_errno);
    goto fn_exit;
}

void ABTI_sched_push(ABTI_sched *p_sched, ABT_unit unit)
{
    ABT_unit_type type = ABTI_unit_get_type(unit);

    ABTI_mutex_waitlock(p_sched->mutex);
    switch (type) {
        case ABT_UNIT_TYPE_THREAD: p_sched->num_threads++; break;
        case ABT_UNIT_TYPE_TASK:   p_sched->num_tasks++;   break;
        default: break;
    }
    p_sched->p_push(p_sched->pool, unit);
    ABT_mutex_unlock(p_sched->mutex);
}

ABT_unit ABTI_sched_pop(ABTI_sched *p_sched)
{
    ABT_unit unit;
    ABT_unit_type type;

    ABTI_mutex_waitlock(p_sched->mutex);
    unit = p_sched->p_pop(p_sched->pool);
    type = ABTI_unit_get_type(unit);
    switch (type) {
        case ABT_UNIT_TYPE_THREAD: p_sched->num_threads--; break;
        case ABT_UNIT_TYPE_TASK:   p_sched->num_tasks--;   break;
        default: break;
    }
    ABT_mutex_unlock(p_sched->mutex);
    return unit;
}

void ABTI_sched_remove(ABTI_sched *p_sched, ABT_unit unit)
{
    ABTI_mutex_waitlock(p_sched->mutex);
    p_sched->p_remove(p_sched->pool, unit);
    ABT_mutex_unlock(p_sched->mutex);
}

int ABTI_sched_inc_num_blocked(ABTI_sched *p_sched)
{
    ABTD_atomic_fetch_add_uint32(&p_sched->num_blocked, 1);
    return ABT_SUCCESS;
}

int ABTI_sched_dec_num_blocked(ABTI_sched *p_sched)
{
    ABTD_atomic_fetch_sub_uint32(&p_sched->num_blocked, 1);
    return ABT_SUCCESS;
}

int ABTI_sched_print(ABTI_sched *p_sched)
{
    int abt_errno = ABT_SUCCESS;
    if (p_sched == NULL) {
        printf("NULL SCHEDULER\n");
        goto fn_exit;
    }

    printf("== SCHEDULER (%p) ==\n", p_sched);
    printf("xstream: %p\n", p_sched->p_xstream);
    printf("type: ");
    switch (p_sched->type) {
        case ABTI_SCHED_TYPE_DEFAULT: printf("DEFAULT\n"); break;
        case ABTI_SCHED_TYPE_BASIC:   printf("BASIC\n"); break;
        case ABTI_SCHED_TYPE_USER:    printf("USER\n"); break;
        default: printf("UNKNOWN\n"); break;
    }
    if (p_sched->type == ABTI_SCHED_TYPE_BASIC) {
        printf("kind: ");
        switch (p_sched->kind) {
            case ABT_SCHED_FIFO: printf("FIFO\n"); break;
            case ABT_SCHED_LIFO: printf("LIFO\n"); break;
            case ABT_SCHED_PRIO: printf("PRIO\n"); break;
            default: printf("UNKNOWN\n"); break;
        }
    }
    printf("pool: ");
    abt_errno = ABTI_pool_print(p_sched->pool);
    ABTI_CHECK_ERROR(abt_errno);
    printf("num_threads: %u\n", p_sched->num_threads);
    printf("num_tasks: %u\n", p_sched->num_tasks);
    printf("num_blocked: %u\n", p_sched->num_blocked);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABTI_sched_print", abt_errno);
    goto fn_exit;
}

