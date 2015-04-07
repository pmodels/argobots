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

#endif /* POOL_H_INCLUDED */

