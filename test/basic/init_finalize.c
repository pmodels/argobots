/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "abt.h"
#include "abttest.h"

#define DEFAULT_NUM_THREADS     10
#define DEFAULT_NUM_ITER        10

static int num_iter = DEFAULT_NUM_ITER;

static void *init_test(void *arg)
{
    int i, ret;

    ret = ABT_initialized();
    ABT_TEST_ERROR(ret, "ABT_initialized");

    for (i = 0; i < num_iter; i++) {
        ret = ABT_init(0, NULL);
        ABT_TEST_ERROR(ret, "ABT_init");
        ret = ABT_finalize();
        ABT_TEST_ERROR(ret, "ABT_finalize");
    }

    ret = ABT_initialized();
    ABT_TEST_ERROR(ret, "ABT_initialized");

    pthread_exit(NULL);
    return NULL;
}

int main(int argc, char *argv[])
{
    int num_threads = DEFAULT_NUM_THREADS;
    pthread_t *threads;
    int i, ret;

    int initialized = ABT_initialized();
    assert(initialized == ABT_ERR_UNINITIALIZED);

    /* Initialize */
    ABT_test_init(argc, argv);

    if (argc > 2) {
        num_threads = ABT_test_get_arg_val(ABT_TEST_ARG_N_ES);
        num_iter    = ABT_test_get_arg_val(ABT_TEST_ARG_N_ITER);
    }

    threads = (pthread_t *)malloc(num_threads * sizeof(pthread_t));
    assert(threads);

    /* Create threads. Each thread will call ABT_init and ABT_finalize. */
    for (i = 0; i < num_threads; i++) {
        ret = pthread_create(&threads[i], NULL, init_test, NULL);
        assert(ret == 0);
    }

    /* Join threads */
    for (i = 0; i < num_threads; i++) {
        ret = pthread_join(threads[i], NULL);
        assert(ret == 0);
    }

    /* Finalize */
    ret = ABT_test_finalize(0);

    initialized = ABT_initialized();
    assert(initialized == ABT_ERR_UNINITIALIZED);

    return ret;
}
