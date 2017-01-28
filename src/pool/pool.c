/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"

static inline uint64_t ABTI_pool_get_new_id(void);


/** @defgroup POOL Pool
 * This group is for Pool.
 */

/**
 * @ingroup POOL
 * @brief   Create a new pool and return its handle through \c newpool.
 *
 * This function creates a new pool, given by a definition (\c def) and a
 * configuration (\c config). The configuration can be \c ABT_SCHED_CONFIG_NULL
 * or obtained from a specific function of the pool defined by \c def. The
 * configuration will be passed as the parameter of the initialization function
 * of the pool.
 *
 * @param[in]  def     definition required for pool creation
 * @param[in]  config  specific config used during the pool creation
 * @param[out] newpool handle to a new pool
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_pool_create(ABT_pool_def *def, ABT_pool_config config,
                    ABT_pool *newpool)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_pool *p_pool;

    p_pool = (ABTI_pool *)ABTU_malloc(sizeof(ABTI_pool));
    p_pool->access               = def->access;
    p_pool->automatic            = ABT_FALSE;
    p_pool->num_scheds           = 0;
#ifndef ABT_CONFIG_DISABLE_POOL_CONSUMER_CHECK
    p_pool->consumer             = NULL;
#endif
#ifndef ABT_CONFIG_DISABLE_POOL_PRODUCER_CHECK
    p_pool->producer             = NULL;
#endif
    p_pool->num_blocked          = 0;
    p_pool->num_migrations       = 0;
    p_pool->data                 = NULL;

    /* Set up the pool functions from def */
    p_pool->u_get_type           = def->u_get_type;
    p_pool->u_get_thread         = def->u_get_thread;
    p_pool->u_get_task           = def->u_get_task;
    p_pool->u_is_in_pool         = def->u_is_in_pool;
    p_pool->u_create_from_thread = def->u_create_from_thread;
    p_pool->u_create_from_task   = def->u_create_from_task;
    p_pool->u_free               = def->u_free;
    p_pool->p_init               = def->p_init;
    p_pool->p_get_size           = def->p_get_size;
    p_pool->p_push               = def->p_push;
    p_pool->p_pop                = def->p_pop;
    p_pool->p_remove             = def->p_remove;
    p_pool->p_free               = def->p_free;
    p_pool->id                   = ABTI_pool_get_new_id();
    LOG_EVENT("[P%" PRIu64 "] created\n", p_pool->id);

    *newpool = ABTI_pool_get_handle(p_pool);

    /* Configure the pool */
    if (p_pool->p_init) {
        abt_errno = p_pool->p_init(*newpool, config);
        if (abt_errno != ABT_SUCCESS) {
            ABTU_free(p_pool);
            goto fn_fail;
        }
    }

  fn_exit:
    return abt_errno;

  fn_fail:
    *newpool = ABT_POOL_NULL;
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

