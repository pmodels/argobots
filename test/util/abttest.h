/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#ifndef ABTTEST_H_INCLUDED
#define ABTTEST_H_INCLUDED

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

#define ABT_TEST_ERROR(e,m)     ABT_test_error(e,m,__FILE__,__LINE__)

#endif /* ABTTEST_H_INCLUDED */
