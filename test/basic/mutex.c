/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include "abt.h"
#include "abttest.h"

#define DEFAULT_NUM_XSTREAMS    4
#define DEFAULT_NUM_THREADS     4

int g_counter = 0;

typedef struct thread_arg {
    int id;
    ABT_mutex mutex;
} thread_arg_t;

void thread_func(void *arg)
{
    thread_arg_t *t_arg = (thread_arg_t *)arg;

    ABT_thread_yield();

    ABT_mutex_lock(t_arg->mutex);
    g_counter++;
    ABT_mutex_unlock(t_arg->mutex);
    ABT_test_printf(1, "[TH%d] increased\n", t_arg->id);

    ABT_thread_yield();
}

int main(int argc, char *argv[])
{
    int i, j;
    int ret, expected;
    int num_xstreams = DEFAULT_NUM_XSTREAMS;
    int num_threads = DEFAULT_NUM_THREADS;
    if (argc > 1) num_xstreams = atoi(argv[1]);
    assert(num_xstreams >= 0);
    if (argc > 2) num_threads = atoi(argv[2]);
    assert(num_threads >= 0);

    ABT_mutex mutex;
    ABT_xstream *xstreams;
    thread_arg_t **args;
    xstreams = (ABT_xstream *)malloc(sizeof(ABT_xstream) * num_xstreams);
    assert(xstreams != NULL);
    args = (thread_arg_t **)malloc(sizeof(thread_arg_t *) * num_xstreams);
    assert(args != NULL);
    for (i = 0; i < num_xstreams; i++) {
        args[i] = (thread_arg_t *)malloc(sizeof(thread_arg_t) * num_threads);
    }

    /* Initialize */
    ABT_test_init(argc, argv);

    /* Create Execution Streams */
    ret = ABT_xstream_self(&xstreams[0]);
    ABT_TEST_ERROR(ret, "ABT_xstream_self");
    for (i = 1; i < num_xstreams; i++) {
        ret = ABT_xstream_create(ABT_SCHED_NULL, &xstreams[i]);
        ABT_TEST_ERROR(ret, "ABT_xstream_create");
    }

    /* Get the pools attached to an execution stream */
    ABT_pool *pools;
    pools = (ABT_pool *)malloc(sizeof(ABT_pool) * num_xstreams);
    for (i = 0; i < num_xstreams; i++) {
        ret = ABT_xstream_get_main_pools(xstreams[i], 1, pools+i);
        ABT_TEST_ERROR(ret, "ABT_xstream_get_main_pools");
    }

    /* Create a mutex */
    ret = ABT_mutex_create(&mutex);
    ABT_TEST_ERROR(ret, "ABT_mutex_create");

    /* Create threads */
    for (i = 0; i < num_xstreams; i++) {
        for (j = 0; j < num_threads; j++) {
            int tid = i * num_threads + j + 1;
            args[i][j].id = tid;
            args[i][j].mutex = mutex;
            ret = ABT_thread_create(pools[i],
                    thread_func, (void *)&args[i][j], ABT_THREAD_ATTR_NULL,
                    NULL);
            ABT_TEST_ERROR(ret, "ABT_thread_create");
        }
    }

    /* Switch to other user level threads */
    ABT_thread_yield();

    /* Join Execution Streams */
    for (i = 1; i < num_xstreams; i++) {
        ret = ABT_xstream_join(xstreams[i]);
        ABT_TEST_ERROR(ret, "ABT_xstream_join");
    }

    /* Free the mutex */
    ret = ABT_mutex_free(&mutex);
    ABT_TEST_ERROR(ret, "ABT_mutex_free");

    /* Free Execution Streams */
    for (i = 1; i < num_xstreams; i++) {
        ret = ABT_xstream_free(&xstreams[i]);
        ABT_TEST_ERROR(ret, "ABT_xstream_free");
    }

    /* Validation */
    expected = num_xstreams * num_threads;
    if (g_counter != expected) {
        printf("g_counter = %d\n", g_counter);
    }

    /* Finalize */
    ret = ABT_test_finalize(g_counter != expected);

    for (i = 0; i < num_xstreams; i++) {
        free(args[i]);
    }
    free(args);
    free(xstreams);
    free(pools);

    return ret;
}

