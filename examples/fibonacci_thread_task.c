/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

/**
 * This example shows the interleaving of threads and tasks to compute Fibonacci
 * numbers.
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
    ABT_eventual eventual;
} thread_args;

/* structure to pass arguments to threads */
typedef struct task_args_t {
    int n;
    int result;
    ABT_mutex mutex;
    struct task_args_t *parent;
} task_args;

/* Function to compute Fibonacci numbers */
void fibonacci_thread(void *arguments)
{
    int n, *n1, *n2;
    thread_args a1, a2;
    ABT_eventual eventual, f1, f2;

    thread_args *args = (thread_args *)arguments;
    n = args->n;
    eventual = args->eventual;

    /* checking for base cases */
    if (n <= 2)
        args->result = 1;
    else {
        ABT_eventual_create(sizeof(int), &f1);
        a1.n = n - 1;
        a1.eventual = f1;
        ABT_thread_create(g_pool, fibonacci_thread, &a1, ABT_THREAD_ATTR_NULL,
                          NULL);

        ABT_eventual_create(sizeof(int), &f2);
        a2.n = n - 2;
        a2.eventual = f2;
        ABT_thread_create(g_pool, fibonacci_thread, &a2, ABT_THREAD_ATTR_NULL,
                          NULL);

        ABT_eventual_wait(f1, (void **)&n1);
        ABT_eventual_wait(f2, (void **)&n2);

        args->result = *n1 + *n2;

        ABT_eventual_free(&f1);
        ABT_eventual_free(&f2);
    }

    /* checking whether to signal the eventual */
    if (eventual != ABT_EVENTUAL_NULL) {
        ABT_eventual_set(eventual, &args->result, sizeof(int));
    }
}

/* Function to compute Fibonacci numbers */
void fibonacci_task(void *arguments)
{
    int n, result;
    task_args *a1, *a2, *parent, *temp;
    ABT_task t1, t2;

    task_args *args = (task_args *)arguments;
    n = args->n;
    parent = args->parent;

    /* checking for base cases */
    if (n <= 2) {
        args->result = 1;
        result = 1;
        int flag = 1;
        while (flag && parent != NULL) {
            ABT_mutex_lock(parent->mutex);
            parent->result += result;
            if (result == parent->result) flag = 0;
            ABT_mutex_unlock(parent->mutex);
            result = parent->result;
            temp = parent->parent;

            if (flag && temp) {
                ABT_mutex_free(&parent->mutex);
                free(parent);
            }

            parent = temp;
        }

        ABT_mutex_free(&args->mutex);
        if (args->parent) {
            free(args);
        }
    } else {
        a1 = (task_args *)malloc(sizeof(task_args));
        a1->n = n - 1;
        a1->result = 0;
        ABT_mutex_create(&a1->mutex);
        a1->parent = args;
        ABT_task_create(g_pool, fibonacci_task, a1, &t1);

        a2 = (task_args *)malloc(sizeof(task_args));
        a2->n = n - 2;
        a2->result = 0;
        ABT_mutex_create(&a2->mutex);
        a2->parent = args;
        ABT_task_create(g_pool, fibonacci_task, a2, &t2);
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
    int n, i, result, expected;
    int num_xstreams;
    ABT_xstream *xstreams;

    ABT_thread thread;
    thread_args args_thread;
    ABT_task task;
    task_args *args_task;

    if (argc > 1 && strcmp(argv[1], "-h") == 0) {
        printf("Usage: %s [N=10] [num_ES=4]\n", argv[0]);
        return EXIT_SUCCESS;
    }
    n = argc > 1 ? atoi(argv[1]) : N;
    num_xstreams = argc > 2 ? atoi(argv[2]) : NUM_XSTREAMS;
    printf("# of ESs: %d\n", num_xstreams);

    if (n <= 2) {
        result = 1;
        goto fn_result;
    }

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

    /* creating thread */
    args_thread.n = n - 1;
    args_thread.eventual = ABT_EVENTUAL_NULL;
    ABT_thread_create(g_pool, fibonacci_thread, &args_thread, ABT_THREAD_ATTR_NULL, &thread);

    /* creating task */
    args_task = (task_args *)malloc(sizeof(task_args));
    args_task->n = n - 2;
    args_task->result = 0;
    ABT_mutex_create(&args_task->mutex);
    args_task->parent = NULL;
    ABT_task_create(g_pool, fibonacci_task, args_task, &task);

    /* join other threads */
    ABT_thread_join(thread);
    ABT_thread_free(&thread);

    /* join ESs */
    for (i = 1; i < num_xstreams; i++) {
        ABT_xstream_join(xstreams[i]);
        ABT_xstream_free(&xstreams[i]);
    }

    result = args_thread.result + args_task->result;
    free(args_task);

    ABT_finalize();

    free(xstreams);

  fn_result:
    printf("Fib(%d): %d\n", n, result);
    expected = verify(n);
    if (result != expected) {
        fprintf(stderr, "ERROR: expected=%d\n", expected);
        exit(EXIT_FAILURE);
    }

    return EXIT_SUCCESS;
}
