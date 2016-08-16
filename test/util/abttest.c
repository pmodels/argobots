/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include "abt.h"
#include "abttest.h"

static int g_verbose = 0;
static int g_num_errs = 0;

/* NOTE: The below NUM_ARG_KINDS should match the number of values in enum
 * ABT_test_arg in abttest.h. */
#define NUM_ARG_KINDS   4
static int g_arg_val[NUM_ARG_KINDS];

void ABT_test_init(int argc, char **argv)
{
    int ret;
    char *envval;

    /* Read the command arguments */
    ABT_test_read_args(argc, argv);

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

    if (g_verbose && level <= g_verbose) {
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
    if (err == ABT_ERR_FEATURE_NA) {
        printf("Skipped\n");
        fflush(stdout);
        exit(77);
    }

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

static void ABT_test_print_help(char *prog)
{
    fprintf(stderr, "Usage: %s [-e num_es] [-u num_ult] [-t num_task] "
                    "[-i iter] [-v verbose_level]\n", prog);
    fflush(stderr);
}

void ABT_test_read_args(int argc, char **argv)
{
    static int read = 0;
    int i, opt;

    if (read == 0) read = 1;
    else return;

    for (i = 0; i < NUM_ARG_KINDS; i++) {
        g_arg_val[i] = 1;
    }

    opterr = 0;
    while ((opt = getopt(argc, argv, "he:u:t:i:v:")) != -1) {
        switch (opt) {
            case 'e':
                g_arg_val[ABT_TEST_ARG_N_ES] = atoi(optarg);
                break;
            case 'u':
                g_arg_val[ABT_TEST_ARG_N_ULT] = atoi(optarg);
                break;
            case 't':
                g_arg_val[ABT_TEST_ARG_N_TASK] = atoi(optarg);
                break;
            case 'i':
                g_arg_val[ABT_TEST_ARG_N_ITER] = atoi(optarg);
                break;
            case 'v':
                g_verbose = atoi(optarg);
                break;
            case 'h':
                ABT_test_print_help(argv[0]);
                exit(EXIT_SUCCESS);
                break;
            default:
                break;
        }
    }
}

int ABT_test_get_arg_val(ABT_test_arg arg)
{
    if (arg < ABT_TEST_ARG_N_ES || (int)arg >= NUM_ARG_KINDS) {
        return 0;
    }
    return g_arg_val[arg];
}

void ABT_test_print_line(FILE *fp, char c, int len)
{
    int i;
    for (i = 0; i < len; i++) {
        fprintf(fp, "%c", c);
    }
    fprintf(fp, "\n");
    fflush(fp);
}

