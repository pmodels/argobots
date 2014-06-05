/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <abt.h>

#define DEFAULT_NUM_STREAMS     4
#define DEFAULT_NUM_THREADS     4

#define HANDLE_ERROR(ret,msg)                           \
    if (ret != ABT_SUCCESS) {                           \
        fprintf(stderr, "ERROR[%d]: %s\n", ret, msg);   \
        exit(EXIT_FAILURE);                             \
    }

typedef struct thread_arg {
    int id;
    int num_threads;
    ABT_thread *threads;
} thread_arg_t;

ABT_thread pick_one(ABT_thread *threads, int num_threads)
{
    int i;
    ABT_thread next;
    ABT_thread_state state;
    do {
        i = rand() % num_threads;
        next = threads[i];
        ABT_thread_get_state(next, &state);
    } while (state == ABT_THREAD_STATE_TERMINATED);
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

    ABT_stream *streams;
    ABT_thread **threads;
    thread_arg_t **args;
    streams = (ABT_stream *)malloc(sizeof(ABT_stream) * num_streams);
    threads = (ABT_thread **)malloc(sizeof(ABT_thread *) * num_streams);
    args = (thread_arg_t **)malloc(sizeof(thread_arg_t *) * num_streams);
    for (i = 0; i < num_streams; i++) {
        threads[i] = (ABT_thread *)malloc(sizeof(ABT_thread) * num_threads);
        args[i] = (thread_arg_t *)malloc(sizeof(thread_arg_t) * num_threads);
    }

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

    /* Create threads */
    for (i = 0; i < num_streams; i++) {
        for (j = 0; j < num_threads; j++) {
            int tid = i * num_threads + j + 1;
            args[i][j].id = tid;
            args[i][j].num_threads = num_threads;
            args[i][j].threads = &threads[i][0];
            ret = ABT_thread_create(streams[i],
                    thread_func, (void *)&args[i][j], 16384,
                    &threads[i][j]);
            HANDLE_ERROR(ret, "ABT_thread_create");
        }
    }

    /* Switch to other user level threads */
    ABT_thread_yield();

    /* Join streams */
    for (i = 1; i < num_streams; i++) {
        ret = ABT_stream_join(streams[i]);
        HANDLE_ERROR(ret, "ABT_stream_join");
    }

    /* Free threads and streams */
    for (i = 0; i < num_streams; i++) {
        for (j = 0; j < num_threads; j++) {
            ret = ABT_thread_free(&threads[i][j]);
            HANDLE_ERROR(ret, "ABT_thread_free");
        }

        if (i == 0) continue;

        ret = ABT_stream_free(&streams[i]);
        HANDLE_ERROR(ret, "ABT_stream_free");
    }

    /* Finalize */
    ret = ABT_finalize();
    HANDLE_ERROR(ret, "ABT_finalize");

    for (i = 0; i < num_streams; i++) {
        free(args[i]);
        free(threads[i]);
    }
    free(args);
    free(threads);
    free(streams);

    return EXIT_SUCCESS;
}