/**
 * @ingroup POOL
 * @brief   Create a new pool from a predefined type and return its handle
 *          through \c newpool.
 *
 * For more details see \c ABT_pool_create().
 *
 * @param[in]  kind      name of the predefined pool
 * @param[in]  access    access type of the predefined pool
 * @param[in]  automatic ABT_TRUE if the pool should be automatically freed
 * @param[out] newpool   handle to a new pool
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_pool_create_basic(ABT_pool_kind kind, ABT_pool_access access,
                          ABT_bool automatic, ABT_pool *newpool)
{
    int abt_errno = ABT_SUCCESS;
    ABT_pool_def def;

    switch (kind) {
        case ABT_POOL_FIFO:
            abt_errno = ABTI_pool_get_fifo_def(access, &def);
            break;
        default:
            abt_errno = ABT_ERR_INV_POOL_KIND;
            break;
    }
    ABTI_CHECK_ERROR(abt_errno);

    abt_errno = ABT_pool_create(&def, ABT_POOL_CONFIG_NULL, newpool);
    ABTI_CHECK_ERROR(abt_errno);
    ABTI_pool *p_pool = ABTI_pool_get_ptr(*newpool);
    p_pool->automatic = automatic;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    *newpool = ABT_SCHED_NULL;
    goto fn_exit;
}

/**
 * @ingroup POOL
 * @brief   Free the given pool, and modify its value to ABT_POOL_NULL
 *
 * @param[inout] pool handle
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_pool_free(ABT_pool *pool)
{
    int abt_errno = ABT_SUCCESS;

    ABT_pool h_pool = *pool;
    ABTI_pool *p_pool = ABTI_pool_get_ptr(h_pool);

    ABTI_CHECK_TRUE(p_pool != NULL && h_pool != ABT_POOL_NULL, ABT_ERR_INV_POOL);

    LOG_EVENT("[P%" PRIu64 "] freed\n", p_pool->id);

    p_pool->p_free(h_pool);
    ABTU_free(p_pool);

    *pool = ABT_POOL_NULL;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

/**
 * @ingroup POOL
 * @brief   Get the access type of target pool
 *
 * @param[in]  pool    handle to the pool
 * @param[out] access  access type
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_pool_get_access(ABT_pool pool, ABT_pool_access *access)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_pool *p_pool = ABTI_pool_get_ptr(pool);
    ABTI_CHECK_NULL_POOL_PTR(p_pool);

    *access = p_pool->access;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

/**
 * @ingroup POOL
 * @brief   Return the total size of a pool
 *
 * The returned size is the number of elements in the pool (provided by the
 * specific function in case of a user-defined pool), plus the number of
 * blocked ULTs and migrating ULTs.
 *
 * @param[in] pool handle to the pool
 * @param[out] size size of the pool
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_pool_get_total_size(ABT_pool pool, size_t *size)
{
    int abt_errno = ABT_SUCCESS;
    size_t total_size;

    ABTI_pool *p_pool = ABTI_pool_get_ptr(pool);
    ABTI_CHECK_NULL_POOL_PTR(p_pool);

    total_size = ABTI_pool_get_size(p_pool);
    total_size += p_pool->num_blocked;
    total_size += p_pool->num_migrations;
    *size = total_size;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}


/**
 * @ingroup POOL
 * @brief   Return the size of a pool
 *
 * The returned size is the number of elements in the pool (provided by the
 * specific function in case of a user-defined pool).
 *
 * @param[in] pool handle to the pool
 * @param[out] size size of the pool
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_pool_get_size(ABT_pool pool, size_t *size)
{
    int abt_errno = ABT_SUCCESS;

    ABTI_pool *p_pool = ABTI_pool_get_ptr(pool);
    ABTI_CHECK_NULL_POOL_PTR(p_pool);

    *size = ABTI_pool_get_size(p_pool);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

/**
 * @ingroup POOL
 * @brief   Pop a unit from the target pool
 *
 * @param[in] pool handle to the pool
 * @param[out] p_unit handle to the unit
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_pool_pop(ABT_pool pool, ABT_unit *p_unit)
{
    int abt_errno = ABT_SUCCESS;
    ABT_unit unit;

    /* If called by an external thread, return an error. */
    ABTI_CHECK_TRUE(lp_ABTI_local != NULL, ABT_ERR_INV_XSTREAM);

    ABTI_pool *p_pool = ABTI_pool_get_ptr(pool);
    ABTI_CHECK_NULL_POOL_PTR(p_pool);

    unit = ABTI_pool_pop(p_pool);

  fn_exit:
    *p_unit = unit;
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    unit = ABT_UNIT_NULL;
    goto fn_exit;
}

/**
 * @ingroup POOL
 * @brief   Push a unit to the target pool
 *
 * @param[in] pool handle to the pool
 * @param[in] unit handle to the unit
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_pool_push(ABT_pool pool, ABT_unit unit)
{
    int abt_errno = ABT_SUCCESS;

    ABTI_pool *p_pool = ABTI_pool_get_ptr(pool);
    ABTI_CHECK_NULL_POOL_PTR(p_pool);

    ABTI_CHECK_TRUE(unit != ABT_UNIT_NULL, ABT_ERR_UNIT);

#ifdef ABT_CONFIG_DISABLE_POOL_PRODUCER_CHECK
    ABTI_pool_push(p_pool, unit);
#else
    /* Save the producer ES information in the pool */
    ABTI_xstream *p_xstream = ABTI_xstream_self();
    abt_errno = ABTI_pool_push(p_pool, unit, p_xstream);
    ABTI_CHECK_ERROR(abt_errno);
#endif

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

