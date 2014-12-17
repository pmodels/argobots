/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"


/** @defgroup ULT_ATTR ULT Attributes
 * Attributes are used to specify ULT behavior that is different from the
 * default. When a ULT is created with \c ABT_thread_create(), attributes
 * can be specified with an \c ABT_thread_attr object.
 */

/**
 * @ingroup ULT_ATTR
 * @brief   Create a new ULT attribute object.
 *
 * \c ABT_thread_attr_create() creates a ULT attribute object with default
 * attribute values. The handle to the attribute object is returned through
 * \c newattr. The attribute object can be used in more than one ULT.
 *
 * @param[out] newattr  handle to a new attribute object
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_thread_attr_create(ABT_thread_attr *newattr)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_thread_attr *p_newattr;

    p_newattr = (ABTI_thread_attr *)ABTU_malloc(sizeof(ABTI_thread_attr));
    if (!p_newattr) {
        HANDLE_ERROR("ABTU_malloc");
        *newattr = ABT_THREAD_ATTR_NULL;
        abt_errno = ABT_ERR_MEM;
        goto fn_fail;
    }

    /* Default values */
    p_newattr->stacksize  = ABTI_THREAD_DEFAULT_STACKSIZE;
    p_newattr->prio       = ABT_SCHED_PRIO_NORMAL;
    p_newattr->migratable = ABT_TRUE;
    p_newattr->f_cb       = NULL;
    p_newattr->p_cb_arg   = NULL;

    /* Return value */
    *newattr = ABTI_thread_attr_get_handle(p_newattr);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_thread_attr_create", abt_errno);
    goto fn_exit;
}

/**
 * @ingroup ULT_ATTR
 * @brief   Free the ULT attribute object.
 *
 * \c ABT_thread_attr_free() deallocates memory used for the ULT attribute
 * object. If this function successfully returns, \c attr will be set to
 * \c ABT_THREAD_ATTR_NULL.
 *
 * @param[in,out] attr  handle to the target attribute object
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_thread_attr_free(ABT_thread_attr *attr)
{
    int abt_errno = ABT_SUCCESS;
    ABT_thread_attr h_attr = *attr;
    ABTI_thread_attr *p_attr = ABTI_thread_attr_get_ptr(h_attr);
    ABTI_CHECK_NULL_THREAD_ATTR_PTR(p_attr);

    /* Free the memory */
    ABTU_free(p_attr);

    /* Return value */
    *attr = ABT_THREAD_ATTR_NULL;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_thread_attr_free", abt_errno);
    goto fn_exit;
}

/**
 * @ingroup ULT_ATTR
 * @brief   Set the stack size in the attribute object.
 *
 * \c ABT_thread_attr_set_stacksize() sets the stack size (in bytes) in the
 * attribute object associated with handle \c attr.
 *
 * @param[in] attr       handle to the target attribute object
 * @param[in] stacksize  stack size in bytes
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_thread_attr_set_stacksize(ABT_thread_attr attr, size_t stacksize)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_thread_attr *p_attr = ABTI_thread_attr_get_ptr(attr);
    ABTI_CHECK_NULL_THREAD_ATTR_PTR(p_attr);

    /* Set the value */
    p_attr->stacksize = stacksize;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_thread_attr_set_stacksize", abt_errno);
    goto fn_exit;
}

/**
 * @ingroup ULT_ATTR
 * @brief   Get the stack size from the attribute object.
 *
 * \c ABT_thread_attr_get_stacksize() returns the stack size (in bytes) through
 * \c stacksize from the attribute object associated with handle \c attr.
 *
 * @param[in]  attr       handle to the target attribute object
 * @param[out] stacksize  stack size in bytes
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_thread_attr_get_stacksize(ABT_thread_attr attr, size_t *stacksize)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_thread_attr *p_attr = ABTI_thread_attr_get_ptr(attr);
    ABTI_CHECK_NULL_THREAD_ATTR_PTR(p_attr);

    *stacksize = p_attr->stacksize;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_thread_attr_get_stacksize", abt_errno);
    goto fn_exit;
}

/**
 * @ingroup ULT_ATTR
 * @brief   Set the scheduling priority in the attribute object.
 *
 * \c ABT_thread_attr_set_prio() sets the scheduling priority with one value of
 * \c ABT_sched_prio in the target attribute object.
 *
 * @param[in] attr  handle to the target attribute object
 * @param[in] prio  scheduling priority
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_thread_attr_set_prio(ABT_thread_attr attr, ABT_sched_prio prio)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_thread_attr *p_attr = ABTI_thread_attr_get_ptr(attr);

    /* Sanity check */
    ABTI_CHECK_NULL_THREAD_ATTR_PTR(p_attr);
    ABTI_CHECK_SCHED_PRIO(prio);

    /* Set the value */
    p_attr->prio = prio;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_thread_attr_set_prio", abt_errno);
    goto fn_exit;
}

