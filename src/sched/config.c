/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

static inline size_t sched_config_type_size(ABT_sched_config_type type);

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
 * The current Argobots supports the following hints:
 *
 * - \c ABT_sched_basic_freq:
 *
 *   The frequency of event checks of the predefined scheduler.  A smaller value
 *   indicates a more frequent check.  If this is not specified, the default
 *   value is used for scheduler creation.
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
 * \c ABT_sched_config_var, the value associated with \c idx is corrupted.
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
    int abt_errno;
    ABTI_sched_config *p_config;

    char *buffer = NULL;
    size_t alloc_size = 8 * sizeof(size_t);

    int num_params = 0;
    size_t offset = sizeof(num_params);

    size_t buffer_size = alloc_size;
    abt_errno = ABTU_malloc(buffer_size, (void **)&buffer);
    ABTI_CHECK_ERROR(abt_errno);

    va_list varg_list;
    va_start(varg_list, config);

    /* We read each couple (var, value) until we find ABT_sched_config_var_end
     */
    while (1) {
        ABT_sched_config_var var = va_arg(varg_list, ABT_sched_config_var);
        if (var.idx == ABT_sched_config_var_end.idx)
            break;

        int param = var.idx;
        ABT_sched_config_type type = var.type;
        num_params++;

        size_t size = sched_config_type_size(type);
        if (offset + sizeof(param) + sizeof(type) + size > buffer_size) {
            size_t cur_buffer_size = buffer_size;
            buffer_size += alloc_size;
            abt_errno =
                ABTU_realloc(cur_buffer_size, buffer_size, (void **)&buffer);
            if (ABTI_IS_ERROR_CHECK_ENABLED && abt_errno != ABT_SUCCESS) {
                ABTU_free(buffer);
                ABTI_HANDLE_ERROR(abt_errno);
            }
        }
        /* Copy the parameter index */
        memcpy(buffer + offset, (void *)&param, sizeof(param));
        offset += sizeof(param);

        /* Copy the size of the argument */
        memcpy(buffer + offset, (void *)&size, sizeof(size));
        offset += sizeof(size);

        /* Copy the argument */
        void *ptr;
        int i;
        double d;
        void *p;
        switch (type) {
            case ABT_SCHED_CONFIG_INT:
                i = va_arg(varg_list, int);
                ptr = (void *)&i;
                break;
            case ABT_SCHED_CONFIG_DOUBLE:
                d = va_arg(varg_list, double);
                ptr = (void *)&d;
                break;
            case ABT_SCHED_CONFIG_PTR:
                p = va_arg(varg_list, void *);
                ptr = (void *)&p;
                break;
            default:
                ABTI_HANDLE_ERROR(ABT_ERR_SCHED_CONFIG);
        }

        memcpy(buffer + offset, ptr, size);
        offset += size;
    }
    va_end(varg_list);

    if (num_params) {
        memcpy(buffer, (int *)&num_params, sizeof(num_params));
    } else {
        ABTU_free(buffer);
        buffer = NULL;
    }

    p_config = (ABTI_sched_config *)buffer;
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
 * For example, this routine can be called as follows to get a value is
 * corresponding to \c ABT_sched_config_var where its \c idx is 1.
 * @code{.c}
 * ABT_sched_config_var var = { 1, ABT_SCHED_CONFIG_INT };
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
    int abt_errno;
    int v;

    /* We read all the variables and save the addresses */
    void **variables;
    abt_errno = ABTU_malloc(num_vars * sizeof(void *), (void **)&variables);
    ABTI_CHECK_ERROR(abt_errno);

    va_list varg_list;
    va_start(varg_list, num_vars);
    for (v = 0; v < num_vars; v++) {
        variables[v] = va_arg(varg_list, void *);
    }
    va_end(varg_list);

    abt_errno = ABTI_sched_config_read(config, 1, num_vars, variables);
    ABTI_CHECK_ERROR(abt_errno);

    ABTU_free(variables);
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
    ABTI_sched_config *p_config = ABTI_sched_config_get_ptr(*config);
    ABTU_free(p_config);

    *config = ABT_SCHED_CONFIG_NULL;

    return ABT_SUCCESS;
}

/*****************************************************************************/
/* Private APIs                                                              */
/*****************************************************************************/

ABTU_ret_err int ABTI_sched_config_read_global(ABT_sched_config config,
                                               ABT_pool_access *access,
                                               ABT_bool *automatic)
{
    int abt_errno;
    int num_vars = 2;
    /* We use XXX_i variables because va_list converts these types into int */
    int access_i = -1;
    int automatic_i = -1;

    void **variables;
    abt_errno = ABTU_malloc(num_vars * sizeof(void *), (void **)&variables);
    ABTI_CHECK_ERROR(abt_errno);

    variables[(ABT_sched_config_access.idx + 2) * (-1)] = &access_i;
    variables[(ABT_sched_config_automatic.idx + 2) * (-1)] = &automatic_i;

    abt_errno = ABTI_sched_config_read(config, 0, num_vars, variables);
    ABTU_free(variables);
    ABTI_CHECK_ERROR(abt_errno);

    if (access_i != -1)
        *access = (ABT_pool_access)access_i;
    if (automatic_i != -1)
        *automatic = (ABT_bool)automatic_i;

    return ABT_SUCCESS;
}

/* type is 0 if we read the private parameters, else 1 */
ABTU_ret_err int ABTI_sched_config_read(ABT_sched_config config, int type,
                                        int num_vars, void **variables)
{
    size_t offset = 0;
    int num_params;

    if (config == ABT_SCHED_CONFIG_NULL) {
        return ABT_SUCCESS;
    }

    ABTI_sched_config *p_config = ABTI_sched_config_get_ptr(config);

    char *buffer = (char *)p_config;

    /* Number of parameters in buffer */
    memcpy(&num_params, buffer, sizeof(num_params));
    offset += sizeof(num_params);

    /* Copy the data from buffer to the right variables */
    int p;
    for (p = 0; p < num_params; p++) {
        int var_idx;
        size_t size;

        /* Get the variable index of the next parameter */
        memcpy(&var_idx, buffer + offset, sizeof(var_idx));
        offset += sizeof(var_idx);
        /* Get the size of the next parameter */
        memcpy(&size, buffer + offset, sizeof(size));
        offset += sizeof(size);
        /* Get the next argument */
        /* We save it only if
         *   - the index is < 0  when type == 0
         *   - the index is >= 0 when type == 1
         */
        if (type == 0) {
            if (var_idx < 0) {
                var_idx = (var_idx + 2) * -1;
                if (var_idx >= num_vars)
                    return ABT_ERR_INV_SCHED_CONFIG;
                memcpy(variables[var_idx], buffer + offset, size);
            }
        } else {
            if (var_idx >= 0) {
                if (var_idx >= num_vars)
                    return ABT_ERR_INV_SCHED_CONFIG;
                memcpy(variables[var_idx], buffer + offset, size);
            }
        }
        offset += size;
    }
    return ABT_SUCCESS;
}

/*****************************************************************************/
/* Internal static functions                                                 */
/*****************************************************************************/

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
