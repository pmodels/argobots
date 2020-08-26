/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#ifndef ABTI_POOL_H_INCLUDED
#define ABTI_POOL_H_INCLUDED

#ifdef ABT_CONFIG_DISABLE_POOL_PRODUCER_CHECK
#define ABTI_IS_POOL_PRODUCER_CHECK_ENABLED 0
#else
#define ABTI_IS_POOL_PRODUCER_CHECK_ENABLED 1
#endif

#ifdef ABT_CONFIG_DISABLE_POOL_CONSUMER_CHECK
#define ABTI_IS_POOL_CONSUMER_CHECK_ENABLED 0
#else
#define ABTI_IS_POOL_CONSUMER_CHECK_ENABLED 1
#endif

/* Inlined functions for Pool */

static inline ABTI_pool *ABTI_pool_get_ptr(ABT_pool pool)
{
#ifndef ABT_CONFIG_DISABLE_ERROR_CHECK
    ABTI_pool *p_pool;
    if (pool == ABT_POOL_NULL) {
        p_pool = NULL;
    } else {
        p_pool = (ABTI_pool *)pool;
    }
    return p_pool;
#else
    return (ABTI_pool *)pool;
#endif
}

static inline ABT_pool ABTI_pool_get_handle(ABTI_pool *p_pool)
{
#ifndef ABT_CONFIG_DISABLE_ERROR_CHECK
    ABT_pool h_pool;
    if (p_pool == NULL) {
        h_pool = ABT_POOL_NULL;
    } else {
        h_pool = (ABT_pool)p_pool;
    }
    return h_pool;
#else
    return (ABT_pool)p_pool;
#endif
}

/* A ULT is blocked and is waiting for going back to this pool */
static inline void ABTI_pool_inc_num_blocked(ABTI_pool *p_pool)
{
    ABTD_atomic_fetch_add_int32(&p_pool->num_blocked, 1);
}

/* A blocked ULT is back in the pool */
static inline void ABTI_pool_dec_num_blocked(ABTI_pool *p_pool)
{
    ABTD_atomic_fetch_sub_int32(&p_pool->num_blocked, 1);
}

/* The pool will receive a migrated ULT */
static inline void ABTI_pool_inc_num_migrations(ABTI_pool *p_pool)
{
    ABTD_atomic_fetch_add_int32(&p_pool->num_migrations, 1);
}

/* The pool has received a migrated ULT */
static inline void ABTI_pool_dec_num_migrations(ABTI_pool *p_pool)
{
    ABTD_atomic_fetch_sub_int32(&p_pool->num_migrations, 1);
}

/* Set the associated consumer ES of a pool. This function has no effect on
 * pools of shared-read access mode. If a pool is private-read to an ES, we
 * check that the previous value of "consumer_id" is the same as the argument of
 * the function "consumer_id"
 * */
#ifndef ABT_CONFIG_DISABLE_POOL_CONSUMER_CHECK
static inline int
ABTI_pool_set_consumer_common_impl(ABTI_pool *p_pool,
                                   ABTI_native_thread_id consumer_id)
{
    switch (p_pool->access) {
        case ABT_POOL_ACCESS_PRIV:
#ifndef ABT_CONFIG_DISABLE_POOL_PRODUCER_CHECK
            ABTI_CHECK_TRUE_RET(!p_pool->producer_id ||
                                    p_pool->producer_id == consumer_id,
                                ABT_ERR_INV_POOL_ACCESS);
#endif
            ABTI_CHECK_TRUE_RET(!p_pool->consumer_id ||
                                    p_pool->consumer_id == consumer_id,
                                ABT_ERR_INV_POOL_ACCESS);
            p_pool->consumer_id = consumer_id;
            break;

        case ABT_POOL_ACCESS_SPSC:
        case ABT_POOL_ACCESS_MPSC:
            ABTI_CHECK_TRUE_RET(!p_pool->consumer_id ||
                                    p_pool->consumer_id == consumer_id,
                                ABT_ERR_INV_POOL_ACCESS);
            /* NB: as we do not want to use a mutex, the function can be wrong
             * here */
            p_pool->consumer_id = consumer_id;
            break;

        case ABT_POOL_ACCESS_SPMC:
        case ABT_POOL_ACCESS_MPMC:
            p_pool->consumer_id = consumer_id;
            break;

        default:
            return ABT_ERR_INV_POOL_ACCESS;
    }
    return ABT_SUCCESS;
}

