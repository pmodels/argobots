/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#ifndef COND_H_INCLUDED
#define COND_H_INCLUDED

/* Inlined functions for Condition Variable  */

static inline
ABTI_cond *ABTI_cond_get_ptr(ABT_cond cond)
{
#ifndef ABT_CONFIG_DISABLE_ERROR_CHECK
    ABTI_cond *p_cond;
    if (cond == ABT_COND_NULL) {
        p_cond = NULL;
    } else {
        p_cond = (ABTI_cond *)cond;
    }
    return p_cond;
#else
    return (ABTI_cond *)cond;
#endif
}

static inline
ABT_cond ABTI_cond_get_handle(ABTI_cond *p_cond)
{
#ifndef ABT_CONFIG_DISABLE_ERROR_CHECK
    ABT_cond h_cond;
    if (p_cond == NULL) {
        h_cond = ABT_COND_NULL;
    } else {
        h_cond = (ABT_cond)p_cond;
    }
    return h_cond;
#else
    return (ABT_cond)p_cond;
#endif
}

#endif /* COND_H_INCLUDED */

