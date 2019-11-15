/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"


/*****************************************************************************/
/* Private APIs                                                              */
/*****************************************************************************/

static ABTI_local *ABTI_local_get_local_internal(void)
{
    return lp_ABTI_local;
}

static void ABTI_local_set_local_internal(ABTI_local *p_local)
{
    lp_ABTI_local = p_local;
}

ABTI_local_func gp_ABTI_local_func = {
    {0},
    ABTI_local_get_local_internal,
    ABTI_local_set_local_internal,
    {0}
};
/* ES Local Data */
ABTD_XSTREAM_LOCAL ABTI_local *lp_ABTI_local = NULL;

int ABTI_local_init(ABTI_local **pp_local)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_CHECK_TRUE(lp_ABTI_local == NULL, ABT_ERR_OTHER);

    ABTI_local *p_local = (ABTI_local *)ABTU_malloc(sizeof(ABTI_local));
    p_local->p_xstream = NULL;
    p_local->p_thread = NULL;
    p_local->p_task = NULL;
    lp_ABTI_local = p_local;

    ABTI_mem_init_local(p_local);

    ABTI_LOG_INIT();
    *pp_local = p_local;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

int ABTI_local_finalize(ABTI_local **pp_local)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_local *p_local = *pp_local;
    ABTI_CHECK_TRUE(p_local != NULL, ABT_ERR_OTHER);
    ABTI_mem_finalize_local(p_local);
    ABTU_free(p_local);
    lp_ABTI_local = NULL;
    *pp_local = NULL;

    ABTI_LOG_FINALIZE();

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

