/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#ifndef POOL_H_INCLUDED
#define POOL_H_INCLUDED

/* Inlined functions for Pool */

static inline
ABTI_pool *ABTI_pool_get_ptr(ABT_pool pool)
{
#ifndef UNSAFE_MODE
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

static inline
ABT_pool ABTI_pool_get_handle(ABTI_pool *p_pool)
{
#ifndef UNSAFE_MODE
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
static inline
void ABTI_pool_inc_num_blocked(ABTI_pool *p_pool)
{
    ABTD_atomic_fetch_add_uint32(&p_pool->num_blocked, 1);
}

/* A blocked ULT is back in the pool */
static inline
void ABTI_pool_dec_num_blocked(ABTI_pool *p_pool)
{
    ABTD_atomic_fetch_sub_uint32(&p_pool->num_blocked, 1);
}

/* The pool will receive a migrated ULT */
static inline
void ABTI_pool_inc_num_migrations(ABTI_pool *p_pool)
{
    ABTD_atomic_fetch_add_int32(&p_pool->num_migrations, 1);
}

/* The pool has received a migrated ULT */
static inline
void ABTI_pool_dec_num_migrations(ABTI_pool *p_pool)
{
    ABTD_atomic_fetch_sub_int32(&p_pool->num_migrations, 1);
}

static inline
int ABTI_pool_push(ABTI_pool *p_pool, ABT_unit unit, ABTI_xstream *p_writer)
{
    int abt_errno = ABT_SUCCESS;

    /* Save the writer ES information in the pool */
    abt_errno = ABTI_pool_set_writer(p_pool, p_writer);
    ABTI_CHECK_ERROR(abt_errno);

    /* Push unit into pool */
    p_pool->p_push(ABTI_pool_get_handle(p_pool), unit);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABTI_pool_push", abt_errno);
    goto fn_exit;
}

static inline
int ABTI_pool_remove(ABTI_pool *p_pool, ABT_unit unit, ABTI_xstream *p_reader)
{
    int abt_errno = ABT_SUCCESS;

    abt_errno = ABTI_pool_set_reader(p_pool, p_reader);
    ABTI_CHECK_ERROR(abt_errno);

    abt_errno = p_pool->p_remove(ABTI_pool_get_handle(p_pool), unit);
    ABTI_CHECK_ERROR(abt_errno);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABTI_pool_remove", abt_errno);
    goto fn_exit;
}

static inline
ABT_unit ABTI_pool_pop(ABTI_pool *p_pool)
{
    return p_pool->p_pop(ABTI_pool_get_handle(p_pool));
}

/* Increase num_scheds to mark the pool as having another scheduler. If the
 * pool is not available, it returns ABT_ERR_INV_POOL_ACCESS.  */
static inline
void ABTI_pool_retain(ABTI_pool *p_pool)
{
    ABTD_atomic_fetch_add_int32(&p_pool->num_scheds, 1);
}

/* Decrease the num_scheds to realease this pool from a scheduler. Call when
 * the pool is removed from a scheduler or when it stops. */
static inline
void ABTI_pool_release(ABTI_pool *p_pool)
{
    assert(p_pool->num_scheds > 0);
    ABTD_atomic_fetch_sub_int32(&p_pool->num_scheds, 1);
}

#endif /* POOL_H_INCLUDED */