static inline int ABTI_pool_set_consumer_id(ABTI_pool *p_pool,
                                            ABTI_native_thread_id consumer_id)
{
    if (ABTD_atomic_acquire_load_int32(&p_pool->num_scheds) == 0)
        return ABT_SUCCESS;
    return ABTI_pool_set_consumer_common_impl(p_pool, consumer_id);
}
#endif /* ABT_CONFIG_DISABLE_POOL_CONSUMER_CHECK */

static inline int ABTI_pool_set_consumer_impl(ABTI_local *p_local,
                                              ABTI_pool *p_pool)
{
#ifndef ABT_CONFIG_DISABLE_POOL_CONSUMER_CHECK
    if (ABTD_atomic_acquire_load_int32(&p_pool->num_scheds) == 0)
        return ABT_SUCCESS;
    ABTI_native_thread_id consumer_id = ABTI_self_get_native_thread_id(p_local);
    return ABTI_pool_set_consumer_common_impl(p_pool, consumer_id);
#else
    return ABT_SUCCESS;
#endif
}

#define ABTI_pool_set_consumer(p_local, p_pool)                                \
    ABTI_pool_set_consumer_impl(ABTI_IS_POOL_CONSUMER_CHECK_ENABLED            \
                                    ? (p_local)                                \
                                    : NULL,                                    \
                                p_pool)

/* Set the associated producer ES of a pool. This function has no effect on
 * pools of shared-write access mode. If a pool is private-write to an ES, we
 * check that the previous value of "producer_id" is the same as the argument of
 * the function "producer_id"
 * */
static inline int ABTI_pool_set_producer_impl(ABTI_local *p_local,
                                              ABTI_pool *p_pool)
{
#ifndef ABT_CONFIG_DISABLE_POOL_PRODUCER_CHECK
    if (ABTD_atomic_acquire_load_int32(&p_pool->num_scheds) == 0)
        return ABT_SUCCESS;
    ABTI_native_thread_id producer_id = ABTI_self_get_native_thread_id(p_local);
    switch (p_pool->access) {
        case ABT_POOL_ACCESS_PRIV:
#ifndef ABT_CONFIG_DISABLE_POOL_CONSUMER_CHECK
            ABTI_CHECK_TRUE_RET(!p_pool->consumer_id ||
                                    p_pool->consumer_id == producer_id,
                                ABT_ERR_INV_POOL_ACCESS);
#endif
            ABTI_CHECK_TRUE_RET(!p_pool->producer_id ||
                                    p_pool->producer_id == producer_id,
                                ABT_ERR_INV_POOL_ACCESS);
            p_pool->producer_id = producer_id;
            break;

        case ABT_POOL_ACCESS_SPSC:
        case ABT_POOL_ACCESS_SPMC:
            ABTI_CHECK_TRUE_RET(!p_pool->producer_id ||
                                    p_pool->producer_id == producer_id,
                                ABT_ERR_INV_POOL_ACCESS);
            /* NB: as we do not want to use a mutex, the function can be wrong
             * here */
            p_pool->producer_id = producer_id;
            break;

        case ABT_POOL_ACCESS_MPSC:
        case ABT_POOL_ACCESS_MPMC:
            p_pool->producer_id = producer_id;
            break;

        default:
            return ABT_ERR_INV_POOL_ACCESS;
    }
    return ABT_SUCCESS;
#else
    return ABT_SUCCESS;
#endif
}

#define ABTI_pool_set_producer(p_local, p_pool)                                \
    ABTI_pool_set_producer_impl(ABTI_IS_POOL_PRODUCER_CHECK_ENABLED            \
                                    ? (p_local)                                \
                                    : NULL,                                    \
                                p_pool)

static inline int ABTI_pool_push_impl(ABTI_local *p_local, ABTI_pool *p_pool,
                                      ABT_unit unit)
{
    LOG_DEBUG_POOL_PUSH(p_local, p_pool, unit);

    /* Save the producer ES information in the pool */
    int abt_errno = ABTI_pool_set_producer(p_local, p_pool);
    ABTI_CHECK_ERROR_RET(abt_errno);

    /* Push unit into pool */
    p_pool->p_push(ABTI_pool_get_handle(p_pool), unit);
    return ABT_SUCCESS;
}

