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
 * @brief   Obtain a type of the caller.
 *
 * \c ABT_self_get_type() returns a type of the calling work unit through
 * \c type.  If the caller is a ULT, \c type is set to \c ABT_UNIT_TYPE_THREAD.
 * If the caller is a tasklet, \c type is set to \c ABT_UNIT_TYPE_TASK.
 * Otherwise (i.e., if the caller is an external thread), \c type is set to
 * \c ABT_UNIT_TYPE_EXT.
 *
 * @changev20
 * \DOC_DESC_V1X_NOEXT{\c ABT_ERR_INV_XSTREAM}
 *
 * \DOC_DESC_V1X_RETURN_UNINITIALIZED
 *
 * \DOC_DESC_V1X_SET_VALUE_ON_ERROR{\c type, \c ABT_UNIT_TYPE_EXT}
 * @endchangev20
 *
 * @contexts
 * \DOC_V1X \DOC_CONTEXT_INIT_NOEXT \DOC_CONTEXT_NOCTXSWITCH\n
 * \DOC_V20 \DOC_CONTEXT_INIT \DOC_CONTEXT_NOCTXSWITCH
 *
 * @errors
 * \DOC_ERROR_SUCCESS
 * \DOC_V1X \DOC_ERROR_UNINITIALIZED
 * \DOC_V1X \DOC_ERROR_INV_XSTREAM_EXT
 *
 * @undefined
 * \DOC_UNDEFINED_NULL_PTR{\c type}
 * \DOC_V20 \DOC_UNDEFINED_UNINIT
 *
 * @param[out] type  work unit type
 * @return Error code
 */
int ABT_self_get_type(ABT_unit_type *type)
{
#ifndef ABT_CONFIG_ENABLE_VER_20_API
    /* By default, type is ABT_UNIT_TYPE_EXT in Argobots 1.x */
    *type = ABT_UNIT_TYPE_EXT;
    ABTI_xstream *p_local_xstream;
    ABTI_SETUP_LOCAL_XSTREAM_WITH_INIT_CHECK(&p_local_xstream);
    *type = ABTI_thread_type_get_type(p_local_xstream->p_thread->type);
#else
    ABTI_xstream *p_local_xstream =
        ABTI_local_get_xstream_or_null(ABTI_local_get_local());
    if (p_local_xstream) {
        *type = ABTI_thread_type_get_type(p_local_xstream->p_thread->type);
    } else {
        *type = ABT_UNIT_TYPE_EXT;
    }
#endif
    return ABT_SUCCESS;
}

/**
 * @ingroup SELF
 * @brief   Check if the caller is the primary ULT.
 *
 * \c ABT_self_is_primary() checks whether the caller is the primary ULT and
 * returns the result through \c is_primary.  If the caller is the primary ULT,
 * \c is_primary is set to \c ABT_TRUE.  Otherwise, \c is_primary is set to
 * \c ABT_FALSE.
 *
 * @changev20
 * \DOC_DESC_V1X_NOTASK{\c ABT_ERR_INV_THREAD}
 *
 * \DOC_DESC_V1X_NOEXT{\c ABT_ERR_INV_XSTREAM}
 *
 * \DOC_DESC_V1X_RETURN_UNINITIALIZED
 *
 * \DOC_DESC_V1X_SET_VALUE_ON_ERROR{\c is_primary, \c ABT_FALSE}
 * @endchangev20
 *
 * @contexts
 * \DOC_V1X \DOC_CONTEXT_INIT_YIELDABLE \DOC_CONTEXT_NOCTXSWITCH\n
 * \DOC_V20 \DOC_CONTEXT_INIT \DOC_CONTEXT_NOCTXSWITCH
 *
 * @errors
 * \DOC_ERROR_SUCCESS
 * \DOC_V1X \DOC_ERROR_UNINITIALIZED
 * \DOC_V1X \DOC_ERROR_INV_XSTREAM_EXT
 * \DOC_V1X \DOC_ERROR_INV_THREAD_NY
 *
 * @undefined
 * \DOC_UNDEFINED_NULL_PTR{\c is_primary}
 * \DOC_V20 \DOC_UNDEFINED_UNINIT
 *
 * @param[out] is_primary  result (\c ABT_TRUE: primary ULT, \c ABT_FALSE: not)
 * @return Error code
 */
