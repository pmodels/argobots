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

int g_counter = 0;

typedef struct thread_arg {
    int id;
    ABT_Mutex mutex;
} thread_arg_t;

void thread_func(void *arg)
{
    thread_arg_t *t_arg = (thread_arg_t *)arg;

    ABT_Thread_yield();

    ABT_Mutex_lock(t_arg->mutex);
    g_counter++;
    ABT_Mutex_unlock(t_arg->mutex);

    ABT_Thread_yield();
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

    ABT_Mutex mutex;
    ABT_Stream *streams;
    thread_arg_t **args;
    streams = (ABT_Stream *)malloc(sizeof(ABT_Stream) * num_streams);
    args = (thread_arg_t **)malloc(sizeof(thread_arg_t *) * num_streams);
    for (i = 0; i < num_streams; i++) {
        args[i] = (thread_arg_t *)malloc(sizeof(thread_arg_t) * num_threads);
    }

    /* Initialize */
    ret = ABT_Init(argc, argv);
    HANDLE_ERROR(ret, "ABT_Init");

    /* Create streams */
    ret = ABT_Stream_self(&streams[0]);
    HANDLE_ERROR(ret, "ABT_Stream_self");
    for (i = 1; i < num_streams; i++) {
        ret = ABT_Stream_create(ABT_SCHEDULER_NULL, &streams[i]);
        HANDLE_ERROR(ret, "ABT_Stream_create");
    }

    /* Create a mutex */
    ret = ABT_Mutex_create(&mutex);
    HANDLE_ERROR(ret, "ABT_Mutex_create");

    /* Create threads */
    for (i = 0; i < num_streams; i++) {
        for (j = 0; j < num_threads; j++) {
            int tid = i * num_threads + j + 1;
            args[i][j].id = tid;
            args[i][j].mutex = mutex;
            ret = ABT_Thread_create(streams[i],
                    thread_func, (void *)&args[i][j], 16384,
                    NULL);
            HANDLE_ERROR(ret, "ABT_Thread_create");
        }
    }

    /* Switch to other user level threads */
    ABT_Thread_yield();

    /* Join streams */
    for (i = 1; i < num_streams; i++) {
        ret = ABT_Stream_join(streams[i]);
        HANDLE_ERROR(ret, "ABT_Stream_join");
    }

    /* Free the mutex */
    ret = ABT_Mutex_free(&mutex);
    HANDLE_ERROR(ret, "ABT_Mutex_free");

    /* Free streams */
    for (i = 1; i < num_streams; i++) {
        ret = ABT_Stream_free(&streams[i]);
        HANDLE_ERROR(ret, "ABT_Stream_free");
    }

    /* Finalize */
    ret = ABT_Finalize();
    HANDLE_ERROR(ret, "ABT_Finalize");

    printf("g_counter = %d\n", g_counter);

    for (i = 0; i < num_streams; i++) {
        free(args[i]);
    }
    free(args);
    free(streams);

    return EXIT_SUCCESS;
}

