/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "abt.h"

#define DEFAULT_NUM_STREAMS     2
#define DEFAULT_NUM_TASKS       2

typedef struct {
    size_t num;
    unsigned long long result;
} task_arg_t;

void task_func1(void *arg)
{
    int i;
    size_t num = (size_t)arg;
    unsigned long long result = 1;
    for (i = 2; i <= num; i++) {
        result += i;
    }
    printf("task_func1: num=%lu result=%llu\n", num, result);
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
    int i, ret;
    int num_streams = DEFAULT_NUM_STREAMS;
    int num_tasks = DEFAULT_NUM_TASKS;
    if (argc > 1) num_streams = atoi(argv[1]);
    assert(num_streams >= 0);
    if (argc > 2) num_tasks = atoi(argv[2]);
    assert(num_tasks >= 0);

    ABT_Stream *streams;
    ABT_Task *tasks;
    task_arg_t *args;
    streams = (ABT_Stream *)malloc(sizeof(ABT_Stream) * num_streams);
    tasks = (ABT_Task *)malloc(sizeof(ABT_Task) * num_tasks);
    args = (task_arg_t *)malloc(sizeof(task_arg_t) * num_tasks);

    /* Initialize */
    ret = ABT_Init(argc, argv);
    if (ret != ABT_SUCCESS) {
        fprintf(stderr, "ERROR: ABT_Init\n");
        exit(EXIT_FAILURE);
    }

    /* Create streams */
    streams[0] = ABT_Stream_self();
    for (i = 1; i < num_streams; i++) {
        ret = ABT_Stream_create(ABT_SCHEDULER_NULL, &streams[i]);
        if (ret != ABT_SUCCESS) {
            fprintf(stderr, "ERROR: ABT_Stream_create for ES%d\n", i);
            exit(EXIT_FAILURE);
        }
    }

    /* Create tasks with task_func1 */
    for (i = 0; i < num_tasks; i++) {
        size_t num = 100 + i;
        ret = ABT_Task_create(ABT_STREAM_NULL,
                              task_func1, (void *)num,
                              NULL);
        if (ret != ABT_SUCCESS) {
            fprintf(stderr, "ERROR: ABT_Task_create for %d\n", i);
            exit(EXIT_FAILURE);
        }
    }

    /* Create tasks with task_func2 */
    for (i = 0; i < num_tasks; i++) {
        args[i].num = 100 + i;
        ret = ABT_Task_create(streams[i % num_streams],
                              task_func2, (void *)&args[i],
                              &tasks[i]);
        if (ret != ABT_SUCCESS) {
            fprintf(stderr, "ERROR: ABT_Task_create for %d\n", i);
            exit(EXIT_FAILURE);
        }
    }

    /* Switch to other work units */
    ABT_Thread_yield();

    /* Results of task_funcs2 */
    for (i = 0; i < num_tasks; i++) {
        while (ABT_Task_get_state(tasks[i]) != ABT_TASK_STATE_COMPLETED)
            ABT_Thread_yield();

        printf("task_func2: num=%lu result=%llu\n",
               args[i].num, args[i].result);

        /* Free named tasks */
        ret = ABT_Task_free(tasks[i]);
        if (ret != ABT_SUCCESS) {
            fprintf(stderr, "ERROR: ABT_Task_free for TK%d\n", i);
            exit(EXIT_FAILURE);
        }
    }

    /* Join streams */
    for (i = 1; i < num_streams; i++) {
        ret = ABT_Stream_join(streams[i]);
        if (ret != ABT_SUCCESS) {
            fprintf(stderr, "ERROR: ABT_Stream_join for ES%d\n", i);
            exit(EXIT_FAILURE);
        }
    }

    /* Free streams */
    for (i = 0; i < num_streams; i++) {
        ret = ABT_Stream_free(streams[i]);
        if (ret != ABT_SUCCESS) {
            fprintf(stderr, "ERROR: ABT_Stream_free for ES%d\n", i);
            exit(EXIT_FAILURE);
        }
    }

    /* Finalize */
    ABT_Finalize();

    free(args);
    free(tasks);
    free(streams);

    return EXIT_SUCCESS;
}

