/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"

#ifdef ABT_CONFIG_USE_DEBUG_LOG
static inline uint64_t ABTI_sched_get_new_id(void);
#endif


/** @defgroup SCHED Scheduler
 * This group is for Scheduler.
 */

/**
 * @ingroup SCHED
 * @brief   Create a new user-defined scheduler and return its handle through
 * newsched.
 *
 * The pools used by the new scheduler are provided by \c pools. The contents
 * of this array is copied, so it can be freed. If a pool in the array is
 * ABT_POOL_NULL, the corresponding pool is automatically created.
 * The config must have been created by ABT_sched_config_create, and will be
 * used as argument in the initialization. If no specific configuration is
 * required, the parameter will be ABT_CONFIG_NULL.
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
    int p;

    ABTI_CHECK_TRUE(newsched != NULL, ABT_ERR_SCHED);

    p_sched = (ABTI_sched *)ABTU_malloc(sizeof(ABTI_sched));

    /* Copy of the contents of pools */
    ABT_pool *pool_list;
    pool_list = (ABT_pool *)ABTU_malloc(num_pools*sizeof(ABT_pool));
    for (p = 0; p < num_pools; p++) {
        if (pools[p] == ABT_POOL_NULL) {
            abt_errno = ABT_pool_create_basic(ABT_POOL_FIFO,
                                              ABT_POOL_ACCESS_MPSC,
                                              ABT_TRUE, &pool_list[p]);
            ABTI_CHECK_ERROR(abt_errno);
        } else {
            pool_list[p] = pools[p];
        }
    }

    /* Check if the pools are available */
    for (p = 0; p < num_pools; p++) {
        ABTI_pool_retain(ABTI_pool_get_ptr(pool_list[p]));
    }

    p_sched->used          = ABTI_SCHED_NOT_USED;
    p_sched->automatic     = ABT_FALSE;
    p_sched->kind          = ABTI_sched_get_kind(def);
    p_sched->state         = ABT_SCHED_STATE_READY;
    p_sched->request       = 0;
    p_sched->pools         = pool_list;
    p_sched->num_pools     = num_pools;
    p_sched->type          = def->type;
    p_sched->p_thread      = NULL;
    p_sched->p_task        = NULL;
    p_sched->p_ctx         = NULL;

    p_sched->init          = def->init;
    p_sched->run           = def->run;
    p_sched->free          = def->free;
    p_sched->get_migr_pool = def->get_migr_pool;

#ifdef ABT_CONFIG_USE_DEBUG_LOG
    p_sched->id            = ABTI_sched_get_new_id();
#endif
    LOG_EVENT("[S%" PRIu64 "] created\n", p_sched->id);

    /* Return value */
    *newsched = ABTI_sched_get_handle(p_sched);

    /* Specific initialization */
    p_sched->init(*newsched, config);

  fn_exit:
    return abt_errno;

  fn_fail:
    *newsched = ABT_SCHED_NULL;
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

