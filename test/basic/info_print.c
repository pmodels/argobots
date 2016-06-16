/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include "abt.h"
#include "abttest.h"

#define DEFAULT_NUM_XSTREAMS    2

void thread_func(void *arg)
{
    ABT_thread self;
    int ret;

    ret = ABT_thread_self(&self);
    ABT_TEST_ERROR(ret, "ABT_thread_self");

    ret = ABT_info_print_thread(stdout, self);
    ABT_TEST_ERROR(ret, "ABT_info_print_thread");
    fprintf(stdout, "\n");
}

void task_func(void *arg)
{
    ABT_task self;
    int ret;

    ret = ABT_task_self(&self);
    ABT_TEST_ERROR(ret, "ABT_task_self");

    ret = ABT_info_print_task(stdout, self);
    ABT_TEST_ERROR(ret, "ABT_info_print_task");
    fprintf(stdout, "\n");
}

int main(int argc, char *argv[])
{
    ABT_xstream *xstreams;
    ABT_sched *scheds;
    ABT_pool *pools;
    ABT_thread *threads;
    ABT_task *tasks;
    int num_xstreams;
    int i, ret;

    /* Initialize */
    ABT_test_init(argc, argv);
    if (argc < 2) {
        num_xstreams = DEFAULT_NUM_XSTREAMS;
    } else {
        num_xstreams = ABT_test_get_arg_val(ABT_TEST_ARG_N_ES);
    }

    ret = ABT_info_print_config(stdout);
    ABT_TEST_ERROR(ret, "ABT_info_print_config");
    fprintf(stdout, "\n");

    ABT_test_printf(1, "# of ESs        : %d\n", num_xstreams);

    xstreams = (ABT_xstream *)malloc(num_xstreams * sizeof(ABT_xstream));
    scheds   = (ABT_sched *)malloc(num_xstreams * sizeof(ABT_sched));
    pools    = (ABT_pool *)malloc(num_xstreams * sizeof(ABT_pool));
    threads  = (ABT_thread *)malloc(num_xstreams * sizeof(ABT_thread));
    tasks    = (ABT_task *)malloc(num_xstreams * sizeof(ABT_task));

    /* Create Execution Streams */
    ret = ABT_xstream_self(&xstreams[0]);
    ABT_TEST_ERROR(ret, "ABT_xstream_self");
    for (i = 1; i < num_xstreams; i++) {
        ret = ABT_xstream_create(ABT_SCHED_NULL, &xstreams[i]);
        ABT_TEST_ERROR(ret, "ABT_xstream_create");
    }

    ret = ABT_info_print_all_xstreams(stdout);
    ABT_TEST_ERROR(ret, "ABT_info_print_all_xstreams");
    fprintf(stdout, "\n");

    for (i = 0; i < num_xstreams; i++) {
        ret = ABT_xstream_get_main_sched(xstreams[i], &scheds[i]);
        ABT_TEST_ERROR(ret, "ABT_xstream_get_main_sched");

        ret = ABT_xstream_get_main_pools(xstreams[i], 1, &pools[i]);
        ABT_TEST_ERROR(ret, "ABT_xstream_get_main_pools");
    }

    for (i = 0; i < num_xstreams; i++) {
        ret = ABT_info_print_xstream(stdout, xstreams[i]);
        ABT_TEST_ERROR(ret, "ABT_info_print_xstream");
        fprintf(stdout, "\n");

        ret = ABT_info_print_sched(stdout, scheds[i]);
        ABT_TEST_ERROR(ret, "ABT_info_print_sched");
        fprintf(stdout, "\n");

        ret = ABT_info_print_pool(stdout, pools[i]);
        ABT_TEST_ERROR(ret, "ABT_info_print_pool");
        fprintf(stdout, "\n");
    }

    /* Create one ULT and one tasklet on each ES */
    for (i = 0; i < num_xstreams; i++) {
        ret = ABT_thread_create(pools[i], thread_func, NULL,
                                ABT_THREAD_ATTR_NULL, &threads[i]);
        ABT_TEST_ERROR(ret, "ABT_thread_create");
        ret = ABT_info_print_thread(stdout, threads[i]);
        ABT_TEST_ERROR(ret, "ABT_info_print_thread");
        fprintf(stdout, "\n");

        ret = ABT_task_create(pools[i], task_func, NULL, &tasks[i]);
        ABT_TEST_ERROR(ret, "ABT_task_create");
        ret = ABT_info_print_task(stdout, tasks[i]);
        ABT_TEST_ERROR(ret, "ABT_info_print_task");
        fprintf(stdout, "\n");
    }

    /* Join and free ULTs and tasklets */
    for (i = 0; i < num_xstreams; i++) {
        ret = ABT_thread_free(&threads[i]);
        ABT_TEST_ERROR(ret, "ABT_thread_free");
        ret = ABT_task_free(&tasks[i]);
        ABT_TEST_ERROR(ret, "ABT_task_free");
    }

    /* Join and free Execution Streams */
    for (i = 1; i < num_xstreams; i++) {
        ret = ABT_xstream_join(xstreams[i]);
        ABT_TEST_ERROR(ret, "ABT_xstream_join");
        ret = ABT_xstream_free(&xstreams[i]);
        ABT_TEST_ERROR(ret, "ABT_xstream_free");
    }

    /* Finalize */
    ret = ABT_test_finalize(0);

    free(xstreams);
    free(scheds);
    free(pools);
    free(threads);
    free(tasks);

    return ret;
}