/**
 * @ingroup POOL
 * @brief   Remove a specified unit from the target pool
 *
 * @param[in] pool handle to the pool
 * @param[in] unit handle to the unit
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_pool_remove(ABT_pool pool, ABT_unit unit)
{
    int abt_errno = ABT_SUCCESS;

    /* If called by an external thread, return an error. */
    ABTI_CHECK_TRUE(lp_ABTI_local != NULL, ABT_ERR_INV_XSTREAM);

    ABTI_pool *p_pool = ABTI_pool_get_ptr(pool);
    ABTI_CHECK_NULL_POOL_PTR(p_pool);

    abt_errno = ABTI_POOL_REMOVE(p_pool, unit, ABTI_local_get_xstream());
    ABTI_CHECK_ERROR(abt_errno);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

/**
 * @ingroup POOL
 * @brief   Set the specific data of the target user-defined pool
 *
 * This function will be called by the user during the initialization of his
 * user-defined pool.
 *
 * @param[in] pool handle to the pool
 * @param[in] data specific data of the pool
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_pool_set_data(ABT_pool pool, void *data)
{
    int abt_errno = ABT_SUCCESS;

    ABTI_pool *p_pool = ABTI_pool_get_ptr(pool);
    ABTI_CHECK_NULL_POOL_PTR(p_pool);

    p_pool->data = data;

fn_exit:
    return abt_errno;

fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

/**
 * @ingroup POOL
 * @brief   Retrieve the specific data of the target user-defined pool
 *
 * This function will be called by the user in a user-defined function of his
 * user-defined pool.
 *
 * @param[in] pool handle to the pool
 * @param[in] data specific data of the pool
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_pool_get_data(ABT_pool pool, void **data)
{
    int abt_errno = ABT_SUCCESS;

    ABTI_pool *p_pool = ABTI_pool_get_ptr(pool);
    ABTI_CHECK_NULL_POOL_PTR(p_pool);

    *data = ABTI_pool_get_data(p_pool);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

/**
 * @ingroup POOL
 * @brief   Push a scheduler to a pool
 *
 * By pushing a scheduler, the user can change the running scheduler: when the
 * top scheduler (the running scheduler) will pick it from the pool and run it,
 * it will become the new scheduler. This new scheduler will be in charge until
 * it explicitly yields, except if ABT_sched_finish() or ABT_sched_exit() are
 * called.
 *
 * The scheduler should have been created by ABT_sched_create or
 * ABT_sched_create_basic.
 *
 * @param[in] pool handle to the pool
 * @param[in] sched handle to the sched
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_pool_add_sched(ABT_pool pool, ABT_sched sched)
{
#ifdef ABT_CONFIG_DISABLE_STACKABLE_SCHED
    return ABT_ERR_FEATURE_NA;
#else
    int abt_errno = ABT_SUCCESS;
    
    ABTI_pool *p_pool = ABTI_pool_get_ptr(pool);
    ABTI_CHECK_NULL_POOL_PTR(p_pool);

    ABTI_sched *p_sched = ABTI_sched_get_ptr(sched);
    ABTI_CHECK_NULL_SCHED_PTR(p_sched);

#ifndef ABT_CONFIG_DISABLE_POOL_CONSUMER_CHECK
    int p;

    switch (p_pool->access) {
        case ABT_POOL_ACCESS_PRIV:
        case ABT_POOL_ACCESS_SPSC:
        case ABT_POOL_ACCESS_MPSC:
            /* we need to ensure that the target pool has already an
             * associated ES */
            ABTI_CHECK_TRUE(p_pool->consumer != NULL, ABT_ERR_POOL);

            /* We check that from the pool set of the scheduler we do not find
             * a pool with another associated pool, and set the right value if
             * it is okay  */
            for (p = 0; p < p_sched->num_pools; p++) {
                abt_errno = ABTI_pool_set_consumer(p_sched->pools[p],
                                                   p_pool->consumer);
                ABTI_CHECK_ERROR(abt_errno);
            }
            break;

        case ABT_POOL_ACCESS_SPMC:
        case ABT_POOL_ACCESS_MPMC:
            /* we need to ensure that the pool set of the scheduler does
             * not contain an ES private pool  */
            for (p = 0; p < p_sched->num_pools; p++) {
                ABTI_pool *p_pool = ABTI_pool_get_ptr(p_sched->pools[p]);
                ABTI_CHECK_TRUE(p_pool->access != ABT_POOL_ACCESS_PRIV &&
                                  p_pool->access != ABT_POOL_ACCESS_SPSC &&
                                  p_pool->access != ABT_POOL_ACCESS_MPSC,
                                ABT_ERR_POOL);
            }
            break;

        default:
            ABTI_CHECK_TRUE(0, ABT_ERR_INV_POOL_ACCESS);
    }