int ABT_self_is_primary(ABT_bool *is_primary)
{
#ifndef ABT_CONFIG_ENABLE_VER_20_API
    *is_primary = ABT_FALSE;
    ABTI_ythread *p_ythread;
    ABTI_SETUP_LOCAL_YTHREAD_WITH_INIT_CHECK(NULL, &p_ythread);
    *is_primary =
        (p_ythread->thread.type & ABTI_THREAD_TYPE_MAIN) ? ABT_TRUE : ABT_FALSE;
#else
    ABTI_xstream *p_local_xstream =
        ABTI_local_get_xstream_or_null(ABTI_local_get_local());
    if (p_local_xstream) {
        *is_primary = (p_local_xstream->p_thread->type & ABTI_THREAD_TYPE_MAIN)
                          ? ABT_TRUE
                          : ABT_FALSE;
    } else {
        *is_primary = ABT_FALSE;
    }
#endif
    return ABT_SUCCESS;
}

/**
 * @ingroup SELF
 * @brief   Check if the caller is running on the primary execution stream.
 *
 * \c ABT_self_on_primary_xstream() checks whether the caller is running on the
 * primary execution stream and returns the result through \c on_primary.  If
 * the caller is a work unit running on the primary execution stream,
 * \c on_primary is set to \c ABT_TRUE.  Otherwise, \c on_primary is set to
 * \c ABT_FALSE.
 *
 * @changev20
 * \DOC_DESC_V1X_NOEXT{\c ABT_ERR_INV_XSTREAM}
 *
 * \DOC_DESC_V1X_RETURN_UNINITIALIZED
 *
 * \DOC_DESC_V1X_SET_VALUE_ON_ERROR{\c on_primary, \c ABT_FALSE}
 * @endchangev20
 *
 * @contexts
 * \DOC_V1X \DOC_CONTEXT_INIT_NOEXT \DOC_CONTEXT_NOCTXSWITCH\n
 * \DOC_V20 \DOC_CONTEXT_INIT \DOC_CONTEXT_NOCTXSWITCH
 *
 * @errors
 * \DOC_ERROR_SUCCESS
 * \DOC_V1X \DOC_ERROR_UNINITIALIZED
 * \DOC_V1X \DOC_ERROR_INV_XSTREAM_EXT
 *
 * @undefined
 * \DOC_UNDEFINED_NULL_PTR{\c on_primary}
 * \DOC_V20 \DOC_UNDEFINED_UNINIT
 *
 * @param[out] on_primary  result (\c ABT_TRUE: primary execution stream,
 *                                 \c ABT_FALSE: not)
 * @return Error code
 */
int ABT_self_on_primary_xstream(ABT_bool *on_primary)
{
#ifndef ABT_CONFIG_ENABLE_VER_20_API
    *on_primary = ABT_FALSE;
    ABTI_xstream *p_local_xstream;
    ABTI_SETUP_LOCAL_XSTREAM_WITH_INIT_CHECK(&p_local_xstream);
    *on_primary = (p_local_xstream->type == ABTI_XSTREAM_TYPE_PRIMARY)
                      ? ABT_TRUE
                      : ABT_FALSE;
#else
    ABTI_xstream *p_local_xstream =
        ABTI_local_get_xstream_or_null(ABTI_local_get_local());
    if (p_local_xstream) {
        *on_primary = (p_local_xstream->type == ABTI_XSTREAM_TYPE_PRIMARY)
                          ? ABT_TRUE
                          : ABT_FALSE;
    } else {
        *on_primary = ABT_FALSE;
    }
#endif
    return ABT_SUCCESS;
}

