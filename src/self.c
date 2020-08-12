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
 * If this routine is called when Argobots has not been initialized,
 * \c ABT_ERR_UNINITIALIZED will be returned.
 * If this routine is called by an external thread, e.g., pthread,
 * \c ABT_ERR_INV_XSTREAM will be returned.
 *
 * Now \c type will be set to \c ABT_UNIT_TYPE_EXT when it returns an
 * error, but this behavior is deprecated; the user should check the error code
 * to check if this routine is called by a thread not managed by Argobots.
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

    ABTI_xstream *p_local_xstream = ABTI_local_get_xstream();
    ABTI_thread_type raw_type = ABTI_self_get_type(p_local_xstream);
    *type = ABTI_thread_type_get_type(raw_type);
#ifndef ABT_CONFIG_DISABLE_EXT_THREAD
    /* This is when an external thread called this routine. */
    if (*type == ABT_UNIT_TYPE_EXT) {
        abt_errno = ABT_ERR_INV_XSTREAM;
        goto fn_exit;
    }
#endif

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
    ABTI_xstream *p_local_xstream = ABTI_local_get_xstream();

    /* If Argobots has not been initialized, set flag to ABT_FALSE. */
    if (gp_ABTI_global == NULL) {
        abt_errno = ABT_ERR_UNINITIALIZED;
        *flag = ABT_FALSE;
        goto fn_exit;
    }

#ifndef ABT_CONFIG_DISABLE_EXT_THREAD
    /* This is when an external thread called this routine. */
    if (p_local_xstream == NULL) {
        abt_errno = ABT_ERR_INV_XSTREAM;
        *flag = ABT_FALSE;
        goto fn_exit;
    }
#endif

    ABTI_thread *p_thread = p_local_xstream->p_thread;
    if (p_thread->type == ABTI_THREAD_TYPE_THREAD_MAIN) {
        *flag = ABT_TRUE;
    } else {
        if (!ABTI_thread_type_is_thread(p_thread->type))
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
    ABTI_xstream *p_local_xstream = ABTI_local_get_xstream();

    /* If Argobots has not been initialized, set flag to ABT_FALSE. */
    if (gp_ABTI_global == NULL) {
        abt_errno = ABT_ERR_UNINITIALIZED;
        *flag = ABT_FALSE;
        goto fn_exit;
    }

#ifndef ABT_CONFIG_DISABLE_EXT_THREAD
    /* This is when an external thread called this routine. */
    if (p_local_xstream == NULL) {
        abt_errno = ABT_ERR_INV_XSTREAM;
        *flag = ABT_FALSE;
        goto fn_exit;
    }
#endif

    /* Return value */
    *flag = (p_local_xstream->type == ABTI_XSTREAM_TYPE_PRIMARY) ? ABT_TRUE
                                                                 : ABT_FALSE;

fn_exit:
    return abt_errno;
}

/**
 * @ingroup SELF
 * @brief   Get the last pool's ID of calling work unit.
 *
 * \c ABT_self_get_last_pool_id() returns the last pool's ID of caller work
 * unit.  If the work unit is not running, this routine returns the ID of the
 * pool where it is residing.  Otherwise, it returns the ID of the last pool
 * where the work unit was (i.e., the pool from which the work unit was
 * popped).
 * NOTE: If this routine is not called by Argobots work unit (ULT or tasklet),
 * \c pool_id will be set to \c -1.
 *
 * @param[out] pool_id  pool id
 * @return Error code
 * @retval ABT_SUCCESS           on success
 * @retval ABT_ERR_UNINITIALIZED Argobots has not been initialized
 * @retval ABT_ERR_INV_XSTREAM   called by an external thread, e.g., pthread
 */
int ABT_self_get_last_pool_id(int *pool_id)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_xstream *p_local_xstream = ABTI_local_get_xstream();

    if (gp_ABTI_global == NULL) {
        /* Argobots has not been initialized. */
        abt_errno = ABT_ERR_UNINITIALIZED;
        *pool_id = -1;
        goto fn_exit;
    }

#ifndef ABT_CONFIG_DISABLE_EXT_THREAD
    /* This is when an external thread called this routine. */
    if (p_local_xstream == NULL) {
        abt_errno = ABT_ERR_INV_XSTREAM;
        *pool_id = -1;
        goto fn_exit;
    }
#endif

    ABTI_thread *p_self = p_local_xstream->p_thread;
    ABTI_ASSERT(p_self->p_pool);
    *pool_id = (int)(p_self->p_pool->id);

fn_exit:
    return abt_errno;
}

