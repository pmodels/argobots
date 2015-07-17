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

    if (ABTI_local_get_task() != NULL) {
        *type = ABT_UNIT_TYPE_TASK;
    } else {
        /* Since ABTI_local_get_thread() can return NULL during executing
         * ABTI_init(), it should always be safe to say that the type of caller
         * is ULT if the control reaches here. */
        *type = ABT_UNIT_TYPE_THREAD;
    }

  fn_exit:
    return abt_errno;
}

/**
 * @ingroup SELF
 * @brief   Check if the caller is the primary ULT.
 *
 * \c ABT_self_is_primary() confirms whether the caller is the primary ULT and
 * returns the result through \c flag.
 * If the caller is the primary ULT, \c flag is set to \c ABT_TRUE.
 * Otherwise, \c flag is set to \c ABT_FALSE.
 *
 * @param[out] flag    result (<tt>ABT_TRUE</tt>: primary ULT,
 *                     <tt>ABT_FALSE</tt>: not)
 * @return Error code
 * @retval ABT_SUCCESS           on success
 * @retval ABT_ERR_UNINITIALIZED Argobots has not been initialized
 * @retval ABT_ERR_INV_XSTREAM   called by an external thread, e.g., pthread
 * @retval ABT_ERR_INV_THREAD    called by a tasklet
 */
int ABT_self_is_primary(ABT_bool *flag)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_thread *p_thread;

    /* If Argobots has not been initialized, set flag to ABT_FALSE. */
    if (gp_ABTI_global == NULL) {
        abt_errno = ABT_ERR_UNINITIALIZED;
        *flag = ABT_FALSE;
        goto fn_exit;
    }

    /* This is when an external thread called this routine. */
    if (lp_ABTI_local == NULL) {
        abt_errno = ABT_ERR_INV_XSTREAM;
        *flag = ABT_FALSE;
        goto fn_exit;
    }

    p_thread = ABTI_local_get_thread();
    if (p_thread) {
        *flag = (p_thread->type == ABTI_THREAD_TYPE_MAIN)
              ? ABT_TRUE : ABT_FALSE;
    } else {
        abt_errno = ABT_ERR_INV_THREAD;
        *flag = ABT_FALSE;
    }

  fn_exit:
    return abt_errno;
}

/**
 * @ingroup SELF
 * @brief   Check if the caller's ES is the primary ES.
 *
 * \c ABT_self_on_primary_xstream() checks whether the caller work unit is
 * associated with the primary ES. If the caller is running on the primary ES,
 * \c flag is set to \c ABT_TRUE. Otherwise, \c flag is set to \c ABT_FALSE.
 *
 * @param[out] flag    result (<tt>ABT_TRUE</tt>: primary ES,
 *                     <tt>ABT_FALSE</tt>: not)
 * @return Error code
 * @retval ABT_SUCCESS           on success
 * @retval ABT_ERR_UNINITIALIZED Argobots has not been initialized
 * @retval ABT_ERR_INV_XSTREAM   called by an external thread, e.g., pthread
 */
int ABT_self_on_primary_xstream(ABT_bool *flag)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_xstream *p_xstream;

    /* If Argobots has not been initialized, set flag to ABT_FALSE. */
    if (gp_ABTI_global == NULL) {
        abt_errno = ABT_ERR_UNINITIALIZED;
        *flag = ABT_FALSE;
        goto fn_exit;
    }

    /* This is when an external thread called this routine. */
    if (lp_ABTI_local == NULL) {
        abt_errno = ABT_ERR_INV_XSTREAM;
        *flag = ABT_FALSE;
        goto fn_exit;
    }

    p_xstream = ABTI_local_get_xstream();
    ABTI_CHECK_NULL_XSTREAM_PTR(p_xstream);

    /* Return value */
    *flag = (p_xstream->type == ABTI_XSTREAM_TYPE_PRIMARY)
          ? ABT_TRUE : ABT_FALSE;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

