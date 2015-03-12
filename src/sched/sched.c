/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"


/** @defgroup SCHED Scheduler
 * This group is for Scheduler.
 */

/* Predefined schedulers */
extern ABT_sched_def ABTI_sched_basic;

/**
 * @ingroup SCHED
 * @brief   Create a new user-defined scheduler and return its handle through
 * newsched.
 *
 * The pools used by the new scheduler are provided by pools. This is an array
 * of pools whose the address will be copied. So it must not be freed. The
 * config has been created by a specific function of the user-defined
 * scheduler, and will be used has argument in the initialization.
 *
 * @param[in]  def       definition required for scheduler creation
 * @param[in]  num_pools number of pools associated with this scheduler
 * @param[in]  pools     pools associated with this scheduler
 * @param[in]  config    specific config used during the scheduler creation
 * @param[out] newsched handle to a new scheduler
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_sched_create(ABT_sched_def *def, int num_pools, ABT_pool *pools,
                     ABT_sched_config config, ABT_sched *newsched)
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

    /* Check if the pools are available */
    int p;
    for (p = 0; p < num_pools; p++) {
      ABTI_pool_retain(pools[p]);
    }

    /* Create a mutex */
    abt_errno = ABT_mutex_create(&p_sched->mutex);
    ABTI_CHECK_ERROR(abt_errno);

    p_sched->used          = ABTI_SCHED_NOT_USED;
    p_sched->automatic     = ABT_FALSE;
    p_sched->kind          = ABTI_sched_get_kind(def);
    p_sched->state         = ABT_SCHED_STATE_READY;
    p_sched->request       = 0;
    p_sched->pools         = pools;
    p_sched->free_pools    = ABT_FALSE;
    p_sched->num_pools     = num_pools;
    p_sched->type          = def->type;
    p_sched->thread        = ABT_THREAD_NULL;
    p_sched->task          = ABT_TASK_NULL;

    p_sched->init          = def->init;
    p_sched->run           = def->run;
    p_sched->free          = def->free;
    p_sched->get_migr_pool = def->get_migr_pool;

    /* Return value */
    *newsched = ABTI_sched_get_handle(p_sched);

    /* Specific initialization */
    p_sched->init(*newsched, config);

  fn_exit:
    return abt_errno;

  fn_fail:
    *newsched = ABT_SCHED_NULL;
    HANDLE_ERROR_WITH_CODE("ABT_sched_create", abt_errno);
    goto fn_exit;
}