/**
 * @ingroup SCHED
 * @brief   Create a predefined scheduler.
 *
 * \c ABT_sched_create_basic() creates a predefined scheduler and returns its
 * handle through \c newsched.  The pools used by the new scheduler are
 * provided by \c pools.  The contents of this array is copied, so it can be
 * freed. If a pool in the array is \c ABT_POOL_NULL, the corresponding pool is
 * automatically created.  The pool array can be \c NULL.  In this case, all
 * the pools will be created automatically.  The config must have been created
 * by \c ABT_sched_config_create(), and will be used as argument in the
 * initialization. If no specific configuration is required, the parameter can
 * be \c ABT_CONFIG_NULL.
 *
 * NOTE: The new scheduler will be automatically freed when it is not used
 * anymore or its associated ES is terminated.  Accordingly, the pools
 * associated with the new scheduler will be automatically freed if they are
 * exclusive to the scheduler and are not user-defined ones (i.e., created by
 * \c ABT_pool_create_basic() or implicitly created because \c pools is \c NULL
 * or a pool in the \c pools array is \c ABT_POOL_NULL).  If the pools were
 * created using \c ABT_pool_create() by the user, they have to be freed
 * explicitly with \c ABT_pool_free().
 *
 * @param[in]  predef    predefined scheduler
 * @param[in]  num_pools number of pools associated with this scheduler
 * @param[in]  pools     pools associated with this scheduler
 * @param[in]  config    specific config used during the scheduler creation
 * @param[out] newsched  handle to a new scheduler
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_sched_create_basic(ABT_sched_predef predef, int num_pools,
                           ABT_pool *pools, ABT_sched_config config,
                           ABT_sched *newsched)
{
    int abt_errno = ABT_SUCCESS;
    ABT_pool_access access;
    ABT_bool automatic;
    int p;

    /* We set the access to the default one */
    access = ABT_POOL_ACCESS_MPSC;
    automatic = ABT_TRUE;;
    /* We read the config and set the configured parameters */
    abt_errno = ABTI_sched_config_read_global(config, &access, &automatic);
    ABTI_CHECK_ERROR(abt_errno);

    /* A pool array is provided, predef has to be compatible */
    if (pools != NULL) {
        /* Copy of the contents of pools */
        ABT_pool *pool_list;
        pool_list = (ABT_pool *)ABTU_malloc(num_pools*sizeof(ABT_pool));
        for (p = 0; p < num_pools; p++) {
            if (pools[p] == ABT_POOL_NULL) {
                abt_errno = ABT_pool_create_basic(ABT_POOL_FIFO, access,
                                                  ABT_TRUE, &pool_list[p]);
                ABTI_CHECK_ERROR(abt_errno);
            } else {
                pool_list[p] = pools[p];
            }
        }

        /* Creation of the scheduler */
        switch (predef) {
            case ABT_SCHED_DEFAULT:
            case ABT_SCHED_BASIC:
                abt_errno = ABT_sched_create(ABTI_sched_get_basic_def(),
                                             num_pools, pool_list,
                                             ABT_SCHED_CONFIG_NULL,
                                             newsched);
                break;
            case ABT_SCHED_PRIO:
                abt_errno = ABT_sched_create(ABTI_sched_get_prio_def(),
                                             num_pools, pool_list,
                                             ABT_SCHED_CONFIG_NULL,
                                             newsched);
                break;
            case ABT_SCHED_RANDWS:
                abt_errno = ABT_sched_create(ABTI_sched_get_randws_def(),
                                             num_pools, pool_list,
                                             ABT_SCHED_CONFIG_NULL,
                                             newsched);
                break;
            default:
                abt_errno = ABT_ERR_INV_SCHED_PREDEF;
                break;
        }
        ABTI_CHECK_ERROR(abt_errno);
        ABTU_free(pool_list);
    }

    /* No pool array is provided, predef has to be compatible */
    else {
        /* Set the number of pools */
        switch (predef) {
            case ABT_SCHED_DEFAULT:
            case ABT_SCHED_BASIC:
                num_pools = 1;
                break;
            case ABT_SCHED_PRIO:
                num_pools = ABTI_SCHED_NUM_PRIO;
                break;
            case ABT_SCHED_RANDWS:
                num_pools = 1;
                break;
            default:
                abt_errno = ABT_ERR_INV_SCHED_PREDEF;
                ABTI_CHECK_ERROR(abt_errno);
                break;
        }

        /* Creation of the pools */
        /* To avoid the malloc overhead, we use a stack array. */
        ABT_pool pool_list[ABTI_SCHED_NUM_PRIO];
        int p;
        for (p = 0; p < num_pools; p++) {
            abt_errno = ABT_pool_create_basic(ABT_POOL_FIFO, access, ABT_TRUE,
                                              pool_list+p);
            ABTI_CHECK_ERROR(abt_errno);
        }

        /* Creation of the scheduler */
        switch (predef) {
            case ABT_SCHED_DEFAULT:
            case ABT_SCHED_BASIC:
                abt_errno = ABT_sched_create(ABTI_sched_get_basic_def(),
                                             num_pools, pool_list,
                                             config, newsched);
                break;
            case ABT_SCHED_PRIO:
                abt_errno = ABT_sched_create(ABTI_sched_get_prio_def(),
                                             num_pools, pool_list,
                                             config, newsched);
                break;
            case ABT_SCHED_RANDWS:
                abt_errno = ABT_sched_create(ABTI_sched_get_randws_def(),
                                             num_pools, pool_list,
                                             config, newsched);
                break;
            default:
                abt_errno = ABT_ERR_INV_SCHED_PREDEF;
                ABTI_CHECK_ERROR(abt_errno);
                break;
        }
    }
    ABTI_CHECK_ERROR(abt_errno);

    ABTI_sched *p_sched = ABTI_sched_get_ptr(*newsched);
    p_sched->automatic = automatic;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    *newsched = ABT_SCHED_NULL;
    goto fn_exit;
}