/**
 * @ingroup SELF
 * @brief   Get ID of the last pool of the calling work unit.
 *
 * \c ABT_self_get_last_pool_id() returns the last pool's ID of the calling work
 * unit through \c pool_id.
 *
 * @changev20
 * \DOC_DESC_V1X_RETURN_UNINITIALIZED
 *
 * \DOC_DESC_V1X_SET_VALUE_ON_ERROR{\c pool_id, -1}
 * @endchangev20
 *
 * @contexts
 * \DOC_CONTEXT_INIT_NOEXT \DOC_CONTEXT_NOCTXSWITCH
 *
 * @errors
 * \DOC_ERROR_SUCCESS
 * \DOC_ERROR_INV_XSTREAM_EXT
 * \DOC_V1X \DOC_ERROR_UNINITIALIZED
 *
 * @undefined
 * \DOC_UNDEFINED_NULL_PTR{\c pool_id}
 * \DOC_V20 \DOC_UNDEFINED_UNINIT
 *
 * @param[out] pool_id  pool ID
 * @return  Error code
 */
int ABT_self_get_last_pool_id(int *pool_id)
{
    ABTI_xstream *p_local_xstream;
#ifndef ABT_CONFIG_ENABLE_VER_20_API
    *pool_id = -1;
    ABTI_SETUP_LOCAL_XSTREAM_WITH_INIT_CHECK(&p_local_xstream);
    ABTI_thread *p_self = p_local_xstream->p_thread;
    ABTI_ASSERT(p_self->p_pool);
    *pool_id = p_self->p_pool->id;
#else
    ABTI_SETUP_LOCAL_XSTREAM(&p_local_xstream);
    ABTI_thread *p_self = p_local_xstream->p_thread;
    ABTI_ASSERT(p_self->p_pool);
    *pool_id = p_self->p_pool->id;
#endif
    return ABT_SUCCESS;
}

/**
 * @ingroup SELF
 * @brief   Suspend the calling ULT.
 *
 * \c ABT_self_suspend() suspends the execution of the calling ULT and switches
 * to its parent ULT.  The caller ULT is not pushed to its associated pool and
 * its state becomes blocked.  The suspended ULT can be awakened and pushed back
 * to its associated pool when \c ABT_thread_resume() is called.
 *
 * @changev11
 * \DOC_DESC_V10_ERROR_CODE_CHANGE{\c ABT_ERR_INV_THREAD,
 *                                 \c ABT_ERR_INV_XSTREAM,
 *                                 this routine is called by an external thread}
 * @endchangev11
 *
 * @contexts
 * \DOC_CONTEXT_INIT_YIELDABLE \DOC_CONTEXT_CTXSWITCH
 *
 * @errors
 * \DOC_ERROR_SUCCESS
 * \DOC_ERROR_INV_XSTREAM_EXT
 * \DOC_ERROR_INV_THREAD_NY
 *
 * @undefined
 * \DOC_UNDEFINED_UNINIT
 * \DOC_UNDEFINED_THREAD_UNSAFE{the caller}
 *
 * @return Error code
 */
int ABT_self_suspend(void)
{
    ABTI_xstream *p_local_xstream;
    ABTI_ythread *p_self;
    ABTI_SETUP_LOCAL_YTHREAD(&p_local_xstream, &p_self);

    ABTI_ythread_set_blocked(p_self);
    ABTI_ythread_suspend(&p_local_xstream, p_self, ABT_SYNC_EVENT_TYPE_USER,
                         NULL);
    return ABT_SUCCESS;
}

/**
 * @ingroup SELF
 * @brief   Set an argument for a work-unit function of the calling work unit
 *
 * \c ABT_self_set_arg() sets the argument \c arg for the caller's work unit
 * function.
 *
 * @note
 * The newly set argument will be used if the caller is revived.
 *
 * @changev20
 * \DOC_DESC_V1X_RETURN_UNINITIALIZED
 * @endchangev20
 *
 * @contexts
 * \DOC_CONTEXT_INIT_NOEXT \DOC_CONTEXT_NOCTXSWITCH
 *
 * @errors
 * \DOC_ERROR_SUCCESS
 * \DOC_ERROR_INV_XSTREAM_EXT
 * \DOC_V1X \DOC_ERROR_UNINITIALIZED
 *
 * @undefined
 * \DOC_UNDEFINED_THREAD_UNSAFE{the caller}
 * \DOC_V20 \DOC_UNDEFINED_UNINIT
 *
 * @param[in] arg  argument a work-unit function of the calling work unit
 * @return Error code
 */
