/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#ifndef THREAD_ATTR_H_INCLUDED
#define THREAD_ATTR_H_INCLUDED

/* Inlined functions for ULT Attributes */

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
static inline ABTI_thread_attr *ABTI_thread_attr_get_ptr(ABT_thread_attr attr)
{
#ifndef ABT_CONFIG_DISABLE_ERROR_CHECK
    ABTI_thread_attr *p_attr;
    if (attr == ABT_THREAD_ATTR_NULL) {
        p_attr = NULL;
    } else {
        p_attr = (ABTI_thread_attr *)attr;
    }
    return p_attr;
#else
    return (ABTI_thread_attr *)attr;
#endif
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
static inline
    ABT_thread_attr ABTI_thread_attr_get_handle(ABTI_thread_attr *p_attr)
{
#ifndef ABT_CONFIG_DISABLE_ERROR_CHECK
    ABT_thread_attr h_attr;
    if (p_attr == NULL) {
        h_attr = ABT_THREAD_ATTR_NULL;
    } else {
        h_attr = (ABT_thread_attr)p_attr;
    }
    return h_attr;
#else
    return (ABT_thread_attr)p_attr;
#endif
}

#ifndef ABT_CONFIG_DISABLE_MIGRATION
static inline void ABTI_thread_attr_init_migration(ABTI_thread_attr *p_attr,
                                                   ABT_bool migratable)
{
    p_attr->migratable = migratable;
    p_attr->f_cb = NULL;
    p_attr->p_cb_arg = NULL;
}
#endif

static inline void ABTI_thread_attr_init(ABTI_thread_attr *p_attr,
                                         void *p_stack, size_t stacksize,
                                         ABTI_stack_type stacktype,
                                         ABT_bool migratable)
{
    p_attr->p_stack = p_stack;
    p_attr->stacksize = stacksize;
    p_attr->stacktype = stacktype;
#ifndef ABT_CONFIG_DISABLE_MIGRATION
    ABTI_thread_attr_init_migration(p_attr, migratable);
#endif
}

static inline void ABTI_thread_attr_copy(ABTI_thread_attr *p_dest,
                                         ABTI_thread_attr *p_src)
{
    memcpy(p_dest, p_src, sizeof(ABTI_thread_attr));
}

#endif /* THREAD_ATTR_H_INCLUDED */
