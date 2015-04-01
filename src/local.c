/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"


/*****************************************************************************/
/* Private APIs                                                              */
/*****************************************************************************/

/* ES Local Data */
ABTD_XSTREAM_LOCAL ABTI_local *lp_ABTI_local = NULL;

int ABTI_local_init(void)
{
    assert(lp_ABTI_local == NULL);
    int abt_errno = ABT_SUCCESS;

    lp_ABTI_local = (ABTI_local *)ABTU_malloc(sizeof(ABTI_local));
    lp_ABTI_local->p_xstream = NULL;
    lp_ABTI_local->p_thread_main = NULL;
    lp_ABTI_local->p_thread = NULL;
    lp_ABTI_local->p_task = NULL;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABTI_local_init", abt_errno);
    goto fn_exit;
}

int ABTI_local_finalize(void)
{
    assert(lp_ABTI_local != NULL);
    int abt_errno = ABT_SUCCESS;
    ABTU_free(lp_ABTI_local);
    lp_ABTI_local = NULL;
    return abt_errno;
}

