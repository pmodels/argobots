/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

static inline size_t sched_config_type_size(ABT_sched_config_type type);
ABTU_ret_err static int sched_config_get(const ABTI_sched_config *p_config,
                                         int idx, ABT_sched_config_type *p_type,
                                         void *p_val);
ABTU_ret_err static int sched_config_set(ABTI_sched_config *p_config, int idx,
                                         ABT_sched_config_type type,
                                         const void *p_val);
static void sched_config_free(ABTI_sched_config *p_config);

/** @defgroup SCHED_CONFIG Scheduler config
 * This group is for Scheduler config.
 */

/* Global configurable parameters */
ABT_sched_config_var ABT_sched_config_var_end = { .idx = -1,
                                                  .type =
                                                      ABT_SCHED_CONFIG_INT };

ABT_sched_config_var ABT_sched_config_access = { .idx = -2,
                                                 .type = ABT_SCHED_CONFIG_INT };

ABT_sched_config_var ABT_sched_config_automatic = { .idx = -3,
                                                    .type =
                                                        ABT_SCHED_CONFIG_INT };

ABT_sched_config_var ABT_sched_basic_freq = { .idx = -4,
                                              .type = ABT_SCHED_CONFIG_INT };

/**
 * @ingroup SCHED_CONFIG
 * @brief   Create a new scheduler configuration.
 *
 * \c ABT_sched_config_create() creates a new scheduler configuration and
 * returns its handle through \c config.
 *
 * The variadic arguments are an array of tuples composed of a variable of type
 * \c ABT_sched_config_var and a value for this variable.  The array must end
 * with a single value \c ABT_sched_config_var_end.
 *
 * Currently, Argobots supports the following hints:
 *
 * - \c ABT_sched_basic_freq:
 *
 *   The frequency of event checks of the predefined scheduler.  A smaller value
 *   indicates more frequent check.  If this is not specified, the default value
 *   is used for scheduler creation.
 *
 * - \c ABT_sched_config_automatic:
 *
 *   Whether the scheduler is automatically freed or not.  If the value is
 *   \c ABT_TRUE, the scheduler is automatically freed when a work unit
 *   associated with the scheduler is freed.  If this is not specified, the
 *   default value of each scheduler creation routine is used for scheduler
 *   creation.
 *
 * - \c ABT_sched_config_access:
 *
 *   This is deprecated and ignored.
 *
 * @note
 * \DOC_NOTE_DEFAULT_SCHED_AUTOMATIC
 *
 * \c config must be freed by \c ABT_sched_config_free() after its use.
 *
 * @note
 * For example, this routine can be called as follows to configure the
 * predefined scheduler to have a frequency for checking events equal to \a 5:
 * @code{.c}
 * ABT_sched_config config;
 * ABT_sched_config_create(&config, ABT_sched_basic_freq, 5,
 *                         ABT_sched_config_var_end);
 * @endcode
 *
 * If the array contains multiple tuples that have the same \c idx of
 * \c ABT_sched_config_var, \c idx is mapped to a corrupted value.
 *
 * @contexts
 * \DOC_CONTEXT_INIT \DOC_CONTEXT_NOCTXSWITCH
 *
 * @errors
 * \DOC_ERROR_SUCCESS
 * \DOC_ERROR_INV_ARG_SCHED_CONFIG_TYPE
 * \DOC_ERROR_RESOURCE
 *
 * @undefined
 * \DOC_UNDEFINED_UNINIT
 * \DOC_UNDEFINED_SCHED_CONFIG_CREATE_UNFORMATTED
 * \DOC_UNDEFINED_NULL_PTR{\c config}
 *
 * @param[out] config   scheduler configuration handle
 * @param[in]  ...      array of arguments
 * @return Error code
 */
