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

    /* Default values */
    p_newattr->stacksize  = ABTI_global_get_thread_stacksize();
    p_newattr->migratable = ABT_TRUE;
    p_newattr->f_cb       = NULL;
    p_newattr->p_cb_arg   = NULL;

    /* Return value */
    *newattr = ABTI_thread_attr_get_handle(p_newattr);

    return abt_errno;
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
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
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
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
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
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
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
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
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
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
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
 * @brief   Print field values of ABTI_thread_attr.
 *
 * \c ABTI_thread_attr_print() prints out values of all fields in
 * \c ABTI_thread_attr struct.
 *
 * @param[in] p_attr  pointer to ABTI_thread_attr
 * @param[in] p_os    pointer to a FILE object (output stream)
 * @param[in] indent  amount of space to indent
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
void ABTI_thread_attr_print(ABTI_thread_attr *p_attr, FILE *p_os, int indent)
{
    char *prefix = ABTU_get_indent_str(indent);
    char attr[100];

    ABTI_thread_attr_get_str(p_attr, attr);
    fprintf(p_os, "%sULT attr: %s\n", prefix, attr);
    fflush(p_os);
    ABTU_free(prefix);
}

void ABTI_thread_attr_get_str(ABTI_thread_attr *p_attr, char *p_buf)
{
    if (p_attr == NULL) {
        sprintf(p_buf, "[NULL ATTR]");
        return;
    }

    sprintf(p_buf,
        "["
        "stacksize:%zu "
        "cb_func:%p "
        "cb_arg:%p"
        "]",
        (size_t)p_attr->stacksize,
        p_attr->f_cb,
        p_attr->p_cb_arg
    );
}
