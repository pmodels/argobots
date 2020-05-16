/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"

/*****************************************************************************/
/* Private APIs                                                              */
/*****************************************************************************/

static ABTI_xstream *ABTI_local_get_xstream_internal(void)
{
    return lp_ABTI_xstream;
}

static void ABTI_local_set_xstream_internal(ABTI_xstream *p_local_xstream)
{
    lp_ABTI_xstream = p_local_xstream;
}

static void *ABTI_local_get_local_ptr_internal(void)
{
    return (void *)&lp_ABTI_xstream;
}

ABTI_local_func gp_ABTI_local_func = { { 0 },
                                       ABTI_local_get_xstream_internal,
                                       ABTI_local_set_xstream_internal,
                                       ABTI_local_get_local_ptr_internal,
                                       { 0 } };
/* ES Local Data */
ABTD_XSTREAM_LOCAL ABTI_xstream *lp_ABTI_xstream = NULL;
