/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "abt.h"
#include "abttest.h"

#define DEFAULT_NUM_XSTREAMS    2
#define DEFAULT_NUM_THREADS     2
#define DEFAULT_NUM_TASKS       0
#define DEFAULT_NUM_LOOP        10

typedef struct {
    size_t num;
    unsigned long long result;
} task_arg_t;

void thread_func(void *arg)
{
    size_t my_id = (size_t)arg;
    int i;

    /* Set my priority */
    ABT_thread thread;
    ABT_thread_self(&thread);
    ABT_sched_prio prio_min, prio_max, prio;
    ABT_sched_get_prio_min(ABT_SCHED_PRIO, &prio_min);
    ABT_sched_get_prio_max(ABT_SCHED_PRIO, &prio_max);
    ABT_test_printf(1, "[TH%lu]: prio_max=%d, prio_min=%d\n",
                    my_id, prio_max, prio_min);
    prio = prio_min + my_id % (prio_max - prio_min + 1);
    ABT_thread_set_prio(thread, prio);
    for (i = 0; i < DEFAULT_NUM_LOOP / 2; i ++) {
        ABT_test_printf(1, "[TH%lu]: my priority=%d\n", my_id, prio);

        ABT_test_printf(1, "[TH%lu]: brefore yield\n", my_id);
        ABT_thread_yield();
        ABT_test_printf(1, "[TH%lu]: doing something ...\n", my_id);
        ABT_thread_yield();
        ABT_test_printf(1, "[TH%lu]: after yield\n", my_id);
    }
    prio = prio_max - my_id % (prio_max - prio_min + 1);
    ABT_thread_set_prio(thread, prio);
    for (i = DEFAULT_NUM_LOOP / 2; i < DEFAULT_NUM_LOOP; i ++) {
        ABT_test_printf(1, "[TH%lu]: my priority=%d\n", my_id, prio);

        ABT_test_printf(1, "[TH%lu]: brefore yield\n", my_id);
        ABT_thread_yield();
        ABT_test_printf(1, "[TH%lu]: doing something ...\n", my_id);
        ABT_thread_yield();
        ABT_test_printf(1, "[TH%lu]: after yield\n", my_id);
    }

    ABT_thread_release(thread);
}

void task_func1(void *arg)
{
    int i;
    size_t num = (size_t)arg;
    unsigned long long result = 1;
    for (i = 2; i <= num; i++) {
        result += i;
    }
    ABT_test_printf(1, "task_func1: num=%lu result=%llu\n", num, result);
}

void task_func2(void *arg)
{
    size_t i;
    task_arg_t *my_arg = (task_arg_t *)arg;
    unsigned long long result = 1;
    for (i = 2; i <= my_arg->num; i++) {
        result += i;
    }
    my_arg->result = result;
}

int main(int argc, char *argv[])
{
    int i, j;
    int ret;
    int num_xstreams = DEFAULT_NUM_XSTREAMS;
    int num_threads = DEFAULT_NUM_THREADS;
    int num_tasks = DEFAULT_NUM_TASKS;
    if (argc > 1) num_xstreams = atoi(argv[1]);
    assert(num_xstreams >= 0);
    if (argc > 2) num_threads = atoi(argv[2]);
    assert(num_threads >= 0);
    if (argc > 3) num_tasks = atoi(argv[3]);
    assert(num_tasks >= 0);

    ABT_sched *scheds;
    ABT_xstream *xstreams;
    ABT_task *tasks;
    task_arg_t *task_args;

    scheds = (ABT_sched *)malloc(sizeof(ABT_sched) * num_xstreams);
    xstreams = (ABT_xstream *)malloc(sizeof(ABT_xstream) * num_xstreams);
    tasks = (ABT_task *)malloc(sizeof(ABT_task) * num_tasks);
    task_args = (task_arg_t *)malloc(sizeof(task_arg_t) * num_tasks);

    /* Initialize */
    ABT_test_init(argc, argv);

    /* Create schedulers.
     * There is one scheduler for each execution stream
     * and the default is FIFO.
     */
    for (i = 0; i < num_xstreams; i++) {
        ABT_sched_kind kind = ABT_SCHED_FIFO;
        switch (i % 3) {
            case 0: kind = ABT_SCHED_FIFO; break;
            case 1: kind = ABT_SCHED_LIFO; break;
            case 2: kind = ABT_SCHED_PRIO; break;
            default: break;
        }
        ret = ABT_sched_create_basic(kind, &scheds[i]);
        ABT_TEST_ERROR(ret, "ABT_sched_create_basic");
    }

    /* Create Execution Streams */
    ret = ABT_xstream_self(&xstreams[0]);
    ABT_TEST_ERROR(ret, "ABT_xstream_self");
    ABT_xstream_set_sched(xstreams[0], scheds[0]);
    for (i = 1; i < num_xstreams; i++) {
        ret = ABT_xstream_create(scheds[i], &xstreams[i]);
        ABT_TEST_ERROR(ret, "ABT_xstream_create");
    }

    /* Create tasks with task_func1 */
    for (i = 0; i < num_tasks; i++) {
        size_t num = 100 + i;
        ret = ABT_task_create(ABT_XSTREAM_NULL,
                              task_func1, (void *)num,
                              NULL);
        ABT_TEST_ERROR(ret, "ABT_task_create");
    }

    /* Create threads */
    for (i = 0; i < num_xstreams; i++) {
        for (j = 0; j < num_threads; j++) {
            size_t tid = i * num_threads + j + 1;
            ret = ABT_thread_create(xstreams[i],
                    thread_func, (void *)tid, ABT_THREAD_ATTR_NULL,
                    NULL);
            ABT_TEST_ERROR(ret, "ABT_thread_create");
        }
    }

    /* Create tasks with task_func2 */
    for (i = 0; i < num_tasks; i++) {
        task_args[i].num = 100 + i;
        ret = ABT_task_create(xstreams[i % num_xstreams],
                              task_func2, (void *)&task_args[i],
                              &tasks[i]);
        ABT_TEST_ERROR(ret, "ABT_task_create");
    }

    /* Switch to other user level threads */
    ABT_thread_yield();

    /* Results of task_funcs2 */
    for (i = 0; i < num_tasks; i++) {
        ABT_task_state state;
        do {
            ABT_task_get_state(tasks[i], &state);
            ABT_thread_yield();
        } while (state != ABT_TASK_STATE_TERMINATED);

        ABT_test_printf(1, "task_func2: num=%lu result=%llu\n",
               task_args[i].num, task_args[i].result);

        /* Free named tasks */
        ret = ABT_task_free(&tasks[i]);
        ABT_TEST_ERROR(ret, "ABT_task_free");
    }

    /* Join Execution Streams */
    for (i = 1; i < num_xstreams; i++) {
        ret = ABT_xstream_join(xstreams[i]);
        ABT_TEST_ERROR(ret, "ABT_xstream_join");
    }

    /* Free Execution Streams */
    for (i = 1; i < num_xstreams; i++) {
        ret = ABT_xstream_free(&xstreams[i]);
        ABT_TEST_ERROR(ret, "ABT_xstream_free");
    }

    /* Finalize */
    ret = ABT_test_finalize(0);

    free(task_args);
    free(tasks);
    free(xstreams);
    free(scheds);

    return ret;
}


