#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>
#include "abt.h"

#define DEFAULT_NUM_STREAMS     2
#define DEFAULT_NUM_THREADS     2

typedef struct thread_arg {
    int id;
    int num_threads;
    ABT_Thread *threads;
} thread_arg_t;

ABT_Thread pick_one(ABT_Thread *threads, int num_threads)
{
    int i;
    ABT_Thread next;
    do {
        i = rand() % num_threads;
        next = threads[i];
    } while (ABT_Thread_get_state(next) == ABT_THREAD_STATE_TERMINATED);
    return next;
}

void thread_func(void *arg)
{
    thread_arg_t *t_arg = (thread_arg_t *)arg;
    ABT_Thread next;

    printf("    [T%d]: brefore yield\n", t_arg->id);
    next = pick_one(t_arg->threads, t_arg->num_threads);
    ABT_Thread_yield_to(next);

    printf("    [T%d]: doing something ...\n", t_arg->id);
    next = pick_one(t_arg->threads, t_arg->num_threads);
    ABT_Thread_yield_to(next);

    printf("    [T%d]: after yield\n", t_arg->id);
}

int main(int argc, char *argv[])
{
    int i, j;
    int ret;
    int num_streams = DEFAULT_NUM_STREAMS;
    int num_threads = DEFAULT_NUM_THREADS;
    if (argc > 1) num_streams = atoi(argv[1]);
    assert(num_streams >= 0);
    if (argc > 2) num_threads = atoi(argv[2]);
    assert(num_threads >= 0);

    //srand(time(NULL));

    ABT_Stream *streams;
    ABT_Thread **threads;
    thread_arg_t **args;
    streams = (ABT_Stream *)malloc(sizeof(ABT_Stream) * num_streams);
    threads = (ABT_Thread **)malloc(sizeof(ABT_Thread *) * num_streams);
    args = (thread_arg_t **)malloc(sizeof(thread_arg_t *) * num_streams);
    for (i = 0; i < num_streams; i++) {
        threads[i] = (ABT_Thread *)malloc(sizeof(ABT_Thread) * num_threads);
        args[i] = (thread_arg_t *)malloc(sizeof(thread_arg_t) * num_threads);
    }

    /* Create streams */
    streams[0] = ABT_Stream_self();
    for (i = 1; i < num_streams; i++) {
        ret = ABT_Stream_create(&streams[i]);
        if (ret != ABT_SUCCESS) {
            fprintf(stderr, "ERROR: ABT_Stream_create for ES%d\n", i);
            exit(EXIT_FAILURE);
        }
    }

    /* Create threads */
    for (i = 0; i < num_streams; i++) {
        for (j = 0; j < num_threads; j++) {
            int tid = i * num_threads + j;
            args[i][j].id = tid;
            args[i][j].num_threads = num_threads;
            args[i][j].threads = &threads[i][0];
            ret = ABT_Thread_create(streams[i],
                    thread_func, (void *)&args[i][j], 8192,
                    &threads[i][j]);
            if (ret != ABT_SUCCESS) {
                fprintf(stderr, "ERROR: ABT_Thread_create for ES%d-LUT%d\n", i, j);
                exit(EXIT_FAILURE);
            }
        }
    }

    /* Switch to other user level threads */
    ABT_Thread_yield();

    /* Join streams */
    for (i = 1; i < num_streams; i++) {
        ret = ABT_Stream_join(streams[i]);
        if (ret != ABT_SUCCESS) {
            fprintf(stderr, "ERROR: AB_Stream_join for ES%d\n", i);
            exit(EXIT_FAILURE);
        }
    }

    /* Free streams */
    for (i = 0; i < num_streams; i++) {
        ret = ABT_Stream_free(streams[i]);
        if (ret != ABT_SUCCESS) {
            fprintf(stderr, "ERROR: AB_stream_free for ES%d\n", i);
            exit(EXIT_FAILURE);
        }
    }

    for (i = 0; i < num_streams; i++) {
        free(args[i]);
        free(threads[i]);
    }
    free(args);
    free(threads);
    free(streams);

    return EXIT_SUCCESS;
}
