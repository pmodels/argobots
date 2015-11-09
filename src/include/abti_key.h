/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#ifndef KEY_H_INCLUDED
#define KEY_H_INCLUDED

/* Inlined functions for Work unit-specific data key */

static inline
ABTI_key *ABTI_key_get_ptr(ABT_key key)
{
#ifndef ABT_CONFIG_DISABLE_ERROR_CHECK
    ABTI_key *p_key;
    if (key == ABT_KEY_NULL) {
        p_key = NULL;
    } else {
        p_key = (ABTI_key *)key;
    }
    return p_key;
#else
    return (ABTI_key *)key;
#endif
}

static inline
ABT_key ABTI_key_get_handle(ABTI_key *p_key)
{
#ifndef ABT_CONFIG_DISABLE_ERROR_CHECK
    ABT_key h_key;
    if (p_key == NULL) {
        h_key = ABT_KEY_NULL;
    } else {
        h_key = (ABT_key)p_key;
    }
    return h_key;
#else
    return (ABT_key)p_key;
#endif
}

#endif /* KEY_H_INCLUDED */