/**
 * @ingroup SCHED
 * @brief   Create a predefined scheduler and return its handle through
 * newsched.
 *
 * The pools used by the new scheduler are provided by pools. This is an array
 * of pools whose the address will be copied. So it must not be freed.  The
 * pool array can be NULL, but predef of sceduler should be compatible.
 *
 * @param[in]  predef    predefined scheduler
 * @param[in]  num_pools number of pools associated with this scheduler
 * @param[in]  pools     pools associated with this scheduler
 * @param[in]  automatic teel if the sched should be automatically freed
 * @param[out] newsched  handle to a new scheduler
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_sched_create_basic(ABT_sched_predef predef, int num_pools,
                           ABT_pool *p_pools, int automatic,
                           ABT_sched *newsched)
{
    int abt_errno = ABT_SUCCESS;
    int free_pools = ABT_FALSE;

    // A pool array is provided, predef has to be compatible
    if (p_pools != NULL) {
        /* Creation of the scheduler */
        switch (predef) {
            case ABT_SCHED_DEFAULT_NO_POOL:
            case ABT_SCHED_BASIC_NO_POOL:
                abt_errno = ABT_sched_create(&ABTI_sched_basic,
                                             num_pools, p_pools,
                                             ABT_SCHED_CONFIG_NULL,
                                             newsched);
                break;
            default:
                abt_errno = ABT_ERR_INV_SCHED_PREDEF;
                break;
        }
        ABTI_CHECK_ERROR(abt_errno);
    }

    // No pool array is provided, predef has to be compatible
    else {
        free_pools = ABT_TRUE;
        ABT_pool_access access;
        /* First, set the number of pools and the access type */
        switch (predef) {
            case ABT_SCHED_DEFAULT_POOL_FIFO_PRW:
            case ABT_SCHED_BASIC_POOL_FIFO_PRW:
                access = ABT_POOL_ACCESS_PRW;
                num_pools = 1;
                break;
            case ABT_SCHED_DEFAULT_POOL_FIFO_PR_PW:
            case ABT_SCHED_BASIC_POOL_FIFO_PR_PW:
                access = ABT_POOL_ACCESS_PR_PW;
                num_pools = 1;
                break;
            case ABT_SCHED_DEFAULT_POOL_FIFO_PR_SW:
            case ABT_SCHED_BASIC_POOL_FIFO_PR_SW:
                access = ABT_POOL_ACCESS_PR_SW;
                num_pools = 1;
                break;
            case ABT_SCHED_DEFAULT_POOL_FIFO_SR_PW:
            case ABT_SCHED_BASIC_POOL_FIFO_SR_PW:
                access = ABT_POOL_ACCESS_SR_PW;
                num_pools = 1;
                break;
            case ABT_SCHED_DEFAULT_POOL_FIFO_SR_SW:
            case ABT_SCHED_BASIC_POOL_FIFO_SR_SW:
                access = ABT_POOL_ACCESS_SR_SW;
                num_pools = 1;
                break;
            default:
                abt_errno = ABT_ERR_INV_SCHED_PREDEF;
                ABTI_CHECK_ERROR(abt_errno);
                break;
        }

        /* Creation of the pools */
        p_pools = (ABT_pool *)malloc(num_pools*sizeof(ABT_pool));
        int p;
        for (p = 0; p < num_pools; p++) {
            abt_errno = ABT_pool_create_basic(ABT_POOL_FIFO,
                                              access,
                                              p_pools+p);
            ABTI_CHECK_ERROR(abt_errno);
        }

        /* Creation of the scheduler */
        switch (predef) {
            case ABT_SCHED_DEFAULT_POOL_FIFO_PRW:
            case ABT_SCHED_DEFAULT_POOL_FIFO_PR_PW:
            case ABT_SCHED_DEFAULT_POOL_FIFO_PR_SW:
            case ABT_SCHED_DEFAULT_POOL_FIFO_SR_PW:
            case ABT_SCHED_DEFAULT_POOL_FIFO_SR_SW:
            case ABT_SCHED_BASIC_POOL_FIFO_PRW:
            case ABT_SCHED_BASIC_POOL_FIFO_PR_PW:
            case ABT_SCHED_BASIC_POOL_FIFO_PR_SW:
            case ABT_SCHED_BASIC_POOL_FIFO_SR_PW:
            case ABT_SCHED_BASIC_POOL_FIFO_SR_SW:
                abt_errno = ABT_sched_create(&ABTI_sched_basic,
                                             num_pools, p_pools,
                                             ABT_SCHED_CONFIG_NULL,
                                             newsched);
                break;
            default:
                abt_errno = ABT_ERR_INV_SCHED_PREDEF;
                ABTI_CHECK_ERROR(abt_errno);
                break;
        }
    }
    ABTI_CHECK_ERROR(abt_errno);

    ABTI_sched *p_sched = ABTI_sched_get_ptr(*newsched);
    if (automatic)
        p_sched->automatic = ABT_TRUE;
    if (free_pools == ABT_TRUE)
        p_sched->free_pools = ABT_TRUE;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_sched_create_basic", abt_errno);
    *newsched = ABT_SCHED_NULL;
    goto fn_exit;
}

/**
 * @ingroup SCHED
 * @brief   Release the scheduler object associated with sched handle.
 *
 * If this routine successfully returns, sched is set as ABT_SCHED_NULL. The
 * scheduler will be automatically freed.
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

    /* If sched is a default provided one, it should free its pool here.
     * Otherwise, freeing the pool is the user's reponsibility. */
    int p;
    for (p = 0; p < p_sched->num_pools; p++) {
        ABTI_pool *p_pool = ABTI_pool_get_ptr(p_sched->pools[p]);
        ABTI_pool_release(p_pool);
        if (p_pool->automatic == ABT_TRUE && p_pool->num_scheds == 0) {
            abt_errno = ABT_pool_free(p_sched->pools+p);
            ABTI_CHECK_ERROR(abt_errno);
        }
    }
    if (p_sched->free_pools == ABT_TRUE)
        ABTU_free(p_sched->pools);

    /* Free the associated thread */
    if (p_sched->thread != ABT_THREAD_NULL) {
        ABTI_thread *p_thread = ABTI_thread_get_ptr(p_sched->thread);
        if (p_thread->type != ABTI_THREAD_TYPE_MAIN_SCHED)
            ABT_thread_free(&p_sched->thread);
    }

    /* Free the mutex */
    abt_errno = ABT_mutex_free(&p_sched->mutex);
    ABTI_CHECK_ERROR(abt_errno);

    p_sched->free(h_sched);
    p_sched->data = NULL;

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
 * @brief   Get the pools of the scheduler \c sched.
 *
 * @param[in]  sched     handle to the target scheduler
 * @param[in]  max_pools maximum number of pools to get
 * @param[in]  idx       index of the first pool to get
 * @param[out] pools     array of handles to the pools
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_sched_get_pools(ABT_sched sched, int max_pools, int idx,
                        ABT_pool *pools)
{
    int abt_errno = ABT_SUCCESS;

    ABTI_sched *p_sched = ABTI_sched_get_ptr(sched);
    ABTI_CHECK_NULL_SCHED_PTR(p_sched);

    if (idx+max_pools > p_sched->num_pools) {
        abt_errno = ABT_ERR_SCHED;
        goto fn_fail;
    }

    int p;
    for (p = idx; p < idx+max_pools; p++) {
        pools[p-idx] = p_sched->pools[p];
    }

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_sched_get_pools", abt_errno);
    goto fn_exit;
}

