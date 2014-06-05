/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"

int ABTI_Template(ABTI_Stream *p_stream, ABTI_Thread *p_thread, void *p_arg)
{
    int abt_errno = ABT_SUCCESS;

    if (p_stream == NULL) {
        HANDLE_ERROR("NULL STREAM");
        abt_errno = ABT_ERR_INV_STREAM;
        goto fn_fail;
    }

    if (p_thread == NULL) {
        HANDLE_ERROR("NULL THREAD");
        abt_errno = ABT_ERR_INV_THREAD;
        goto fn_fail;
    }

    /* Implementation */
    /* ... */

  fn_exit:
    return abt_errno;

  fn_fail:
    goto fn_exit;
}
