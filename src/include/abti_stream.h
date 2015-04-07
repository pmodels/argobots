/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#ifndef XSTREAM_H_INCLUDED
#define XSTREAM_H_INCLUDED

/* Inlined functions for Execution Stream (ES) */

static inline
ABTI_xstream *ABTI_xstream_get_ptr(ABT_xstream xstream)
{
#ifndef UNSAFE_MODE
    ABTI_xstream *p_xstream;
    if (xstream == ABT_XSTREAM_NULL) {
        p_xstream = NULL;
    } else {
        p_xstream = (ABTI_xstream *)xstream;
    }
    return p_xstream;
#else
    return (ABTI_xstream *)xstream;
#endif
}

static inline
ABT_xstream ABTI_xstream_get_handle(ABTI_xstream *p_xstream)
{
#ifndef UNSAFE_MODE
    ABT_xstream h_xstream;
    if (p_xstream == NULL) {
        h_xstream = ABT_XSTREAM_NULL;
    } else {
        h_xstream = (ABT_xstream)p_xstream;
    }
    return h_xstream;
#else
    return (ABT_xstream)p_xstream;
#endif
}


#endif /* XSTREAM_H_INCLUDED */
