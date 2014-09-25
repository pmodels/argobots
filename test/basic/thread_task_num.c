/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "abt.h"
#include "abttest.h"

#define DEFAULT_NUM_THREADS     4
#define DEFAULT_NUM_TASKS       4

void thread_func(void *arg)
{
    /* Do nothing */
}

void task_func(void *arg)
{
    /* Do nothing */
}

int main(int argc, char *argv[])
{
    int i, ret;
    int num_threads = DEFAULT_NUM_THREADS;
    int num_tasks = DEFAULT_NUM_TASKS;
    if (argc > 1) num_threads = atoi(argv[1]);
    assert(num_threads >= 0);
    if (argc > 2) num_tasks = atoi(argv[2]);
    assert(num_tasks >= 0);

    ABT_xstream xstream;
    int n_threads, n_tasks;
    int err = 0;

    /* Initialize */
    ABT_test_init(argc, argv);

    /* Get the SELF Execution Stream */
    ret = ABT_xstream_self(&xstream);
    ABT_TEST_ERROR(ret, "ABT_xstream_self");

    /* Create ULTs */
    for (i = 0; i < num_threads; i++) {
        ret = ABT_thread_create(xstream, thread_func, NULL,
                ABT_THREAD_ATTR_NULL, NULL);
        ABT_TEST_ERROR(ret, "ABT_thread_create");
    }

    /* Create tasklets */
    for (i = 0; i < num_tasks; i++) {
        ret = ABT_task_create(xstream, task_func, NULL, NULL);
        ABT_TEST_ERROR(ret, "ABT_task_create");
    }

    /* Get the numbers of ULTs and tasklets */
    ABT_xstream_get_num_threads(xstream, &n_threads);
    ABT_xstream_get_num_tasks(xstream, &n_tasks);
    if (n_threads != num_threads) {
        err++;
        printf("# of ULTs: expected(%d) vs. result(%d)\n",
               num_threads, n_threads);
    }
    if (n_tasks != num_tasks) {
        err++;
        printf("# of Tasklets: expected(%d) vs. result(%d)\n",
               num_tasks, n_tasks);
    }

    /* Switch to other work units */
    ABT_thread_yield();

    /* Get the numbers of ULTs and tasklets */
    ABT_xstream_get_num_threads(xstream, &n_threads);
    ABT_xstream_get_num_tasks(xstream, &n_tasks);
    if (n_threads != 0) {
        err++;
        printf("# of ULTs: expected(%d) vs. result(%d)\n", 0, n_threads);
    }
    if (n_tasks != 0) {
        err++;
        printf("# of Tasklets: expected(%d) vs. result(%d)\n", 0, n_tasks);
    }

    /* Finalize */
    ret = ABT_test_finalize(err);

    return ret;
}