/**
 * @ingroup SCHED
 * @brief   Release the scheduler object associated with sched handle.
 *
 * If this routine successfully returns, sched is set as ABT_SCHED_NULL. The
 * scheduler will be automatically freed.
 * If \c sched is currently being used by an ES or in a pool, freeing \c sched
 * is not allowed. In this case, this routine fails and returns \c
 * ABT_ERR_SCHED.
 *
 * @param[in,out] sched  handle to the target scheduler
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_sched_free(ABT_sched *sched)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_sched *p_sched = ABTI_sched_get_ptr(*sched);
    ABTI_CHECK_NULL_SCHED_PTR(p_sched);

    /* Free the scheduler */
    abt_errno = ABTI_sched_free(p_sched);
    ABTI_CHECK_ERROR(abt_errno);

    /* Return value */
    *sched = ABT_SCHED_NULL;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

/**
 * @ingroup SCHED
 * @brief   Get the number of pools associated with scheduler.
 *
 * \c ABT_sched_get_num_pools returns the number of pools associated with
 * the target scheduler \c sched through \c num_pools.
 *
 * @param[in]  sched     handle to the target scheduler
 * @param[out] num_pools the number of all pools associated with \c sched
 * @return Error code
 * @retval ABT_SUCCESS       on success
 * @retval ABT_ERR_INV_SCHED invalid scheduler
 */