#endif

    /* Mark the scheduler as it is used in pool */
    ABTI_CHECK_TRUE(p_sched->used == ABTI_SCHED_NOT_USED, ABT_ERR_INV_SCHED);
    p_sched->used = ABTI_SCHED_IN_POOL;

    if (p_sched->type == ABT_SCHED_TYPE_ULT) {
        abt_errno = ABTI_thread_create_sched(p_pool, p_sched);
        ABTI_CHECK_ERROR(abt_errno);
    } else if (p_sched->type == ABT_SCHED_TYPE_TASK){
        abt_errno = ABTI_task_create_sched(p_pool, p_sched);
        ABTI_CHECK_ERROR(abt_errno);
    } else {
        ABTI_CHECK_TRUE(0, ABT_ERR_SCHED);
    }

fn_exit:
    return abt_errno;

fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
#endif
}

/**
 * @ingroup POOL
 * @brief   Get the ID of the target pool
 *
 * \c ABT_pool_get_id() returns the ID of \c pool.
 *
 * @param[in]  pool  handle to the target pool
 * @param[out] id    pool id
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_pool_get_id(ABT_pool pool, int *id)
{
    int abt_errno = ABT_SUCCESS;

    ABTI_pool *p_pool = ABTI_pool_get_ptr(pool);
    ABTI_CHECK_NULL_POOL_PTR(p_pool);

    *id = (int)p_pool->id;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

/*****************************************************************************/
/* Private APIs                                                              */
/*****************************************************************************/
void ABTI_pool_print(ABTI_pool *p_pool, FILE *p_os, int indent)
{
    char *prefix = ABTU_get_indent_str(indent);

    if (p_pool == NULL) {
        fprintf(p_os, "%s== NULL POOL ==\n", prefix);
        goto fn_exit;
    }

    char *access;

    switch (p_pool->access) {
        case ABT_POOL_ACCESS_PRIV: access = "PRIV"; break;
        case ABT_POOL_ACCESS_SPSC: access = "SPSC"; break;
        case ABT_POOL_ACCESS_MPSC: access = "MPSC"; break;
        case ABT_POOL_ACCESS_SPMC: access = "SPMC"; break;
        case ABT_POOL_ACCESS_MPMC: access = "MPMC"; break;
        default:                   access = "UNKNOWN"; break;
    }

    fprintf(p_os,
        "%s== POOL (%p) ==\n"
        "%sid            : %" PRIu64 "\n"
        "%saccess        : %s\n"
        "%sautomatic     : %s\n"
        "%snum_scheds    : %d\n"
#ifndef ABT_CONFIG_DISABLE_POOL_CONSUMER_CHECK
        "%sconsumer ES   : %p (%d)\n"
#endif
#ifndef ABT_CONFIG_DISABLE_POOL_PRODUCER_CHECK
        "%sproducer ES   : %p (%d)\n"
#endif
        "%ssize          : %zu\n"
        "%snum_blocked   : %u\n"
        "%snum_migrations: %d\n"
        "%sdata          : %p\n",
        prefix, p_pool,
        prefix, p_pool->id,
        prefix, access,
        prefix, (p_pool->automatic == ABT_TRUE) ? "TRUE" : "FALSE",
        prefix, p_pool->num_scheds,
#ifndef ABT_CONFIG_DISABLE_POOL_CONSUMER_CHECK
        prefix, p_pool->consumer, p_pool->consumer ? p_pool->consumer->rank : 0,
#endif
#ifndef ABT_CONFIG_DISABLE_POOL_PRODUCER_CHECK
        prefix, p_pool->producer, p_pool->producer ? p_pool->producer->rank : 0,
#endif
        prefix, ABTI_pool_get_size(p_pool),
        prefix, p_pool->num_blocked,
        prefix, p_pool->num_migrations,
        prefix, p_pool->data
    );

  fn_exit:
    fflush(p_os);
    ABTU_free(prefix);
}

#ifndef ABT_CONFIG_DISABLE_POOL_CONSUMER_CHECK
/* Set the associated consumer ES of a pool. This function has no effect on pools
 * of shared-read access mode.
 * If a pool is private-read to an ES, we check that the previous value of the
 * field "p_xstream" is the same as the argument of the function "p_xstream"
 * */