/**
 * @ingroup SELF
 * @brief   Suspend the current ULT.
 *
 * \c ABT_self_suspend() suspends the execution of current ULT and switches
 * to the scheduler.  The caller ULT is not pushed to its associated pool and
 * its state becomes BLOCKED.  It can be awakened and be pushed back to the
 * pool when \c ABT_thread_resume() is called.
 *
 * This routine must be called by a ULT.  Otherwise, it returns
 * \c ABT_ERR_INV_THREAD without suspending the caller.
 *
 * @return Error code
 * @retval ABT_SUCCESS          on success
 * @retval ABT_ERR_INV_THREAD   called by a non-ULT
 */
int ABT_self_suspend(void)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_xstream *p_local_xstream = ABTI_local_get_xstream();

#ifndef ABT_CONFIG_DISABLE_EXT_THREAD
    /* This is when an external thread called this routine. */
    if (p_local_xstream == NULL) {
        abt_errno = ABT_ERR_INV_THREAD;
        goto fn_exit;
    }
#endif

    ABTI_thread *p_self = p_local_xstream->p_thread;
    ABTI_CHECK_TRUE(ABTI_thread_type_is_thread(p_self->type),
                    ABT_ERR_INV_THREAD);
    abt_errno = ABTI_thread_set_blocked(p_self);
    ABTI_CHECK_ERROR(abt_errno);

    ABTI_thread_suspend(&p_local_xstream, p_self, ABT_SYNC_EVENT_TYPE_USER,
                        NULL);

fn_exit:
    return abt_errno;

fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

/**
 * @ingroup SELF
 * @brief   Set the argument for the work unit function
 *
 * \c ABT_self_set_arg() sets the argument for the caller's work unit
 * function.
 *
 * @param[in] arg  argument for the work unit function
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_self_set_arg(void *arg)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_xstream *p_local_xstream = ABTI_local_get_xstream();

    /* When Argobots has not been initialized */
    if (gp_ABTI_global == NULL) {
        abt_errno = ABT_ERR_UNINITIALIZED;
        goto fn_exit;
    }

#ifndef ABT_CONFIG_DISABLE_EXT_THREAD
    /* When an external thread called this routine */
    if (p_local_xstream == NULL) {
        abt_errno = ABT_ERR_INV_XSTREAM;
        goto fn_exit;
    }
#endif

    p_local_xstream->p_thread->p_arg = arg;

fn_exit:
    return abt_errno;
}

/**
 * @ingroup SELF
 * @brief   Retrieve the argument for the work unit function
 *
 * \c ABT_self_get_arg() returns the argument for the caller's work unit
 * function.  If the caller is a ULT, this routine returns the function argument
 * passed to \c ABT_thread_create() when the caller was created or set by \c
 * ABT_thread_set_arg().  On the other hand, if the caller is a tasklet, this
 * routine returns the function argument passed to \c ABT_task_create().
 *
 * @param[out] arg  argument for the work unit function
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_self_get_arg(void **arg)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_xstream *p_local_xstream = ABTI_local_get_xstream();

    /* When Argobots has not been initialized */
    if (gp_ABTI_global == NULL) {
        abt_errno = ABT_ERR_UNINITIALIZED;
        *arg = NULL;
        goto fn_exit;
    }

#ifndef ABT_CONFIG_DISABLE_EXT_THREAD
    /* When an external thread called this routine */
    if (p_local_xstream == NULL) {
        abt_errno = ABT_ERR_INV_XSTREAM;
        *arg = NULL;
        goto fn_exit;
    }
#endif

    *arg = p_local_xstream->p_thread->p_arg;

fn_exit:
    return abt_errno;
}

/**
 * @ingroup SELF
 * @brief   Check if the running work unit is unnamed
 *
 * \c ABT_self_is_unnamed() returns whether the current work units is unnamed or
 * not.  If the caller is an external thread, it sets ABT_FALSE and returns
 * ABT_ERR_INV_XSTREAM.
 *
 * @param[out] flag  result (<tt>ABT_TRUE</tt> if unnamed)
 *
 * @return Error code
 * @retval ABT_SUCCESS  on success
 */
int ABT_self_is_unnamed(ABT_bool *flag)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_xstream *p_local_xstream = ABTI_local_get_xstream();

#ifndef ABT_CONFIG_DISABLE_EXT_THREAD
    /* When an external thread called this routine */
    if (p_local_xstream == NULL) {
        abt_errno = ABT_ERR_INV_XSTREAM;
        *flag = ABT_FALSE;
        goto fn_exit;
    }
#endif

    *flag = (p_local_xstream->p_thread->refcount == 0) ? ABT_TRUE : ABT_FALSE;

#ifndef ABT_CONFIG_DISABLE_EXT_THREAD
fn_exit:
#endif
    return abt_errno;
}