int ABT_sched_get_num_pools(ABT_sched sched, int *num_pools)
{
    int abt_errno = ABT_SUCCESS;

    ABTI_sched *p_sched = ABTI_sched_get_ptr(sched);
    ABTI_CHECK_NULL_SCHED_PTR(p_sched);

    *num_pools = p_sched->num_pools;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
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

    ABTI_CHECK_TRUE(idx+max_pools <= p_sched->num_pools, ABT_ERR_SCHED);

    int p;
    for (p = idx; p < idx+max_pools; p++) {
        pools[p-idx] = p_sched->pools[p];
    }

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
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

    ABTI_sched_set_request(p_sched, ABTI_SCHED_REQ_FINISH);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
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

    ABTI_sched_set_request(p_sched, ABTI_SCHED_REQ_EXIT);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
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
int ABT_sched_has_to_stop(ABT_sched sched, ABT_bool *stop)
{
    int abt_errno = ABT_SUCCESS;

    *stop = ABT_FALSE;

    /* When this routine is called by an external thread, e.g., pthread */
    if (lp_ABTI_local == NULL) {
        abt_errno = ABT_ERR_INV_XSTREAM;
        goto fn_exit;
    }

    ABTI_xstream *p_xstream = ABTI_local_get_xstream();

    ABTI_sched *p_sched = ABTI_sched_get_ptr(sched);
    ABTI_CHECK_NULL_SCHED_PTR(p_sched);

    *stop = ABTI_sched_has_to_stop(p_sched, p_xstream);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

ABT_bool ABTI_sched_has_to_stop(ABTI_sched *p_sched, ABTI_xstream *p_xstream)
{
    ABT_bool stop = ABT_FALSE;
    size_t size;

    /* Check exit request */
    if (p_sched->request & ABTI_SCHED_REQ_EXIT) {
        ABTI_mutex_spinlock(&p_xstream->top_sched_mutex);
        p_sched->state = ABT_SCHED_STATE_TERMINATED;
        stop = ABT_TRUE;
        goto fn_exit;
    }

    size = ABTI_sched_get_effective_size(p_sched);
    if (size == 0) {
        if (p_sched->request & ABTI_SCHED_REQ_FINISH) {
            /* Check join request */
            /* We need to lock in case someone wants to migrate to this
             * scheduler */
            ABTI_mutex_spinlock(&p_xstream->top_sched_mutex);
            size_t size = ABTI_sched_get_effective_size(p_sched);
            if (size == 0) {
                p_sched->state = ABT_SCHED_STATE_TERMINATED;
                stop = ABT_TRUE;
            } else {
                ABTI_mutex_unlock(&p_xstream->top_sched_mutex);
            }
        } else if (p_sched->used == ABTI_SCHED_IN_POOL) {
            /* If the scheduler is a stacked one, we have to escape from the
             * scheduling function. The scheduler will be stopped if it is a
             * tasklet type. However, if the scheduler is a ULT type, we
             * context switch to the parent scheduler. */
            if (p_sched->type == ABT_SCHED_TYPE_TASK) {
                p_sched->state = ABT_SCHED_STATE_TERMINATED;
                stop = ABT_TRUE;
            } else {
                ABTI_ASSERT(p_sched->type == ABT_SCHED_TYPE_ULT);
                ABTI_sched *p_par_sched;
                p_par_sched = ABTI_xstream_get_parent_sched(p_xstream);
                ABTD_thread_context_switch(p_sched->p_ctx, p_par_sched->p_ctx);
            }
        }
    }

  fn_exit:
    return stop;
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
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
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
 * @param[out] data   specific data of the scheduler
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_sched_get_data(ABT_sched sched, void **data)
{
    int abt_errno = ABT_SUCCESS;

    ABTI_sched *p_sched = ABTI_sched_get_ptr(sched);
    ABTI_CHECK_NULL_SCHED_PTR(p_sched);

    *data = p_sched->data;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

/**
 * @ingroup SCHED
 * @brief   Get the sum of the sizes of the pool of \c sched.
 *
 * The size does not include the blocked and migrating ULTs.
 *
 * @param[in]  sched handle to the scheduler
 * @param[out] size  total number of work units
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_sched_get_size(ABT_sched sched, size_t *size)
{
    int abt_errno = ABT_SUCCESS;
    size_t pool_size = 0;

    ABTI_sched *p_sched = ABTI_sched_get_ptr(sched);
    ABTI_CHECK_NULL_SCHED_PTR(p_sched);

    pool_size = ABTI_sched_get_size(p_sched);

  fn_exit:
    *size = pool_size;
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

size_t ABTI_sched_get_size(ABTI_sched *p_sched)
{
    size_t pool_size = 0;
    int p;

    for (p = 0; p < p_sched->num_pools; p++) {
        size_t s;
        ABT_pool pool = p_sched->pools[p];
        ABT_pool_get_size(pool, &s);
        pool_size += s;
    }

    return pool_size;
}

/**
 * @ingroup SCHED
 * @brief   Get the sum of the sizes of the pool of \c sched.
 *
 * The size includes the blocked and migrating ULTs.
 *
 * @param[in]  sched handle to the scheduler
 * @param[out] size  total number of work units
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_sched_get_total_size(ABT_sched sched, size_t *size)
{
    int abt_errno = ABT_SUCCESS;
    size_t pool_size = 0;

    ABTI_sched *p_sched = ABTI_sched_get_ptr(sched);
    ABTI_CHECK_NULL_SCHED_PTR(p_sched);

    pool_size = ABTI_sched_get_total_size(p_sched);

  fn_exit:
    *size = pool_size;
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

size_t ABTI_sched_get_total_size(ABTI_sched *p_sched)
{
    size_t pool_size = 0;
    int p;

    for (p = 0; p < p_sched->num_pools; p++) {
        size_t s;
        ABT_pool pool = p_sched->pools[p];
        ABT_pool_get_total_size(pool, &s);
        pool_size += s;
    }

    return pool_size;
}

/* Compared to \c ABTI_sched_get_total_size, ABTI_sched_get_effective_size does
 * not count the number of blocked ULTs if a pool has more than one consumer or
 * the caller ES is not the latest consumer. This is necessary when the ES
 * associated with the target scheduler has to be joined and the pool is shared
 * between different schedulers associated with different ESs. */
size_t ABTI_sched_get_effective_size(ABTI_sched *p_sched)
{
    size_t pool_size = 0;
    int p;

#ifndef ABT_CONFIG_DISABLE_POOL_CONSUMER_CHECK
    ABTI_xstream *p_xstream = ABTI_local_get_xstream();
#endif

    for (p = 0; p < p_sched->num_pools; p++) {
        ABT_pool pool = p_sched->pools[p];
        ABTI_pool *p_pool = ABTI_pool_get_ptr(pool);
        pool_size += p_pool->p_get_size(pool);
        pool_size += p_pool->num_migrations;
        switch (p_pool->access) {
            case ABT_POOL_ACCESS_PRIV:
                pool_size += p_pool->num_blocked;
                break;
            case ABT_POOL_ACCESS_SPSC:
            case ABT_POOL_ACCESS_MPSC:
            case ABT_POOL_ACCESS_SPMC:
            case ABT_POOL_ACCESS_MPMC:
#ifdef ABT_CONFIG_DISABLE_POOL_CONSUMER_CHECK
                if (p_pool->num_scheds == 1) {
                    pool_size += p_pool->num_blocked;
                }
#else
                if (p_pool->num_scheds == 1 && p_pool->consumer == p_xstream) {
                    pool_size += p_pool->num_blocked;
                }
#endif
                break;
            default: break;
        }
    }

    return pool_size;
}


/*****************************************************************************/
/* Private APIs                                                              */
/*****************************************************************************/

int ABTI_sched_free(ABTI_sched *p_sched)
{
    int abt_errno = ABT_SUCCESS;
    int p;

    /* If sched is currently used, free is not allowed. */
    if (p_sched->used != ABTI_SCHED_NOT_USED) {
        abt_errno = ABT_ERR_SCHED;
        goto fn_fail;
    }

    /* If sched is a default provided one, it should free its pool here.
     * Otherwise, freeing the pool is the user's reponsibility. */
    for (p = 0; p < p_sched->num_pools; p++) {
        ABTI_pool *p_pool = ABTI_pool_get_ptr(p_sched->pools[p]);
        int32_t num_scheds = ABTI_pool_release(p_pool);
        if (p_pool->automatic == ABT_TRUE && num_scheds == 0) {
            abt_errno = ABT_pool_free(p_sched->pools+p);
            ABTI_CHECK_ERROR(abt_errno);
        }
    }
    ABTU_free(p_sched->pools);

    /* Free the associated work unit */
    if (p_sched->type == ABT_SCHED_TYPE_ULT) {
        if (p_sched->p_thread) {
            if (p_sched->p_thread->type == ABTI_THREAD_TYPE_MAIN_SCHED) {
                ABTI_thread_free_main_sched(p_sched->p_thread);
            } else {
                ABTI_thread_free(p_sched->p_thread);
            }
        }
    } else if (p_sched->type == ABT_SCHED_TYPE_TASK) {
        if (p_sched->p_task) {
            ABTI_task_free(p_sched->p_task);
        }
    }

    LOG_EVENT("[S%" PRIu64 "] freed\n", p_sched->id);

    p_sched->free(ABTI_sched_get_handle(p_sched));
    p_sched->data = NULL;

    ABTU_free(p_sched);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

/* Get the pool suitable for receiving a migrating ULT */
int ABTI_sched_get_migration_pool(ABTI_sched *p_sched, ABTI_pool *source_pool,
                                  ABTI_pool **pp_pool)
{
    int abt_errno = ABT_SUCCESS;
    ABT_sched sched = ABTI_sched_get_handle(p_sched);
    ABTI_pool *p_pool;

    ABTI_CHECK_TRUE(p_sched->state != ABT_SCHED_STATE_TERMINATED,
                    ABT_ERR_INV_SCHED);

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
        ABTI_CHECK_TRUE(0, ABT_ERR_INV_POOL_ACCESS);
    }

  fn_exit:
    return abt_errno;

  fn_fail:
    *pp_pool = NULL;
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

ABTI_sched_kind ABTI_sched_get_kind(ABT_sched_def *def)
{
  return (ABTI_sched_kind)def;
}

void ABTI_sched_print(ABTI_sched *p_sched, FILE *p_os, int indent,
                      ABT_bool print_sub)
{
    char *prefix = ABTU_get_indent_str(indent);

    if (p_sched == NULL) {
        fprintf(p_os, "%s== NULL SCHED ==\n", prefix);
        goto fn_exit;
    }

    ABTI_sched_kind kind;
    char *kind_str, *type, *state, *used;
    char *pools_str;
    int i;
    size_t size, pos;

    kind = p_sched->kind;
    if (kind == ABTI_sched_get_kind(ABTI_sched_get_basic_def())) {
        kind_str = "BASIC";
    } else if (kind == ABTI_sched_get_kind(ABTI_sched_get_prio_def())) {
        kind_str = "PRIO";
    } else {
        kind_str = "USER";
    }

    switch (p_sched->type) {
        case ABT_SCHED_TYPE_ULT:  type = "ULT"; break;
        case ABT_SCHED_TYPE_TASK: type = "TASKLET"; break;
        default:                  type = "UNKNOWN"; break;
    }
    switch (p_sched->state) {
        case ABT_SCHED_STATE_READY:      state = "READY"; break;
        case ABT_SCHED_STATE_RUNNING:    state = "RUNNING"; break;
        case ABT_SCHED_STATE_STOPPED:    state = "STOPPED"; break;
        case ABT_SCHED_STATE_TERMINATED: state = "TERMINATED"; break;
        default:                         state = "UNKNOWN"; break;
    }
    switch (p_sched->used) {
        case ABTI_SCHED_NOT_USED: used = "NOT_USED"; break;
        case ABTI_SCHED_MAIN:     used = "MAIN"; break;
        case ABTI_SCHED_IN_POOL:  used = "IN_POOL"; break;
        default:                  used = "UNKNOWN"; break;
    }

    size = sizeof(char) * (p_sched->num_pools * 20 + 4);
    pools_str = (char *)ABTU_calloc(size, 1);
    pools_str[0] = '[';
    pools_str[1] = ' ';
    pos = 2;
    for (i = 0; i < p_sched->num_pools; i++) {
        ABTI_pool *p_pool = ABTI_pool_get_ptr(p_sched->pools[i]);
        sprintf(&pools_str[pos], "%p ", p_pool);
        pos = strlen(pools_str);
    }
    pools_str[pos] = ']';

    fprintf(p_os,
        "%s== SCHED (%p) ==\n"
#ifdef ABT_CONFIG_USE_DEBUG_LOG
        "%sid       : %" PRIu64 "\n"
#endif
        "%skind     : %" PRIu64 " (%s)\n"
        "%stype     : %s\n"
        "%sstate    : %s\n"
        "%sused     : %s\n"
        "%sautomatic: %s\n"
        "%srequest  : 0x%x\n"
        "%snum_pools: %d\n"
        "%spools    : %s\n"
        "%ssize     : %zu\n"
        "%stot_size : %zu\n"
        "%sdata     : %p\n",
        prefix, p_sched,
#ifdef ABT_CONFIG_USE_DEBUG_LOG
        prefix, p_sched->id,
#endif
        prefix, p_sched->kind, kind_str,
        prefix, type,
        prefix, state,
        prefix, used,
        prefix, (p_sched->automatic == ABT_TRUE) ? "TRUE" : "FALSE",
        prefix, p_sched->request,
        prefix, p_sched->num_pools,
        prefix, pools_str,
        prefix, ABTI_sched_get_size(p_sched),
        prefix, ABTI_sched_get_total_size(p_sched),
        prefix, p_sched->data
    );
    ABTU_free(pools_str);

    if (print_sub == ABT_TRUE) {
        for (i = 0; i < p_sched->num_pools; i++) {
            ABTI_pool *p_pool = ABTI_pool_get_ptr(p_sched->pools[i]);
            ABTI_pool_print(p_pool, p_os, indent + 2);
        }
    }

  fn_exit:
    fflush(p_os);
    ABTU_free(prefix);
}

static uint64_t g_sched_id = 0;
void ABTI_sched_reset_id(void)
{
    g_sched_id = 0;
}

/*****************************************************************************/
/* Internal static functions                                                 */
/*****************************************************************************/

#ifdef ABT_CONFIG_USE_DEBUG_LOG
static inline uint64_t ABTI_sched_get_new_id(void)
{
    return (uint64_t)ABTD_atomic_fetch_add_uint64(&g_sched_id, 1);
}
#endif
