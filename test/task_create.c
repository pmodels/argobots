/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <abt.h>

#define DEFAULT_NUM_STREAMS     4
#define DEFAULT_NUM_TASKS       4

#define HANDLE_ERROR(ret,msg)                           \
    if (ret != ABT_SUCCESS) {                           \
        fprintf(stderr, "ERROR[%d]: %s\n", ret, msg);   \
        exit(EXIT_FAILURE);                             \
    }

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

    ABT_stream *streams;
    ABT_task *tasks;
    task_arg_t *args;
    streams = (ABT_stream *)malloc(sizeof(ABT_stream) * num_streams);
    tasks = (ABT_task *)malloc(sizeof(ABT_task) * num_tasks);
    args = (task_arg_t *)malloc(sizeof(task_arg_t) * num_tasks);

    /* Initialize */
    ret = ABT_init(argc, argv);
    HANDLE_ERROR(ret, "ABT_init");

    /* Create streams */
    ret = ABT_stream_self(&streams[0]);
    HANDLE_ERROR(ret, "ABT_stream_self");
    for (i = 1; i < num_streams; i++) {
        ret = ABT_stream_create(ABT_SCHEDULER_NULL, &streams[i]);
        HANDLE_ERROR(ret, "ABT_stream_create");
    }

    /* Create tasks with task_func1 */
    for (i = 0; i < num_tasks; i++) {
        size_t num = 100 + i;
        ret = ABT_task_create(ABT_STREAM_NULL,
                              task_func1, (void *)num,
                              NULL);
        HANDLE_ERROR(ret, "ABT_task_create");
    }

    /* Create tasks with task_func2 */
    for (i = 0; i < num_tasks; i++) {
        args[i].num = 100 + i;
        ret = ABT_task_create(streams[i % num_streams],
                              task_func2, (void *)&args[i],
                              &tasks[i]);
        HANDLE_ERROR(ret, "ABT_task_create");
    }

    /* Switch to other work units */
    ABT_thread_yield();

    /* Results of task_funcs2 */
    for (i = 0; i < num_tasks; i++) {
        ABT_task_state state;
        do {
            ABT_task_get_state(tasks[i], &state);
            ABT_thread_yield();
        } while (state != ABT_TASK_STATE_TERMINATED);

        printf("task_func2: num=%lu result=%llu\n",
               args[i].num, args[i].result);

        /* Free named tasks */
        ret = ABT_task_free(&tasks[i]);
        HANDLE_ERROR(ret, "ABT_task_free");
    }

    /* Join streams */
    for (i = 1; i < num_streams; i++) {
        ret = ABT_stream_join(streams[i]);
        HANDLE_ERROR(ret, "ABT_stream_join");
    }

    /* Free streams */
    for (i = 1; i < num_streams; i++) {
        ret = ABT_stream_free(&streams[i]);
        HANDLE_ERROR(ret, "ABT_stream_free");
    }

    /* Finalize */
    ret = ABT_finalize();
    HANDLE_ERROR(ret, "ABT_finalize");

    free(args);
    free(tasks);
    free(streams);

    return EXIT_SUCCESS;
}

