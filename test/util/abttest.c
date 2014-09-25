/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <assert.h>
#include "abt.h"
#include "abttest.h"

static int g_verbose = 0;
static int g_num_errs = 0;

void ABT_test_init(int argc, char **argv)
{
    int ret;
    char *envval;

    /* Initialize Argobots */
    ret = ABT_init(argc, argv);
    ABT_TEST_ERROR(ret, "ABT_init");

    /* Check environment variables */
    envval = getenv("ABT_TEST_VERBOSE");
    if (envval) {
        char *endptr;
        long val = strtol(envval, &endptr, 0);
        if (endptr == envval) {
            /* No digits are found */
            fprintf(stderr, "[Warning] %s is invalid for ABT_TEST_VERBOSE\n",
                    envval);
            fflush(stderr);
        } else if (val >= 0) {
            g_verbose = val;
        } else {
            /* Negative value */
            fprintf(stderr, "WARNING: %s is invalid for ABT_TEST_VERBOSE\n",
                    envval);
            fflush(stderr);
        }
    }
}

int ABT_test_finalize(int err)
{
    int ret;

    /* Finalize Argobots */
    ret = ABT_finalize();
    ABT_TEST_ERROR(ret, "ABT_finalize");

    if (g_num_errs > 0) {
        printf("Found %d errors\n", g_num_errs);
        ret = EXIT_FAILURE;
    } else if (err != 0) {
        printf("ERROR: code=%d\n", err);
        ret = EXIT_FAILURE;
    } else {
        printf("No Errors\n");
        ret = EXIT_SUCCESS;
    }
    fflush(stdout);

    return ret;
}

void ABT_test_printf(int level, const char *format, ...)
{
    va_list list;

    if (g_verbose && level >= g_verbose) {
        va_start(list, format);
        vprintf(format, list);
        va_end(list);
        fflush(stdout);
    }
}

void ABT_test_error(int err, const char *msg, const char *file, int line)
{
    char *err_str;
    size_t len;
    int ret;

    if (err == ABT_SUCCESS) return;

    ret = ABT_error_get_str(err, NULL, &len);
    assert(ret == ABT_SUCCESS);
    err_str = (char *)malloc(sizeof(char) * len + 1);
    assert(err_str != NULL);
    ret = ABT_error_get_str(err, err_str, NULL);

    fprintf(stderr, "%s (%d): %s (%s:%d)\n",
            err_str, err, msg, file, line);

    free(err_str);

    g_num_errs++;

    exit(EXIT_FAILURE);
}