int ABTI_pool_set_consumer(ABTI_pool *p_pool, ABTI_xstream *p_xstream)
{
    int abt_errno = ABT_SUCCESS;

    if (p_pool->num_scheds == 0) {
        return abt_errno;
    }

    switch (p_pool->access) {
        case ABT_POOL_ACCESS_PRIV:
#ifndef ABT_CONFIG_DISABLE_POOL_PRODUCER_CHECK
            if (p_pool->producer && p_xstream != p_pool->producer) {
                abt_errno = ABT_ERR_INV_POOL_ACCESS;
                ABTI_CHECK_ERROR(abt_errno);
            }
#endif
        case ABT_POOL_ACCESS_SPSC:
        case ABT_POOL_ACCESS_MPSC:
            if (p_pool->consumer && p_pool->consumer != p_xstream) {
                abt_errno = ABT_ERR_INV_POOL_ACCESS;
                ABTI_CHECK_ERROR(abt_errno);
            }
            /* NB: as we do not want to use a mutex, the function can be wrong
             * here */
            p_pool->consumer = p_xstream;
            break;

        case ABT_POOL_ACCESS_SPMC:
        case ABT_POOL_ACCESS_MPMC:
            p_pool->consumer = p_xstream;
            break;

        default:
            abt_errno = ABT_ERR_INV_POOL_ACCESS;
            ABTI_CHECK_ERROR(abt_errno);
    }

fn_exit:
    return abt_errno;

fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}
#endif

#ifndef ABT_CONFIG_DISABLE_POOL_PRODUCER_CHECK
/* Set the associated producer ES of a pool. This function has no effect on pools
 * of shared-write access mode.
 * If a pool is private-write to an ES, we check that the previous value of the
 * field "p_xstream" is the same as the argument of the function "p_xstream"
 * */
int ABTI_pool_set_producer(ABTI_pool *p_pool, ABTI_xstream *p_xstream)
{
    int abt_errno = ABT_SUCCESS;

    if (p_pool->num_scheds == 0) {
        return abt_errno;
    }

    switch (p_pool->access) {
        case ABT_POOL_ACCESS_PRIV:
#ifndef ABT_CONFIG_DISABLE_POOL_CONSUMER_CHECK
            ABTI_CHECK_TRUE(!p_pool->consumer || p_xstream == p_pool->consumer,
                            ABT_ERR_INV_POOL_ACCESS);
#endif
        case ABT_POOL_ACCESS_SPSC:
        case ABT_POOL_ACCESS_SPMC:
            ABTI_CHECK_TRUE(!p_pool->producer || p_pool->producer == p_xstream,
                            ABT_ERR_INV_POOL_ACCESS);
            /* NB: as we do not want to use a mutex, the function can be wrong
             * here */
            p_pool->producer = p_xstream;
            break;

        case ABT_POOL_ACCESS_MPSC:
        case ABT_POOL_ACCESS_MPMC:
            p_pool->producer = p_xstream;
            break;

        default:
            abt_errno = ABT_ERR_INV_POOL_ACCESS;
            ABTI_CHECK_ERROR(abt_errno);
    }

fn_exit:
    return abt_errno;

fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}
#endif

/* Check if a pool accept migrations or not. When the producer of the
 * destination pool is ES private, we have to ensure thaht we are on the right
 * ES */
int ABTI_pool_accept_migration(ABTI_pool *p_pool, ABTI_pool *source)
{
#if !defined(ABT_CONFIG_DISABLE_POOL_PRODUCER_CHECK) && \
    !defined(ABT_CONFIG_DISABLE_POOL_CONSUMER_CHECK)
    switch (p_pool->access)
    {
        /* Need producer in the same ES */
        case ABT_POOL_ACCESS_PRIV:
        case ABT_POOL_ACCESS_SPSC:
        case ABT_POOL_ACCESS_SPMC:
            if (p_pool->consumer == source->producer)
                return ABT_TRUE;
            return ABT_FALSE;

        case ABT_POOL_ACCESS_MPSC:
        case ABT_POOL_ACCESS_MPMC:
            return ABT_TRUE;
        default:
            return ABT_FALSE;
    }
#else
    return ABT_TRUE;
#endif
}


static uint64_t g_pool_id = 0;
void ABTI_pool_reset_id(void)
{
    g_pool_id = 0;
}

/*****************************************************************************/
/* Internal static functions                                                 */
/*****************************************************************************/

static inline uint64_t ABTI_pool_get_new_id(void)
{
    return (uint64_t)ABTD_atomic_fetch_add_uint64(&g_pool_id, 1);
}