int ABT_sched_config_create(ABT_sched_config *config, ...)
{
    ABTI_UB_ASSERT(ABTI_initialized());

    int abt_errno;
    int i = 0;
    ABTI_sched_config *p_config;

    abt_errno = ABTU_calloc(1, sizeof(ABTI_sched_config), (void **)&p_config);
    ABTI_CHECK_ERROR(abt_errno);
    /* Initialize index. */

    for (i = 0; i < ABTI_SCHED_CONFIG_HTABLE_SIZE; i++) {
        p_config->elements[i].idx = ABTI_SCHED_CONFIG_UNUSED_INDEX;
    }

    va_list varg_list;
    va_start(varg_list, config);

    /* We read (var, value) until we find ABT_sched_config_var_end */
    while (1) {
        ABT_sched_config_var var = va_arg(varg_list, ABT_sched_config_var);
        int idx = var.idx;
        if (idx == ABT_sched_config_var_end.idx)
            break;
        /* Add the argument */
        switch (var.type) {
            case ABT_SCHED_CONFIG_INT: {
                int int_val = va_arg(varg_list, int);
                abt_errno = sched_config_set(p_config, idx,
                                             ABT_SCHED_CONFIG_INT, &int_val);
                break;
            }
            case ABT_SCHED_CONFIG_DOUBLE: {
                double double_val = va_arg(varg_list, double);
                abt_errno =
                    sched_config_set(p_config, idx, ABT_SCHED_CONFIG_DOUBLE,
                                     &double_val);
                break;
            }
            case ABT_SCHED_CONFIG_PTR: {
                void *ptr_val = va_arg(varg_list, void *);
                abt_errno = sched_config_set(p_config, idx,
                                             ABT_SCHED_CONFIG_PTR, &ptr_val);
                break;
            }
            default:
                abt_errno = ABT_ERR_INV_ARG;
        }
        if (abt_errno != ABT_SUCCESS) {
            sched_config_free(p_config);
            va_end(varg_list);
            ABTI_HANDLE_ERROR(abt_errno);
        }
    }
    va_end(varg_list);

    *config = ABTI_sched_config_get_handle(p_config);
    return ABT_SUCCESS;
}

/**
 * @ingroup SCHED_CONFIG
 * @brief   Retrieve values from a scheduler configuration.
 *
 * \c ABT_sched_config_read() reads values from the scheduler configuration
 * \c config and sets the values to variables given as the variadic arguments
 * that contain at least \c num_vars pointers.  This routine sets the \a i th
 * argument where \a i starts from 0 to a value mapped to a tuple that has
 * \c ABT_sched_config_var with its \c idx = \a i.  Each argument needs to be a
 * pointer of a type specified by a corresponding \c type of
 * \c ABT_sched_config_var.  If the \a i th argument is \c NULL, a value
 * associated with \c idx = \a i is not copied.  If a value associated with
 * \c idx = \a i does not exist, the \a i th argument is not updated.
 *
 * @note
 * For example, this routine can be called as follows to get a value that is
 * corresponding to \c idx = \a 1.
 * @code{.c}
 * // ABT_sched_config_var var = { 1, ABT_SCHED_CONFIG_INT };
 * int val;
 * ABT_sched_config_read(&config, 2, NULL, &val);
 * @endcode
 *
 * @contexts
 * \DOC_CONTEXT_INIT \DOC_CONTEXT_NOCTXSWITCH
 *
 * @errors
 * \DOC_ERROR_SUCCESS
 * \DOC_ERROR_INV_ARG_NEG{\c num_vars}
 * \DOC_ERROR_INV_SCHED_CONFIG_HANDLE{\c config}
 *
 * @undefined
 * \DOC_UNDEFINED_UNINIT
 *
 * @param[in]  config    scheduler configuration handle
 * @param[in]  num_vars  number of variable pointers in \c ...
 * @param[out] ...       array of variable pointers
 * @return Error code
 */
int ABT_sched_config_read(ABT_sched_config config, int num_vars, ...)
{
    ABTI_UB_ASSERT(ABTI_initialized());

    int idx;
    ABTI_sched_config *p_config = ABTI_sched_config_get_ptr(config);
    ABTI_CHECK_NULL_SCHED_CONFIG_PTR(p_config);

    va_list varg_list;
    va_start(varg_list, num_vars);
    for (idx = 0; idx < num_vars; idx++) {
        void *ptr = va_arg(varg_list, void *);
        if (ptr) {
            int abt_errno = sched_config_get(p_config, idx, NULL, ptr);
            /* It's okay even if there's no associated value. */
            (void)abt_errno;
        }
    }
    va_end(varg_list);
    return ABT_SUCCESS;
}

/**
 * @ingroup SCHED_CONFIG
 * @brief   Free a scheduler configuration.
 *
 * \c ABT_sched_config_free() deallocates the resource used for the scheduler
 * configuration \c sched_config and sets \c sched_config to
 * \c ABT_SCHED_CONFIG_NULL.
 *
 * @contexts
 * \DOC_CONTEXT_INIT \DOC_CONTEXT_NOCTXSWITCH
 *
 * @errors
 * \DOC_ERROR_SUCCESS
 * \DOC_ERROR_INV_SCHED_CONFIG_PTR{\c config}
 *
 * @undefined
 * \DOC_UNDEFINED_UNINIT
 * \DOC_UNDEFINED_NULL_PTR{\c config}
 * \DOC_UNDEFINED_THREAD_UNSAFE_FREE{\c config}
 *
 * @param[in,out] config  scheduler configuration handle
 * @return Error code
 */