/**
 * @ingroup SCHED
 * @brief   Ask a scheduler to finish
 *
 * The scheduler will stop when its pools will be empty.
 *
 * @param[in] sched  handle to the target scheduler
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_sched_finish(ABT_sched sched)
{
    int abt_errno = ABT_SUCCESS;

    ABTI_sched *p_sched = ABTI_sched_get_ptr(sched);
    ABTI_CHECK_NULL_SCHED_PTR(p_sched);

    ABTD_atomic_fetch_or_uint32(&p_sched->request, ABTI_SCHED_REQ_FINISH);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_sched_finish", abt_errno);
    goto fn_exit;
}

/**
 * @ingroup SCHED
 * @brief   Ask a scheduler to stop as soon as possible
 *
 * The scheduler will stop even if its pools are not empty. It is the user's
 * responsibility to ensure that the left work will be done by another scheduler.
 *
 * @param[in] sched  handle to the target scheduler
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_sched_exit(ABT_sched sched)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_sched *p_sched = ABTI_sched_get_ptr(sched);
    ABTI_CHECK_NULL_SCHED_PTR(p_sched);

    ABTD_atomic_fetch_or_uint32(&p_sched->request, ABTI_SCHED_REQ_EXIT);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("XXX", abt_errno);
    goto fn_exit;
}

/**
 * @ingroup SCHED
 * @brief   Check if the scheduler needs to stop
 *
 * Check if there has been an exit or a finish request and if the conditions
 * are respected (empty pool for a finish request).
 * If we are on the primary ES, we will jump back to the main ULT,
 * if the scheduler has nothing to do.
 *
 * It is the user's responsibility to take proper measures to stop the
 * scheduling loop, depending on the value given by stop.
 *
 * @param[in]  sched handle to the target scheduler
 * @param[out] stop  indicate if the scheduler has to stop
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_sched_has_to_stop(ABT_sched sched, int *stop)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_xstream *p_xstream = ABTI_local_get_xstream();

    *stop = ABT_FALSE;

    ABTI_sched *p_sched = ABTI_sched_get_ptr(sched);
    ABTI_CHECK_NULL_SCHED_PTR(p_sched);

    /* Check exit request */
    if (p_sched->request & ABTI_SCHED_REQ_EXIT) {
        ABT_mutex_waitlock(p_xstream->top_sched_mutex);
        p_sched->state = ABT_SCHED_STATE_TERMINATED;
        *stop = ABT_TRUE;
        goto fn_exit;
    }

    /* Check join request */
    size_t size;
    ABT_sched_get_size(p_sched, &size);
    if (size == 0) {
        ABTI_thread *p_main_thread = ABTI_local_get_main();
        if (p_sched->request & ABTI_SCHED_REQ_FINISH) {
            /* We need to lock in case someone wants to migrate to this
             * scheduler */
            ABT_mutex_waitlock(p_xstream->top_sched_mutex);
            size_t size;
            ABT_sched_get_size(p_sched, &size);
            if (size == 0) {
                p_sched->state = ABT_SCHED_STATE_TERMINATED;
                *stop = ABT_TRUE;
            }
            else
                ABT_mutex_unlock(p_xstream->top_sched_mutex);
        }
        /* We jump back to the main ULT if there is */
        else if (p_main_thread != NULL) {
            ABTI_thread *p_thread = ABTI_thread_get_ptr(p_sched->thread);
            if (ABTI_task_current() == NULL)
              assert(p_thread == ABTI_thread_current());
            abt_errno = ABTD_thread_context_switch(&p_thread->ctx,
                                                   &p_main_thread->ctx);
            ABTI_CHECK_ERROR(abt_errno);
            ABTI_local_set_thread(p_thread);
        }
    }

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("XXX", abt_errno);
    goto fn_exit;
}

