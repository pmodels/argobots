/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include "abt.h"
#include "abttest.h"

#define DEFAULT_NUM_XSTREAMS    4
#define DEFAULT_NUM_TASKS       4

int num_tasks = DEFAULT_NUM_TASKS;

void task_func(void *arg)
{
    int rank, ret;
    ABT_task self;
    uint64_t id;

    assert((size_t)arg == 1);

    ret = ABT_xstream_self_rank(&rank);
    ABT_TEST_ERROR(ret, "ABT_xstream_self_rank");
    ret = ABT_task_self(&self);
    ABT_TEST_ERROR(ret, "ABT_task_self");
    ret = ABT_task_get_id(self, &id);
    ABT_TEST_ERROR(ret, "ABT_task_get_id");

    ABT_test_printf(1, "[T%lu:E%u]: Hello, world!\n", id, rank);
}

void task_func2(void *arg)
{
    int rank, ret;
    ABT_task self;
    uint64_t id;

    assert((size_t)arg == 2);

    ret = ABT_xstream_self_rank(&rank);
    ABT_TEST_ERROR(ret, "ABT_xstream_self_rank");
    ret = ABT_task_self(&self);
    ABT_TEST_ERROR(ret, "ABT_task_self");
    ret = ABT_task_get_id(self, &id);
    ABT_TEST_ERROR(ret, "ABT_task_get_id");

    ABT_test_printf(1, "[T%lu:E%u]: Good-bye, world!\n", id, rank);
}

void task_create(void *arg)
{
    int rank, i, ret;
    ABT_thread self;
    ABT_thread_id id;
    ABT_pool my_pool;
    ABT_task *tasks;

    assert((size_t)arg == 0);

    ret = ABT_xstream_self_rank(&rank);
    ABT_TEST_ERROR(ret, "ABT_xstream_self_rank");
    ret = ABT_thread_self(&self);
    ABT_TEST_ERROR(ret, "ABT_thread_self");
    ret = ABT_thread_get_id(self, &id);
    ABT_TEST_ERROR(ret, "ABT_thread_get_id");
    ret = ABT_thread_get_last_pool(self, &my_pool);
    ABT_TEST_ERROR(ret, "ABT_thread_get_last_pool");

    tasks = (ABT_task *)malloc(num_tasks * sizeof(ABT_task));

    /* Create tasklets */
    for (i = 0; i < num_tasks; i++) {
        ret = ABT_task_create(my_pool, task_func, (void *)1, &tasks[i]);
        ABT_TEST_ERROR(ret, "ABT_task_create");
    }
    ABT_test_printf(1, "[U%lu:E%u]: created %d tasklets\n", id, rank, num_tasks);

    /* Join tasklets */
    for (i = 0; i < num_tasks; i++) {
        ret = ABT_task_join(tasks[i]);
        ABT_TEST_ERROR(ret, "ABT_task_join");
    }
    ABT_test_printf(1, "[U%lu:E%u]: joined %d tasklets\n", id, rank, num_tasks);

    /* Revive tasklets with a different function */
    for (i = 0; i < num_tasks; i++) {
        ret = ABT_task_revive(my_pool, task_func2, (void *)2, &tasks[i]);
        ABT_TEST_ERROR(ret, "ABT_task_revive");
    }
    ABT_test_printf(1, "[U%lu:E%u]: revived %d tasklets\n", id, rank, num_tasks);

    /* Join and free tasklets */
    for (i = 0; i < num_tasks; i++) {
        ret = ABT_task_free(&tasks[i]);
        ABT_TEST_ERROR(ret, "ABT_task_free");
    }
    ABT_test_printf(1, "[U%lu:E%u]: freed %d tasklets\n", id, rank, num_tasks);

    free(tasks);
}

int main(int argc, char *argv[])
{
    int i, ret;
    ABT_xstream *xstreams;
    ABT_pool *pools;
    ABT_thread *threads;
    int num_xstreams = DEFAULT_NUM_XSTREAMS;

    /* Initialize */
    ABT_test_init(argc, argv);

    if (argc >= 2) {
        num_xstreams = ABT_test_get_arg_val(ABT_TEST_ARG_N_ES);
        num_tasks    = ABT_test_get_arg_val(ABT_TEST_ARG_N_TASK);
    }

    ABT_test_printf(1, "# of ESs        : %d\n", num_xstreams);
    ABT_test_printf(1, "# of tasklets/ES: %d\n", num_tasks);

    xstreams = (ABT_xstream *)malloc(num_xstreams * sizeof(ABT_xstream));
    pools = (ABT_pool *)malloc(num_xstreams * sizeof(ABT_pool));
    threads = (ABT_thread *)malloc(num_xstreams * sizeof(ABT_thread));

    /* Create Execution Streams */
    ret = ABT_xstream_self(&xstreams[0]);
    ABT_TEST_ERROR(ret, "ABT_xstream_self");
    for (i = 1; i < num_xstreams; i++) {
        ret = ABT_xstream_create(ABT_SCHED_NULL, &xstreams[i]);
        ABT_TEST_ERROR(ret, "ABT_xstream_create");
    }

    /* Get the first pool of each ES */
    for (i = 0; i < num_xstreams; i++) {
        ret = ABT_xstream_get_main_pools(xstreams[i], 1, &pools[i]);
        ABT_TEST_ERROR(ret, "ABT_xstream_get_main_pools");
    }

    /* Create one ULT for each ES */
    for (i = 1; i < num_xstreams; i++) {
        ret = ABT_thread_create(pools[i], task_create, (void *)0,
                                ABT_THREAD_ATTR_NULL, &threads[i]);
        ABT_TEST_ERROR(ret, "ABT_thread_create");
    }

    task_create((void *)0);

    /* Join and free ULTs */
    for (i = 1; i < num_xstreams; i++) {
        ret = ABT_thread_free(&threads[i]);
        ABT_TEST_ERROR(ret, "ABT_thread_free");
    }

    /* Join and free ESs */
    for (i = 1; i < num_xstreams; i++) {
        ret = ABT_xstream_join(xstreams[i]);
        ABT_TEST_ERROR(ret, "ABT_xstream_join");
        ret = ABT_xstream_free(&xstreams[i]);
        ABT_TEST_ERROR(ret, "ABT_xstream_free");
    }

    /* Finalize */
    ret = ABT_test_finalize(0);

    free(xstreams);
    free(pools);
    free(threads);

    return ret;
}

