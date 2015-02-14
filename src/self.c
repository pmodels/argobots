/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"


/** @defgroup SELF Self
 * This group is for the self wok unit.
 */


/**
 * @ingroup SELF
 * @brief   Return the type of calling work unit.
 *
 * \c ABT_self_get_type() returns the type of calling work unit, e.g.,
 * \c ABT_UNIT_TYPE_THREAD for ULT and \c ABT_UNIT_TYPE_TASK for tasklet,
 * through \c type.
 * If this routine is called when Argobots has not been initialized, \c type
 * will be set to \c ABT_UNIT_TYPE_EXT, and \c ABT_ERR_UNINITIALIZED will be
 * returned.
 * If this routine is called by an external thread, e.g., pthread, \c type will
 * be set to \c ABT_UNIT_TYPE_EXT, and \c ABT_ERR_INV_XSTREAM will be returned.
 *
 * @param[out] type  work unit type.
 * @return Error code
 * @retval ABT_SUCCESS           on success
 * @retval ABT_ERR_UNINITIALIZED Argobots has not been initialized
 * @retval ABT_ERR_INV_XSTREAM   called by an external thread, e.g., pthread
 */
int ABT_self_get_type(ABT_unit_type *type)
{
    int abt_errno = ABT_SUCCESS;

    /* If Argobots has not been initialized, set type to ABT_UNIT_TYPE_EXIT. */
    if (gp_ABTI_global == NULL) {
        abt_errno = ABT_ERR_UNINITIALIZED;
        *type = ABT_UNIT_TYPE_EXT;
        goto fn_exit;
    }

    /* This is when an external thread called this routine. */
    if (lp_ABTI_local == NULL) {
        abt_errno = ABT_ERR_INV_XSTREAM;
        *type = ABT_UNIT_TYPE_EXT;
        goto fn_exit;
    }

    if (ABTI_local_get_thread() != NULL) {
        *type = ABT_UNIT_TYPE_THREAD;
    } else if (ABTI_local_get_task() != NULL) {
        *type = ABT_UNIT_TYPE_TASK;
    } else {
        abt_errno = ABT_ERR_OTHER;
        goto fn_fail;
    }

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_self_get_type", abt_errno);
    goto fn_exit;
}

