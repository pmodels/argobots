/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

/**
 * This example shows the use of futures to compute Fibonacci numbers.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <abt.h>

#define N               10
#define NUM_XSTREAMS    4

/* global variables */
ABT_pool g_pool = ABT_POOL_NULL;

/* structure to pass arguments to threads */
typedef struct {
    int n;
    int result;
    ABT_future future;
} thread_args;

/* Callback function passed to future */
void callback(void **args)
{
	int n1, n2;

	n1 = *(int *)args[1];
	n2 = *(int *)args[2];
	*(int *)args[0] = n1 + n2;
}

/* Function to compute Fibonacci numbers */
void fibonacci(void *arguments)
{
    int n;
    thread_args a1, a2;
    ABT_future future, fut;

    thread_args *args = (thread_args *)arguments;
    n = args->n;
    future = args->future;

    /* checking for base cases */
    if (n <= 2)
        args->result = 1;
    else {
        ABT_future_create(3, callback, &fut);
        ABT_future_set(fut, (void *)&args->result);

        a1.n = n - 1;
        a1.future = fut;
        ABT_thread_create(g_pool, fibonacci, &a1, ABT_THREAD_ATTR_NULL, NULL);

        a2.n = n - 2;
        a2.future = fut;
        ABT_thread_create(g_pool, fibonacci, &a2, ABT_THREAD_ATTR_NULL, NULL);

        ABT_future_wait(fut);
        ABT_future_free(&fut);
    }

    /* checking whether to signal the future */
    if (future != ABT_FUTURE_NULL) {
        ABT_future_set(future, (void *)&args->result);
    }
}

/* Verification function */
int verify(int n)
{
    int i;
    int old[2], val;

    if (n <= 2) return 1;

    old[0] = old[1] = 1;
    for (i = 3; i <= n; i++) {
        val = old[0] + old[1];
        old[i % 2] = val;
    }
    return val;
}

/* Main function */
int main(int argc, char *argv[])
{
    int n, i, expected;
    int num_xstreams;
    ABT_xstream *xstreams;
    ABT_thread thread;
    thread_args args;

    if (argc > 1 && strcmp(argv[1], "-h") == 0) {
        printf("Usage: %s [N=10] [num_ES=4]\n", argv[0]);
        return EXIT_SUCCESS;
    }
    n = argc > 1 ? atoi(argv[1]) : N;
    num_xstreams = argc > 2 ? atoi(argv[2]) : NUM_XSTREAMS;
    printf("# of ESs: %d\n", num_xstreams);

    /* initialization */
    ABT_init(argc, argv);

    /* shared pool creation */
    ABT_pool_create_basic(ABT_POOL_FIFO, ABT_POOL_ACCESS_MPMC, ABT_TRUE,
                          &g_pool);

    /* ES creation */
    xstreams = (ABT_xstream *)malloc(sizeof(ABT_xstream) * num_xstreams);
    ABT_xstream_self(&xstreams[0]);
    ABT_xstream_set_main_sched_basic(xstreams[0], ABT_SCHED_DEFAULT,
                                     1, &g_pool);
    for (i = 1; i < num_xstreams; i++) {
        ABT_xstream_create_basic(ABT_SCHED_DEFAULT, 1, &g_pool,
                                 ABT_SCHED_CONFIG_NULL, &xstreams[i]);
        ABT_xstream_start(xstreams[i]);
    }

    args.n = n;
    args.future = ABT_FUTURE_NULL;
    ABT_thread_create(g_pool, fibonacci, &args, ABT_THREAD_ATTR_NULL, &thread);

    /* join the thread */
    ABT_thread_join(thread);
    ABT_thread_free(&thread);

    /* join ESs */
    for (i = 1; i < num_xstreams; i++) {
        ABT_xstream_join(xstreams[i]);
        ABT_xstream_free(&xstreams[i]);
    }

    ABT_finalize();

    free(xstreams);

    printf("Fib(%d): %d\n", n, args.result);
    expected = verify(n);
    if (args.result != expected) {
        fprintf(stderr, "ERROR: expected=%d\n", expected);
        exit(EXIT_FAILURE);
    }

    return EXIT_SUCCESS;
}
