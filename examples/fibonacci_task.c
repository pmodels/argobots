/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

/**
 * This example shows the use of tasks to compute Fibonacci numbers. The
 * execution proceeds in two phases.  1) Expand phase. A binary tree of
 * activation records is built in a top-down fashion. Each activation record
 * corresponds to a Fibonacci number Fib(n) computed recursively. A task
 * computing Fib(n) will create two subtasks to compute Fib(n-1) and Fib(n-2),
 * respectively.  2) Aggregrate phase. The final results is computed bottom-up.
 * Once a base case is reached (n<=2), a task is created to aggregate the result
 * into the parent's activation record. Therefore, two tasks will update the
 * activation record for Fib(n). The last of the two aggregate tasks will spawn
 * another task that will aggregate the result up the binary tree.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <abt.h>

#define N               10
#define NUM_XSTREAMS    4

/* global variables */
ABT_pool g_pool = ABT_POOL_NULL;
ABT_eventual eventual = ABT_EVENTUAL_NULL;

/* forward declaration */
void aggregate_fibonacci(void *arguments);

/* structure to pass arguments to expand tasks */
typedef struct exp_task_args_t {
    int n;
    int result;
    int flag;
    ABT_mutex mutex;
    struct exp_task_args_t *parent;
} exp_task_args;

/* structure to pass arguments to aggregate tasks */
typedef struct agg_task_args_t {
    int result;
    exp_task_args *parent;
} agg_task_args;

/* Function to compute Fibonacci numbers during expand phase */
void expand_fibonacci(void *arguments)
{
    int n;
    exp_task_args *exp_args1, *exp_args2, *parent;
    agg_task_args *agg_args;

    exp_task_args *args = (exp_task_args *)arguments;
    n = args->n;
    parent = args->parent;

    /* checking for base cases */
    if (n <= 2) {
        /* creating an aggregate task */
        if (parent != NULL) {
            agg_args = (agg_task_args *)malloc(sizeof(agg_task_args));
            agg_args->result = 1;
            agg_args->parent = parent;
            ABT_task_create(g_pool, aggregate_fibonacci, agg_args, NULL);
        } else {
            args->result = 1;
            ABT_eventual_set(eventual, &args->result, sizeof(int));
        }
        ABT_mutex_free(&args->mutex);
        free(args);
    } else {
        /* creating task to compute Fib(n-1) */
        exp_args1 = (exp_task_args *)malloc(sizeof(exp_task_args));
        exp_args1->n = n - 1;
        exp_args1->result = 0;
        exp_args1->flag = 0;
        ABT_mutex_create(&exp_args1->mutex);
        exp_args1->parent = args;
        ABT_task_create(g_pool, expand_fibonacci, exp_args1, NULL);

        /* creating task to compute Fib(n-2) */
        exp_args2 = (exp_task_args *)malloc(sizeof(exp_task_args));
        exp_args2->n = n - 2;
        exp_args2->result = 0;
        exp_args2->flag = 0;
        ABT_mutex_create(&exp_args2->mutex);
        exp_args2->parent = args;
        ABT_task_create(g_pool, expand_fibonacci, exp_args2, NULL);
    }
}

/* Function to compute Fibonacci numbers during aggregate phase */
void aggregate_fibonacci(void *arguments)
{
    exp_task_args *parent;
    agg_task_args *args, *agg_args;
    int result;

    args = (agg_task_args *)arguments;
    parent = args->parent;
    result = args->result;
    free(args);

    /* checking whether this is the root of the tree */
    if (parent != NULL) {
        int flag;
        ABT_mutex_lock(parent->mutex);
        parent->result += result;
        flag = parent->flag;
        if (!flag) parent->flag = 1;
        ABT_mutex_unlock(parent->mutex);
        if (flag) {
            /* creating an aggregate task */
            agg_args = (agg_task_args *)malloc(sizeof(agg_task_args));
            agg_args->result = parent->result;
            agg_args->parent = parent->parent;
            ABT_task_create(g_pool, aggregate_fibonacci, agg_args, NULL);

            ABT_mutex_free(&parent->mutex);
            free(parent);
        }
    } else {
        ABT_eventual_set(eventual, &result, sizeof(int));
    }
}

int fibonacci(int n)
{
    exp_task_args *args;
    int result;
    int *data;

    /* creating eventual */
    ABT_eventual_create(sizeof(int), &eventual);

    /* creating parent task to compute Fib(n) */
    args = (exp_task_args *)malloc(sizeof(exp_task_args));
    args->n = n;
    args->result = 0;
    args->flag = 0;
    ABT_mutex_create(&args->mutex);
    args->parent = NULL;
    ABT_task_create(g_pool, expand_fibonacci, args, NULL);

    /* block until the eventual is signaled */
    ABT_eventual_wait(eventual, (void **)&data);

    /* get the result */
    result = *data;

    ABT_eventual_free(&eventual);

    return result;
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
    int n, i, result, expected;
    int num_xstreams;
    ABT_xstream *xstreams;

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

    result = fibonacci(n);

    /* join ESs */
    for (i = 1; i < num_xstreams; i++) {
        ABT_xstream_join(xstreams[i]);
        ABT_xstream_free(&xstreams[i]);
    }

    ABT_finalize();

    free(xstreams);

    printf("Fib(%d): %d\n", n, result);
    expected = verify(n);
    if (result != expected) {
        fprintf(stderr, "ERROR: expected=%d\n", expected);
        exit(EXIT_FAILURE);
    }

    return EXIT_SUCCESS;
}