/**
 * @ingroup ULT_ATTR
 * @brief   Get the scheduling priority from the attribute object.
 *
 * \c ABT_thread_attr_get_prio() returns the scheduling priority through
 * \c prio from the target attribute object.
 *
 * @param[in]  attr  handle to the target attribute object
 * @param[out] prio  scheduling priority
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_thread_attr_get_prio(ABT_thread_attr attr, ABT_sched_prio *prio)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_thread_attr *p_attr = ABTI_thread_attr_get_ptr(attr);
    ABTI_CHECK_NULL_THREAD_ATTR_PTR(p_attr);

    *prio = p_attr->prio;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_thread_attr_get_prio", abt_errno);
    goto fn_exit;
}

/**
 * @ingroup ULT_ATTR
 * @brief   Set callback function and its argument in the attribute object.
 *
 * \c ABT_thread_attr_set_callback() sets the callback function and its
 * argument, which will be invoked on ULT migration.
 *
 * @param[in] attr     handle to the target attribute object
 * @param[in] cb_func  callback function pointer
 * @param[in] cb_arg   argument for the callback function
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_thread_attr_set_callback(ABT_thread_attr attr,
        void(*cb_func)(ABT_thread thread, void *cb_arg), void *cb_arg)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_thread_attr *p_attr = ABTI_thread_attr_get_ptr(attr);
    ABTI_CHECK_NULL_THREAD_ATTR_PTR(p_attr);

    /* Set the value */
    p_attr->f_cb     = cb_func;
    p_attr->p_cb_arg = cb_arg;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_thread_attr_set_callback", abt_errno);
    goto fn_exit;
}

/**
 * @ingroup ULT_ATTR
 * @brief   Set the ULT's migratability in the attribute object.
 *
 * \c ABT_thread_attr_set_migratable() sets the ULT's migratability in the
 * target attribute object.
 * If \c flag is \c ABT_TRUE, the ULT created with this attribute becomes
 * migratable. On the other hand, if \ flag is \c ABT_FALSE, the ULT created
 * with this attribute becomes unmigratable.
 *
 * @param[in] attr  handle to the target attribute object
 * @param[in] flag  migratability flag (<tt>ABT_TRUE</tt>: migratable,
 *                  <tt>ABT_FALSE</tt>: not)
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_thread_attr_set_migratable(ABT_thread_attr attr, ABT_bool flag)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_thread_attr *p_attr = ABTI_thread_attr_get_ptr(attr);
    ABTI_CHECK_NULL_THREAD_ATTR_PTR(p_attr);

    /* Set the value */
    p_attr->migratable = flag;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_thread_attr_set_migratable", abt_errno);
    goto fn_exit;
}


/*****************************************************************************/
/* Private APIs                                                              */
/*****************************************************************************/

/** @defgroup ULT_ATTR_PRIVATE ULT Attributes (Private)
 * This group combines private APIs for ULT attributes.
 */

/**
 * @ingroup ULT_ATTR_PRIVATE
 * @brief   Get \c ABTI_thread_attr pointer from \c ABT_thread_attr handle.
 *
 * \c ABTI_thread_attr_get_ptr() returns \c ABTI_thread_attr pointer
 * corresponding to \c ABT_thread_attr handle \c attr. If \c attr is
 * \c ABT_THREAD_NULL, \c NULL is returned.
 *
 * @param[in] attr  handle to the ULT attribute
 * @return ABTI_thread_attr pointer
 */
ABTI_thread_attr *ABTI_thread_attr_get_ptr(ABT_thread_attr attr)
{
    ABTI_thread_attr *p_attr;
    if (attr == ABT_THREAD_ATTR_NULL) {
        p_attr = NULL;
    } else {
        p_attr = (ABTI_thread_attr *)attr;
    }
    return p_attr;
}

/**
 * @ingroup ULT_ATTR_PRIVATE
 * @brief   Get \c ABT_thread_attr handle from \c ABTI_thread_attr pointer.
 *
 * \c ABTI_thread_attr_get_handle() returns \c ABT_thread_attr handle
 * corresponding to \c ABTI_thread_attr pointer \c attr. If \c attr is
 * \c NULL, \c ABT_THREAD_NULL is returned.
 *
 * @param[in] p_attr  pointer to ABTI_thread_attr
 * @return ABT_thread_attr handle
 */
ABT_thread_attr ABTI_thread_attr_get_handle(ABTI_thread_attr *p_attr)
{
    ABT_thread_attr h_attr;
    if (p_attr == NULL) {
        h_attr = ABT_THREAD_ATTR_NULL;
    } else {
        h_attr = (ABT_thread_attr)p_attr;
    }
    return h_attr;
}

/**
 * @ingroup ULT_ATTR_PRIVATE
 * @brief   Print field values of ABTI_thread_attr.
 *
 * \c ABTI_thread_attr_print() prints out values of all fields in
 * \c ABTI_thread_attr struct.
 *
 * @param[in] p_attr  pointer to ABTI_thread_attr
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABTI_thread_attr_print(ABTI_thread_attr *p_attr)
{
    int abt_errno = ABT_SUCCESS;
    if (p_attr == NULL) {
        printf("[NULL ATTR]");
        goto fn_exit;
    }

    printf("[");
    printf("stacksize:%zu ", p_attr->stacksize);
    printf("prio:");
    switch (p_attr->prio) {
        case ABT_SCHED_PRIO_LOW:    printf("LOW ");    break;
        case ABT_SCHED_PRIO_NORMAL: printf("NORMAL "); break;
        case ABT_SCHED_PRIO_HIGH:   printf("HIGH ");   break;
        default: printf("UNKNOWN "); break;
    }
    printf("cb_func:%p ", p_attr->f_cb);
    printf("cb_arg:%p", p_attr->p_cb_arg);
    printf("]");

  fn_exit:
    return abt_errno;
}
