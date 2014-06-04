/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"

/* ES Local Data */
ABTD_STREAM_LOCAL ABTI_Local *lp_ABTI_Local = NULL;

int ABTI_Local_init(ABTI_Stream *p_stream)
{
    assert(lp_ABTI_Local == NULL);
    int abt_errno = ABT_SUCCESS;

    lp_ABTI_Local = (ABTI_Local *)ABTU_Malloc(sizeof(ABTI_Local));
    if (!lp_ABTI_Local) {
        HANDLE_ERROR("ABTU_Malloc");
        abt_errno = ABT_ERR_MEM;
        goto fn_fail;
    }

    lp_ABTI_Local->p_stream = p_stream;
    lp_ABTI_Local->p_thread = NULL;

  fn_exit:
    return abt_errno;

  fn_fail:
    goto fn_exit;
}

int ABTI_Local_finalize()
{
    assert(lp_ABTI_Local != NULL);
    int abt_errno = ABT_SUCCESS;
    ABTU_Free(lp_ABTI_Local);
    lp_ABTI_Local = NULL;
    return abt_errno;
}

