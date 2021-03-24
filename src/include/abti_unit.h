/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#ifndef ABTI_UNIT_H_INCLUDED
#define ABTI_UNIT_H_INCLUDED

/* A hash table is heavy.  It should be avoided as much as possible. */
#define ABTI_UNIT_BUILTIN_POOL_BIT ((uintptr_t)0x1)

static inline ABT_bool ABTI_unit_is_builtin(ABT_unit unit)
{
    if (((uintptr_t)unit) & ABTI_UNIT_BUILTIN_POOL_BIT) {
        /* This must happen only when unit is associated with a built-in pool.
         * See ABT_pool_def's u_create_from_thread() for details. */
        return ABT_TRUE;
    } else {
        return ABT_FALSE;
    }
}

static inline ABT_unit ABTI_unit_get_builtin_unit(ABTI_thread *p_thread)
{
    ABTI_ASSERT(!(((uintptr_t)p_thread) & ABTI_UNIT_BUILTIN_POOL_BIT));
    return (ABT_unit)(((uintptr_t)p_thread) | ABTI_UNIT_BUILTIN_POOL_BIT);
}

static inline ABTI_thread *ABTI_unit_get_thread_from_builtin_unit(ABT_unit unit)
{
    ABTI_ASSERT(ABTI_unit_is_builtin(unit));
    return (ABTI_thread *)(((uintptr_t)unit) & (~ABTI_UNIT_BUILTIN_POOL_BIT));
}

#endif /* ABTI_UNIT_H_INCLUDED */
