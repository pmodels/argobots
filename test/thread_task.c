/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <abt.h>

#define DEFAULT_NUM_XSTREAMS    4
#define DEFAULT_NUM_THREADS     4
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

typedef struct thread_arg {
    int id;
    int num_threads;
    ABT_thread *threads;
} thread_arg_t;

ABT_thread pick_one(ABT_thread *threads, int num_threads)
{
    int i;
    ABT_thread next;
    ABT_thread_state state = ABT_THREAD_STATE_TERMINATED;
    while (state == ABT_THREAD_STATE_TERMINATED) {
        i = rand() % num_threads;
        next = threads[i];
        if (next != ABT_THREAD_NULL) {
            ABT_thread_get_state(next, &state);
        }
    }
    return next;
}

void thread_func(void *arg)
{
    thread_arg_t *t_arg = (thread_arg_t *)arg;
    ABT_thread next;

    printf("[TH%d]: brefore yield\n", t_arg->id); fflush(stdout);
    next = pick_one(t_arg->threads, t_arg->num_threads);
    ABT_thread_yield_to(next);

    printf("[TH%d]: doing something ...\n", t_arg->id); fflush(stdout);
    next = pick_one(t_arg->threads, t_arg->num_threads);
    ABT_thread_yield_to(next);

    printf("[TH%d]: after yield\n", t_arg->id); fflush(stdout);
}

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
    int i, j, ret;
    int num_xstreams = DEFAULT_NUM_XSTREAMS;
    int num_threads = DEFAULT_NUM_THREADS;
    int num_tasks = DEFAULT_NUM_TASKS;
    if (argc > 1) num_xstreams = atoi(argv[1]);
    assert(num_xstreams >= 0);
    if (argc > 2) num_threads = atoi(argv[2]);
    assert(num_threads >= 0);
    if (argc > 3) num_tasks = atoi(argv[3]);
    assert(num_tasks >= 0);

    ABT_xstream *xstreams;
    ABT_thread **threads;
    thread_arg_t **thread_args;
    ABT_task *tasks;
    task_arg_t *task_args;

    xstreams = (ABT_xstream *)malloc(sizeof(ABT_xstream) * num_xstreams);
    threads = (ABT_thread **)malloc(sizeof(ABT_thread *) * num_xstreams);
    thread_args = (thread_arg_t **)malloc(sizeof(thread_arg_t*) * num_xstreams);
    for (i = 0; i < num_xstreams; i++) {
        threads[i] = (ABT_thread *)malloc(sizeof(ABT_thread) * num_threads);
        for (j = 0; j < num_threads; j++) {
            threads[i][j] = ABT_THREAD_NULL;
        }
        thread_args[i] = (thread_arg_t *)malloc(sizeof(thread_arg_t) *
                                                num_threads);
    }
    tasks = (ABT_task *)malloc(sizeof(ABT_task) * num_tasks);
    task_args = (task_arg_t *)malloc(sizeof(task_arg_t) * num_tasks);

    /* Initialize */
    ret = ABT_init(argc, argv);
    HANDLE_ERROR(ret, "ABT_init");

    /* Create Execution Streams */
    ret = ABT_xstream_self(&xstreams[0]);
    HANDLE_ERROR(ret, "ABT_xstream_self");
    for (i = 1; i < num_xstreams; i++) {
        ret = ABT_xstream_create(ABT_SCHED_NULL, &xstreams[i]);
        HANDLE_ERROR(ret, "ABT_xstream_create");
    }

    /* Create threads */
    for (i = 0; i < num_xstreams; i++) {
        for (j = 0; j < num_threads; j++) {
            int tid = i * num_threads + j + 1;
            thread_args[i][j].id = tid;
            thread_args[i][j].num_threads = num_threads;
            thread_args[i][j].threads = &threads[i][0];
            ret = ABT_thread_create(xstreams[i],
                    thread_func, (void *)&thread_args[i][j],
                    ABT_THREAD_ATTR_NULL,
                    &threads[i][j]);
            HANDLE_ERROR(ret, "ABT_thread_create");
        }
    }

    /* Create tasks with task_func1 */
    for (i = 0; i < num_tasks; i++) {
        size_t num = 100 + i;
        ret = ABT_task_create(ABT_XSTREAM_NULL,
                              task_func1, (void *)num,
                              NULL);
        HANDLE_ERROR(ret, "ABT_task_create");
    }

    /* Create tasks with task_func2 */
    for (i = 0; i < num_tasks; i++) {
        task_args[i].num = 100 + i;
        ret = ABT_task_create(xstreams[i % num_xstreams],
                              task_func2, (void *)&task_args[i],
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
               task_args[i].num, task_args[i].result);

        /* Free named tasks */
        ret = ABT_task_free(&tasks[i]);
        HANDLE_ERROR(ret, "ABT_task_free");
    }

    /* Join Execution Streams */
    for (i = 1; i < num_xstreams; i++) {
        ret = ABT_xstream_join(xstreams[i]);
        HANDLE_ERROR(ret, "ABT_xstream_join");
    }

    /* Free Execution Streams */
    for (i = 0; i < num_xstreams; i++) {
        for (j = 0; j < num_threads; j++) {
            ret = ABT_thread_free(&threads[i][j]);
            HANDLE_ERROR(ret, "ABT_thread_free");
        }

        if (i == 0) continue;

        ret = ABT_xstream_free(&xstreams[i]);
        HANDLE_ERROR(ret, "ABT_xstream_free");
    }

    /* Finalize */
    ret = ABT_finalize();
    HANDLE_ERROR(ret, "ABT_finalize");

    for (i = 0; i < num_xstreams; i++) {
        free(thread_args[i]);
        free(threads[i]);
    }
    free(thread_args);
    free(threads);
    free(task_args);
    free(tasks);
    free(xstreams);

    return EXIT_SUCCESS;
}