int ABT_sched_config_free(ABT_sched_config *config)
{
    ABTI_UB_ASSERT(ABTI_initialized());

    ABTI_sched_config *p_config = ABTI_sched_config_get_ptr(*config);
    ABTI_CHECK_NULL_SCHED_CONFIG_PTR(p_config);

    sched_config_free(p_config);

    *config = ABT_SCHED_CONFIG_NULL;

    return ABT_SUCCESS;
}

/**
 * @ingroup SCHED_CONFIG
 * @brief   Register a value to a scheduler configuration.
 *
 * \c ABT_sched_config_set() associated a value pointed to by the value \c val
 * with the index \c idx in the scheduler configuration \c config.  This routine
 * overwrites a value and its type if a value has already been associated with
 * \c idx.
 *
 * @note
 * For example, this routine can be called as follows to set a value that is
 * corresponding to \c idx = \a 1.
 * @code{.c}
 * const ABT_sched_config_var var = { 1, ABT_SCHED_CONFIG_INT };
 * int val = 10;
 * ABT_sched_config_set(&config, var.idx, var.type, &val);
 * @endcode
 *
 * If \c value is \c NULL, this routine deletes a value associated with \c idx
 * if such exists.
 *
 * @note
 * This routine returns \c ABT_SUCCESS even if \c value is \c NULL but no value
 * is associated with \c idx.
 *
 * @contexts
 * \DOC_CONTEXT_INIT \DOC_CONTEXT_NOCTXSWITCH
 *
 * @errors
 * \DOC_ERROR_SUCCESS
 * \DOC_ERROR_INV_ARG_SCHED_CONFIG_TYPE{\c type}
 * \DOC_ERROR_INV_SCHED_CONFIG_HANDLE{\c config}
 * \DOC_ERROR_RESOURCE
 *
 * @undefined
 * \DOC_UNDEFINED_UNINIT
 * \DOC_UNDEFINED_THREAD_UNSAFE{\c config}
 *
 * @param[in]  config  scheduler configuration handle
 * @param[in]  idx     index of a target value
 * @param[in]  type    type of a target value
 * @param[in]  val     target value
 * @return Error code
 */
int ABT_sched_config_set(ABT_sched_config config, int idx,
                         ABT_sched_config_type type, const void *val)
{
    ABTI_UB_ASSERT(ABTI_initialized());

    ABTI_sched_config *p_config = ABTI_sched_config_get_ptr(config);
    ABTI_CHECK_NULL_SCHED_CONFIG_PTR(p_config);
    int abt_errno = sched_config_set(p_config, idx, type, val);
    ABTI_CHECK_ERROR(abt_errno);
    return ABT_SUCCESS;
}

/**
 * @ingroup SCHED_CONFIG
 * @brief   Retrieve a value from a scheduler configuration.
 *
 * \c ABT_sched_config_get() reads a value associated with the index \c idx of
 * \c ABT_sched_config_var from the scheduler configuration \c config.  If
 * \c val is not \c NULL, \c val is set to the value.  If \c type is not
 * \c NULL, \c type is set to the type of the value.
 *
 * @note
 * For example, this routine can be called as follows to get a value that is
 * corresponding to \c idx = \a 1.
 * @code{.c}
 * const ABT_sched_config_var var = { 1, ABT_SCHED_CONFIG_INT };
 * int val;
 * ABT_sched_config_type type;
 * ABT_sched_config_get(&config, var.idx, &type, &val);
 * assert(type == var.type);
 * @endcode
 *
 * @contexts
 * \DOC_CONTEXT_INIT \DOC_CONTEXT_NOCTXSWITCH
 *
 * @errors
 * \DOC_ERROR_SUCCESS
 * \DOC_ERROR_INV_ARG_SCHED_CONFIG_INDEX{\c config, \c idx}
 * \DOC_ERROR_INV_SCHED_CONFIG_HANDLE{\c config}
 *
 * @undefined
 * \DOC_UNDEFINED_UNINIT
 * \DOC_UNDEFINED_THREAD_UNSAFE{\c config}
 *
 * @param[in]  config  scheduler configuration handle
 * @param[in]  idx     index of a target value
 * @param[out] type    type of a target value
 * @param[out] val     target value
 * @return Error code
 */
int ABT_sched_config_get(ABT_sched_config config, int idx,
                         ABT_sched_config_type *type, void *val)
{
    ABTI_UB_ASSERT(ABTI_initialized());

    ABTI_sched_config *p_config = ABTI_sched_config_get_ptr(config);
    ABTI_CHECK_NULL_SCHED_CONFIG_PTR(p_config);
    int abt_errno = sched_config_get(p_config, idx, type, val);
    ABTI_CHECK_ERROR(abt_errno);
    return ABT_SUCCESS;
}