int ABT_self_set_arg(void *arg)
{
    ABTI_xstream *p_local_xstream;
#ifndef ABT_CONFIG_ENABLE_VER_20_API
    ABTI_SETUP_LOCAL_XSTREAM_WITH_INIT_CHECK(&p_local_xstream);
#else
    ABTI_SETUP_LOCAL_XSTREAM(&p_local_xstream);
#endif

    p_local_xstream->p_thread->p_arg = arg;
    return ABT_SUCCESS;
}

/**
 * @ingroup SELF
 * @brief   Retrieve an argument for a work unit function of the calling work
 *          unit
 *
 * \c ABT_self_get_arg() returns the argument for the caller's work unit
 * function.
 *
 * @changev20
 * \DOC_DESC_V1X_RETURN_UNINITIALIZED
 *
 * \DOC_DESC_V1X_SET_VALUE_ON_ERROR{\c arg, \c NULL}
 * @endchangev20
 *
 * @contexts
 * \DOC_CONTEXT_INIT_NOEXT \DOC_CONTEXT_NOCTXSWITCH
 *
 * @errors
 * \DOC_ERROR_SUCCESS
 * \DOC_ERROR_INV_XSTREAM_EXT
 * \DOC_V1X \DOC_ERROR_UNINITIALIZED
 *
 * @undefined
 * \DOC_UNDEFINED_NULL_PTR{\c arg}
 * \DOC_V20 \DOC_UNDEFINED_UNINIT
 *
 * @param[out] arg  argument for the caller's function
 * @return Error code
 */
int ABT_self_get_arg(void **arg)
{
    ABTI_xstream *p_local_xstream;
#ifndef ABT_CONFIG_ENABLE_VER_20_API
    *arg = NULL;
    ABTI_SETUP_LOCAL_XSTREAM_WITH_INIT_CHECK(&p_local_xstream);
#else
    ABTI_SETUP_LOCAL_XSTREAM(&p_local_xstream);
#endif

    *arg = p_local_xstream->p_thread->p_arg;
    return ABT_SUCCESS;
}

/**
 * @ingroup SELF
 * @brief   Check if the calling work unit is unnamed
 *
 * \c ABT_self_is_unnamed() checks if the current caller is unnamed and returns
 * the result through \c is_unnamed.  \c is_unnamed is set to \c ABT_TRUE if the
 * calling work unit is unnamed.  Otherwise, \c is_unnamed is set to \c
 * ABT_FALSE.
 *
 * @contexts
 * \DOC_CONTEXT_INIT_NOEXT \DOC_CONTEXT_NOCTXSWITCH
 *
 * @errors
 * \DOC_ERROR_SUCCESS
 * \DOC_ERROR_INV_XSTREAM_EXT
 *
 * @undefined
 * \DOC_UNDEFINED_UNINIT
 * \DOC_UNDEFINED_NULL_PTR{\c is_unnamed}
 *
 * @param[out] is_unnamed  result (\c ABT_TRUE: unnamed, \c ABT_FALSE: not)
 * @return Error code
 */
int ABT_self_is_unnamed(ABT_bool *is_unnamed)
{
    ABTI_xstream *p_local_xstream;
    ABTI_SETUP_LOCAL_XSTREAM(&p_local_xstream);

    *is_unnamed = (p_local_xstream->p_thread->type & ABTI_THREAD_TYPE_NAMED)
                      ? ABT_FALSE
                      : ABT_TRUE;
    return ABT_SUCCESS;
}