/**
 * @ingroup SCHED
 * @brief   Set the specific data of the target user-defined scheduler
 *
 * This function will be called by the user during the initialization of his
 * user-defined scheduler.
 *
 * @param[in] sched handle to the scheduler
 * @param[in] data specific data of the scheduler
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_sched_set_data(ABT_sched sched, void *data)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_sched *p_sched = ABTI_sched_get_ptr(sched);
    ABTI_CHECK_NULL_SCHED_PTR(p_sched);
    p_sched->data = data;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_sched_set_data", abt_errno);
    goto fn_exit;
}

/**
 * @ingroup SCHED
 * @brief   Retrieve the specific data of the target user-defnied scheduler
 *
 * This function will be called by the user in a user-defined function of his
 * user-defnied scheduler.
 *
 * @param[in]  sched  handle to the scheduler
 * @param[out] p_data pointer to the specific data of the scheduler
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_sched_get_data(ABT_sched sched, void **p_data)
{
    int abt_errno = ABT_SUCCESS;

    ABTI_sched *p_sched = ABTI_sched_get_ptr(sched);
    ABTI_CHECK_NULL_SCHED_PTR(p_sched);

    *p_data = p_sched->data;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_sched_get_data", abt_errno);
    goto fn_exit;
}

/**
 * @ingroup SCHED
 * @brief   Get the sum of the sizes of the pool of \c sched.
 *
 * The size includes the blocked and migrating ULT.
 *
 * @param[in]  sched handle to the scheduler
 * @param[out] size  total number of work units
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_sched_get_size(ABT_sched sched, size_t *p_size)
{
    int abt_errno = ABT_SUCCESS;
    size_t size = 0;
    int p;

    ABTI_sched *p_sched = ABTI_sched_get_ptr(sched);
    ABTI_CHECK_NULL_SCHED_PTR(p_sched);
    for (p = 0; p < p_sched->num_pools; p++) {
        size_t s;
        ABT_pool pool = p_sched->pools[p];
        ABT_pool_get_total_size(pool, &s);
        size += s;
    }

  fn_exit:
    *p_size = size;
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_sched_get_size", abt_errno);
    goto fn_exit;
}


/*****************************************************************************/
/* Private APIs                                                              */
/*****************************************************************************/

/* Mark the scheduler as used and how it is used */
int ABTI_sched_associate(ABTI_sched *p_sched, ABTI_sched_used use)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_CHECK_NULL_SCHED_PTR(p_sched);
    if (p_sched->used != ABTI_SCHED_NOT_USED)
        abt_errno = ABT_ERR_SCHED;

    p_sched->used = use;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABTI_sched_associate", abt_errno);
    goto fn_exit;
}

/* Get the pool suitable for receiving a migrating ULT */
int ABTI_sched_get_migration_pool(ABTI_sched *p_sched, ABTI_pool *source_pool,
                                  ABTI_pool **pp_pool)
{
    int abt_errno = ABT_SUCCESS;
    ABT_sched sched = ABTI_sched_get_handle(p_sched);
    ABTI_pool *p_pool;

    if (p_sched->state == ABT_SCHED_STATE_TERMINATED) {
        abt_errno = ABT_ERR_INV_SCHED;
        *pp_pool = NULL;
        goto fn_fail;
    }

    /* Find a pool */
    /* If get_migr_pool is not defined, we pick the first pool */
    if (p_sched->get_migr_pool == NULL) {
        if (p_sched->num_pools == 0)
            p_pool = NULL;
        else
            p_pool = p_sched->pools[0];
    }
    else
        p_pool = p_sched->get_migr_pool(sched);

    /* Check the pool */
    if (ABTI_pool_accept_migration(p_pool, source_pool) == ABT_TRUE) {
        *pp_pool = p_pool;
    }
    else {
        *pp_pool = NULL;
        abt_errno = ABT_ERR_INV_POOL_ACCESS;
    }

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABTI_sched_get_migration_pool", abt_errno);
    goto fn_exit;
}

uint64_t ABTI_sched_get_kind(ABT_sched_def *def)
{
  return (uint64_t)def;
}

int ABTI_sched_print(ABTI_sched *p_sched)
{
    int abt_errno = ABT_SUCCESS;
    int p;

    if (p_sched == NULL) {
        printf("NULL SCHEDULER\n");
        goto fn_exit;
    }

    printf("== SCHEDULER (%p) ==\n", p_sched);
    printf("id: ");
    if (p_sched->kind == ABTI_sched_get_kind(&ABTI_sched_basic)) {
        printf("BASIC\n");
    } else {
        printf("%" PRIu64 " (USER)\n", p_sched->kind);
    }

    printf("automatic: %d", p_sched->automatic);
    printf("number of pools: %d", p_sched->num_pools);
    for (p = 0; p < p_sched->num_pools; p++) {
        printf("pool %d: ", p);
        ABTI_pool *p_pool = ABTI_pool_get_ptr(p_sched->pools[p]);
        abt_errno = ABTI_pool_print(p_pool);
        ABTI_CHECK_ERROR(abt_errno);
    }
    size_t size;
    ABT_sched_get_size(p_sched, &size);
    printf("size: %lu\n", (unsigned long)size);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABTI_sched_print", abt_errno);
    goto fn_exit;
}