/*****************************************************************************/
/* Private APIs                                                              */
/*****************************************************************************/

ABTU_ret_err int ABTI_sched_config_read(const ABTI_sched_config *p_config,
                                        int idx, void *p_val)
{
    return sched_config_get(p_config, idx, NULL, p_val);
}

/*****************************************************************************/
/* Internal static functions                                                 */
/*****************************************************************************/

ABTU_ret_err static int sched_config_get(const ABTI_sched_config *p_config,
                                         int idx, ABT_sched_config_type *p_type,
                                         void *p_val)
{
    int table_index = ((idx % ABTI_SCHED_CONFIG_HTABLE_SIZE) +
                       ABTI_SCHED_CONFIG_HTABLE_SIZE) %
                      ABTI_SCHED_CONFIG_HTABLE_SIZE;
    if (p_config->elements[table_index].idx == ABTI_SCHED_CONFIG_UNUSED_INDEX) {
        return ABT_ERR_INV_ARG;
    } else {
        const ABTI_sched_config_element *p_element =
            &p_config->elements[table_index];
        while (p_element) {
            if (p_element->idx == idx) {
                if (p_val) {
                    memcpy(p_val, p_element->val,
                           sched_config_type_size(p_element->type));
                }
                if (p_type) {
                    *p_type = p_element->type;
                }
                return ABT_SUCCESS;
            } else {
                p_element = p_element->p_next;
            }
        }
        return ABT_ERR_INV_ARG;
    }
}

ABTU_ret_err static int sched_config_set(ABTI_sched_config *p_config, int idx,
                                         ABT_sched_config_type type,
                                         const void *p_val)
{
    int table_index = ((idx % ABTI_SCHED_CONFIG_HTABLE_SIZE) +
                       ABTI_SCHED_CONFIG_HTABLE_SIZE) %
                      ABTI_SCHED_CONFIG_HTABLE_SIZE;
    if (p_config->elements[table_index].idx == ABTI_SCHED_CONFIG_UNUSED_INDEX) {
        if (p_val) {
            /* Newly add. */
            p_config->elements[table_index].idx = idx;
            p_config->elements[table_index].type = type;
            memcpy(p_config->elements[table_index].val, p_val,
                   sched_config_type_size(type));
        }
    } else {
        ABTI_sched_config_element *p_element = &p_config->elements[table_index];
        ABTI_sched_config_element **pp_element = NULL;
        while (p_element) {
            if (p_element->idx == idx) {
                if (p_val) {
                    /* Update. */
                    p_element->type = type;
                    memcpy(p_element->val, p_val, sched_config_type_size(type));
                } else {
                    /* Remove the element. */
                    if (pp_element) {
                        *pp_element = p_element->p_next;
                        ABTU_free(p_element);
                    } else {
                        ABTI_sched_config_element *p_next = p_element->p_next;
                        if (p_next) {
                            memcpy(p_element, p_next,
                                   sizeof(ABTI_sched_config_element));
                            ABTU_free(p_next);
                        } else {
                            p_element->idx = ABTI_SCHED_CONFIG_UNUSED_INDEX;
                        }
                    }
                }
                break;
            } else if (!p_element->p_next) {
                if (p_val) {
                    /* Newly add. */
                    ABTI_sched_config_element *p_new_element;
                    int abt_errno =
                        ABTU_calloc(1, sizeof(ABTI_sched_config_element),
                                    (void **)&p_new_element);
                    ABTI_CHECK_ERROR(abt_errno);
                    p_new_element->idx = idx;
                    p_new_element->type = type;
                    memcpy(p_new_element->val, p_val,
                           sched_config_type_size(type));
                    p_element->p_next = p_new_element;
                }
                break;
            } else {
                pp_element = &p_element->p_next;
                p_element = *pp_element;
            }
        }
    }
    return ABT_SUCCESS;
}

static void sched_config_free(ABTI_sched_config *p_config)
{
    /* Check elements. */
    int i;
    for (i = 0; i < ABTI_SCHED_CONFIG_HTABLE_SIZE; i++) {
        ABTI_sched_config_element *p_element = p_config->elements[i].p_next;
        while (p_element) {
            ABTI_sched_config_element *p_next = p_element->p_next;
            ABTU_free(p_element);
            p_element = p_next;
        }
    }
    ABTU_free(p_config);
}

static inline size_t sched_config_type_size(ABT_sched_config_type type)
{
    switch (type) {
        case ABT_SCHED_CONFIG_INT:
            return sizeof(int);
        case ABT_SCHED_CONFIG_DOUBLE:
            return sizeof(double);
        case ABT_SCHED_CONFIG_PTR:
            return sizeof(void *);
        default:
            ABTI_ASSERT(0);
            ABTU_unreachable();
    }
}
