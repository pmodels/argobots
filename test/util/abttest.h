/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#ifndef ABTTEST_H_INCLUDED
#define ABTTEST_H_INCLUDED

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>

/* We always have to use assert in our test suite. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>


/** @defgroup TESTUTIL Test utility
 * This group is for test utility routines.
 */

/**
 * @ingroup TESTUTIL
 * @brief   Initialize Argobots and test environment.
 *
 * ABT_test_init() internally invokes ABT_init(). Therefore, the test code
 * does not need to call ABT_init().
 *
 * Environment variables:
 * - ABT_TEST_VERBOSE: numeric value. It sets the level of verbose output.
 *   This is used by ABT_test_error(). If not set, ABT_TEST_VERBOSE is the
 *   same as 0.
 *
 * @param[in] argc  the number of arguments
 * @param[in] argv  argument vector
 */
void ABT_test_init(int argc, char **argv);


/**
 * @ingroup TESTUTIL
 * @brief   Finailize Argobots and test environment.
 *
 * ABT_test_finalize() internally invokes ABT_finalize(). Therefore, the test
 * code does not need to call ABT_finalize().
 * If err is not zero, or errors have been catched by ABT_test_error(), this
 * routine returns EXIT_FAILURE. Otherwise, it returns EXIT_SUCCESS;
 *
 * @param[in] err  user error code
 * @return Status
 * @retval EXIT_SUCCESS on no error
 * @retval EXIT_FAILURE on error
 */
int ABT_test_finalize(int err);

/**
 * @ingroup TESTUTIL
 * @brief   Print the formatted string according to verbose level.
 *
 * ABT_test_printf() behaves like printf(), but it prints out the string
 * only when level is equal to or greater than the value of ABT_TEST_VERBOSE.
 *
 * @param[in] level  verbose level
 * @param[in] format format string
 */
void ABT_test_printf(int level, const char *format, ...);

/**
 * @ingroup TESTUTIL
 * @brief   Check the error code.
 *
 * ABT_test_error() checks the error code and outputs the string of error code
 * if the error code is not ABT_SUCCESS. Currently, if the error code is not
 * ABT_SUCCESS, this routine calles exit() to terminate the test code.
 *
 * @param[in] err   error code
 * @param[in] msg   user message
 * @param[in] file  file name
 * @param[in] line  line number
 */
void ABT_test_error(int err, const char *msg, const char *file, int line);


typedef enum {
    ABT_TEST_ARG_N_ES   = 0,    /* # of ESs */
    ABT_TEST_ARG_N_ULT  = 1,    /* # of ULTs */
    ABT_TEST_ARG_N_TASK = 2,    /* # of tasklets */
    ABT_TEST_ARG_N_ITER = 3     /* # of iterations */
} ABT_test_arg;

/**
 * @ingroup TESTUTIL
 * @brief   Read the argument vector.
 *
 * \c ABT_test_read_args reads the argument vector \c argv and save valid
 * arguments internally.  \c ABT_test_get_arg_val() is used to get the saved
 * argument value.
 *
 * @param[in] argc  the number of arguments
 * @param[in] argv  argument vector
 */
void ABT_test_read_args(int argc, char **argv);

/**
 * @ingroup TESTUTIL
 * @brief   Get the argument value.
 *
 * \c ABT_test_get_arg_val returns the argument value corresponding to \c arg.
 *
 * @param[in] arg  argument kind
 * @return Argument value
 */
int ABT_test_get_arg_val(ABT_test_arg arg);

/**
 * @ingroup TESTUTIL
 * @brief   Print a line.
 *
 * \c ABT_test_print_line prints out a line, which consists of \c len characters
 * of \c c, to a file pointer \c fp.
 *
 * @param[in] fp   file pointer
 * @param[in] c    character as a line element
 * @param[in] len  length of line
 */
void ABT_test_print_line(FILE *fp, char c, int len);

#define ABT_TEST_ERROR(e,m)     ABT_test_error(e,m,__FILE__,__LINE__)
#define ABT_TEST_UNUSED(a)      (void)(a)

#if defined(__x86_64__)
static inline uint64_t ABT_test_get_cycles()
{
    unsigned hi, lo;
    __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
    uint64_t cycle = ((uint64_t)lo) | (((int64_t)hi) << 32);
    return cycle;
}
#elif defined(__aarch64__)
static inline uint64_t ABT_test_get_cycles()
{
    register uint64_t cycle;
    __asm__ __volatile__ ("isb; mrs %0, cntvct_el0" : "=r"(cycle));
    return cycle;
}
#else
#warning "Cycle information is not supported on this platform"
static inline uint64_t ABT_test_get_cycles()
{
    return 0;
}
#endif

#endif /* ABTTEST_H_INCLUDED */