#define ABTI_pool_push(p_local, p_pool, unit)                                  \
    ABTI_pool_push_impl(ABTI_IS_POOL_PRODUCER_CHECK_ENABLED ? (p_local)        \
                                                            : NULL,            \
                        p_pool, unit)

static inline int ABTI_pool_add_thread_impl(ABTI_local *p_local,
                                            ABTI_thread *p_thread)
{
    /* Set the ULT's state as READY. The relaxed version is used since the state
     * is synchronized by the following pool operation. */
    ABTD_atomic_relaxed_store_int(&p_thread->state, ABT_THREAD_STATE_READY);

    /* Add the ULT to the associated pool */
    int abt_errno = ABTI_pool_push(p_local, p_thread->p_pool, p_thread->unit);
    ABTI_CHECK_ERROR_RET(abt_errno);
    return ABT_SUCCESS;
}

#define ABTI_pool_add_thread(p_local, p_thread)                                \
    ABTI_pool_add_thread_impl(ABTI_IS_POOL_PRODUCER_CHECK_ENABLED ? (p_local)  \
                                                                  : NULL,      \
                              p_thread)

static inline int ABTI_pool_remove_impl(ABTI_local *p_local, ABTI_pool *p_pool,
                                        ABT_unit unit)
{
    int abt_errno;

    LOG_DEBUG_POOL_REMOVE(p_local, p_pool, unit);

    abt_errno = ABTI_pool_set_consumer(p_local, p_pool);
    ABTI_CHECK_ERROR_RET(abt_errno);

    abt_errno = p_pool->p_remove(ABTI_pool_get_handle(p_pool), unit);
    ABTI_CHECK_ERROR_RET(abt_errno);

    return ABT_SUCCESS;
}

#define ABTI_pool_remove(p_local, p_pool, unit)                                \
    ABTI_pool_remove_impl(ABTI_IS_POOL_CONSUMER_CHECK_ENABLED ? (p_local)      \
                                                              : NULL,          \
                          p_pool, unit)

static inline ABT_unit ABTI_pool_pop_wait(ABTI_pool *p_pool, double time_secs)
{
    ABT_unit unit;

    unit = p_pool->p_pop_wait(ABTI_pool_get_handle(p_pool), time_secs);
    LOG_DEBUG_POOL_POP(p_pool, unit);

    return unit;
}

static inline ABT_unit ABTI_pool_pop_timedwait(ABTI_pool *p_pool,
                                               double abstime_secs)
{
    ABT_unit unit;

    unit = p_pool->p_pop_timedwait(ABTI_pool_get_handle(p_pool), abstime_secs);
    LOG_DEBUG_POOL_POP(p_pool, unit);

    return unit;
}

static inline ABT_unit ABTI_pool_pop(ABTI_pool *p_pool)
{
    ABT_unit unit;

    unit = p_pool->p_pop(ABTI_pool_get_handle(p_pool));
    LOG_DEBUG_POOL_POP(p_pool, unit);

    return unit;
}

/* Increase num_scheds to mark the pool as having another scheduler. If the
 * pool is not available, it returns ABT_ERR_INV_POOL_ACCESS.  */
static inline void ABTI_pool_retain(ABTI_pool *p_pool)
{
    ABTD_atomic_fetch_add_int32(&p_pool->num_scheds, 1);
}

/* Decrease the num_scheds to release this pool from a scheduler. Call when
 * the pool is removed from a scheduler or when it stops. */
static inline int32_t ABTI_pool_release(ABTI_pool *p_pool)
{
    ABTI_ASSERT(ABTD_atomic_acquire_load_int32(&p_pool->num_scheds) > 0);
    return ABTD_atomic_fetch_sub_int32(&p_pool->num_scheds, 1) - 1;
}

static inline size_t ABTI_pool_get_size(ABTI_pool *p_pool)
{
    return p_pool->p_get_size(ABTI_pool_get_handle(p_pool));
}

static inline size_t ABTI_pool_get_total_size(ABTI_pool *p_pool)
{
    size_t total_size;
    total_size = ABTI_pool_get_size(p_pool);
    total_size += ABTD_atomic_acquire_load_int32(&p_pool->num_blocked);
    total_size += ABTD_atomic_acquire_load_int32(&p_pool->num_migrations);
    return total_size;
}

#endif /* ABTI_POOL_H_INCLUDED */
