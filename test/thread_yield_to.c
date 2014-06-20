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
    int num_xstreams = DEFAULT_NUM_XSTREAMS;
    int num_threads = DEFAULT_NUM_THREADS;
    if (argc > 1) num_xstreams = atoi(argv[1]);
    assert(num_xstreams >= 0);
    if (argc > 2) num_threads = atoi(argv[2]);
    assert(num_threads >= 0);

    ABT_xstream *xstreams;
    ABT_thread **threads;
    thread_arg_t **args;
    xstreams = (ABT_xstream *)malloc(sizeof(ABT_xstream) * num_xstreams);
    threads = (ABT_thread **)malloc(sizeof(ABT_thread *) * num_xstreams);
    args = (thread_arg_t **)malloc(sizeof(thread_arg_t *) * num_xstreams);
    for (i = 0; i < num_xstreams; i++) {
        threads[i] = (ABT_thread *)malloc(sizeof(ABT_thread) * num_threads);
        args[i] = (thread_arg_t *)malloc(sizeof(thread_arg_t) * num_threads);
    }

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
            args[i][j].id = tid;
            args[i][j].num_threads = num_threads;
            args[i][j].threads = &threads[i][0];
            ret = ABT_thread_create(xstreams[i],
                    thread_func, (void *)&args[i][j], ABT_THREAD_ATTR_NULL,
                    &threads[i][j]);
            HANDLE_ERROR(ret, "ABT_thread_create");
        }
    }

    /* Switch to other user level threads */
    ABT_thread_yield();

    /* Join Execution Streams */
    for (i = 1; i < num_xstreams; i++) {
        ret = ABT_xstream_join(xstreams[i]);
        HANDLE_ERROR(ret, "ABT_xstream_join");
    }

    /* Free threads and Execution Streams */
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
        free(args[i]);
        free(threads[i]);
    }
    free(args);
    free(threads);
    free(xstreams);

    return EXIT_SUCCESS;
}
