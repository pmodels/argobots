/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include "abt.h"
#include "abttest.h"

#define DEFAULT_NUM_THREADS     4
#define DEFAULT_NUM_TASKS       4

void thread_func(void *arg)
{
    ABT_TEST_UNUSED(arg);
    /* Do nothing */
}

void task_func(void *arg)
{
    ABT_TEST_UNUSED(arg);
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

    unsigned long num_units = (unsigned long)(num_threads+num_tasks);

    ABT_xstream xstream;
    size_t n_units;
    size_t pool_size;
    int err = 0;

    /* Initialize */
    ABT_test_init(argc, argv);

    /* Get the SELF Execution Stream */
    ret = ABT_xstream_self(&xstream);
    ABT_TEST_ERROR(ret, "ABT_xstream_self");

    /* Get the pools attached to an execution stream */
    ABT_pool pool;
    ret = ABT_xstream_get_main_pools(xstream, 1, &pool);
    ABT_TEST_ERROR(ret, "ABT_xstream_get_main_pools");

    /* Create ULTs */
    for (i = 0; i < num_threads; i++) {
        ret = ABT_thread_create(pool, thread_func, NULL,
                ABT_THREAD_ATTR_NULL, NULL);
        ABT_TEST_ERROR(ret, "ABT_thread_create");
    }

    /* Create tasklets */
    for (i = 0; i < num_tasks; i++) {
        ret = ABT_task_create(pool, task_func, NULL, NULL);
        ABT_TEST_ERROR(ret, "ABT_task_create");
    }

    /* Get the numbers of ULTs and tasklets */
    ABT_sched sched;
    ret = ABT_xstream_get_main_sched(xstream, &sched);
    ABT_TEST_ERROR(ret, "ABT_xstream_get_main_sched");
    ABT_sched_get_total_size(sched, &n_units);
    ABT_TEST_ERROR(ret, "ABT_sched_get_total_size");

    if (n_units != num_units) {
        err++;
        printf("# of units: expected(%lu) vs. result(%lu)\n",
               num_units, (unsigned long)n_units);
    }

    do {
        /* Switch to other work units */
        ABT_thread_yield();
        ret = ABT_pool_get_size(pool, &pool_size);
        ABT_TEST_ERROR(ret, "ABT_pool_get_size");
    } while (pool_size > 0);

    /* Get the numbers of ULTs and tasklets */
    ret = ABT_sched_get_total_size(sched, &n_units);
    ABT_TEST_ERROR(ret, "ABT_sched_get_total_size");
    if (n_units != 0) {
        err++;
        printf("# of units: expected(%d) vs. result(%lu)\n",
               0, (unsigned long)n_units);
    }

    /* Finalize */
    ret = ABT_test_finalize(err);

    return ret;
}

