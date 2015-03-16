/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"


#include <stdlib.h>
#include <assert.h>
#include <stdarg.h>
#include <string.h>

/** @defgroup SCHED_CONFIG Scheduler config
 * This group is for Scheduler config.
 */

ABT_sched_config_var ABT_sched_config_var_end = {
    .idx = -1,
    .type = ABT_SCHED_CONFIG_INT
};

size_t ABTI_sched_config_type_size(ABT_sched_config_type type);

/**
 * @ingroup SCHED_CONFIG
 * @brief   Create a scheduler configuration.
 *
 * This function is used to create a specific configuration of a scheduler. The
 * dynamic parameters are a list of tuples composed of the variable of type \c
 * ABT_sched_config_var and a value for this variable. The list must end with a
 * single value \c ABT_sched_config_var_end.
 *
 * If you want to write your own scheduler and use this function, you can find
 * a good example in the test called \c sched_config.
 *
 * For example, if you want to configure the basic scheduler to have a
 * frequency for checking events equal to 5, you will have this call:
 * ABT_sched_config_create(&config, ABT_sched_basic_freq, 5, ABT_sched_config_var_end);
 *
 * @param[out] config   configuration to create
 * @param[in]  ...      list of arguments
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_sched_config_create(ABT_sched_config *config, ...)
{
    ABTI_sched_config *p_config;

    char *buffer = NULL;
    size_t alloc_size = 8*sizeof(size_t);

    int num_params = 0;
    size_t offset = sizeof(num_params);

    size_t buffer_size = alloc_size;
    buffer = (char *)ABTU_malloc(buffer_size);

    va_list varg_list;
    va_start(varg_list, config);

    /* We read each couple (var, value) until we find ABT_sched_config_var_end */
    while (1) {
        ABT_sched_config_var var = va_arg(varg_list, ABT_sched_config_var);
        if (var.idx == ABT_sched_config_var_end.idx)
            break;

        int param = var.idx;
        ABT_sched_config_type type = var.type;
        num_params++;

        size_t size = ABTI_sched_config_type_size(type);
        if (offset+sizeof(param)+sizeof(type)+size > buffer_size) {
            buffer_size += alloc_size;
            buffer = ABTU_realloc(buffer, buffer_size);
        }
        /* Copy the parameter index */
        memcpy(buffer+offset, (void *)&param, sizeof(param));
        offset += sizeof(param);

        /* Copy the size of the argument */
        memcpy(buffer+offset, (void *)&size, sizeof(size));
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
                assert(0);
        }

        memcpy(buffer+offset, ptr, size);
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
 * @brief   Copy the set values from config into the variables passed in the
 *          dynamic list of arguments.
 *
 * The arguments in \c ... are the addresses of the variables where to copy the
 * packed data. The packed data are copied to their corresponding variables.
 * For a good example, see the test \c sched_config.
 *
 * @param[in] config    configuration to read
 * @param[in] num_vars  number of variable addresses in \c ...
 * @param[in] ...       list of arguments
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_sched_config_read(ABT_sched_config config, int num_vars, ...)
{
    size_t offset = 0;
    int num_params;
    int v;

    ABTI_sched_config *p_config = ABTI_sched_config_get_ptr(config);

    if (p_config == NULL) return ABT_SUCCESS;

    char *buffer = (char *)p_config;

    /* We read all the variables and save the addresses */
    void **variables = (void *)ABTU_malloc(num_vars*sizeof(void *));
    va_list varg_list;
    va_start(varg_list, num_vars);
    for (v = 0; v < num_vars; v++) {
        variables[v] = va_arg(varg_list, void *);
    }
    va_end(varg_list);

    /* Number of parameters in buffer */
    memcpy(&num_params, buffer, sizeof(num_params));
    offset += sizeof(num_params);

    /* Copy the data from buffer to the right variables */
    int p;
    for (p = 0; p < num_params; p++)
    {
        int var_idx;
        size_t size;

        /* Get the variable index of the next parameter */
        memcpy(&var_idx, buffer+offset, sizeof(var_idx));
        offset += sizeof(var_idx);
        /* Get the size of the next parameter */
        memcpy(&size, buffer+offset, sizeof(size));
        offset += sizeof(size);
        /* Get the next argument */
        memcpy(variables[var_idx], buffer+offset, size);
        offset += size;
    }

    return ABT_SUCCESS;
}

/**
 * @ingroup SCHED_CONFIG
 * @brief   Free the configuration.
 *
 * @param[in,out] config  configuration to free
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_sched_config_free(ABT_sched_config *config)
{
    ABTI_sched_config *p_config = ABTI_sched_config_get_ptr(*config);
    ABTU_free(p_config);

    *config = ABT_SCHED_CONFIG_NULL;

    return ABT_SUCCESS;
}

size_t ABTI_sched_config_type_size(ABT_sched_config_type type)
{
    switch (type) {
        case ABT_SCHED_CONFIG_INT:
            return sizeof(int);
        case ABT_SCHED_CONFIG_DOUBLE:
            return sizeof(double);
        case ABT_SCHED_CONFIG_PTR:
            return sizeof(void *);
        default:
            assert(0);
    }
}
