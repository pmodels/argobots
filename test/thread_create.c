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

void thread_func(void *arg)
{
    size_t my_id = (size_t)arg;
    printf("[TH%lu]: Hello, world!\n", my_id);
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

    ABT_Stream *streams;
    streams = (ABT_Stream *)malloc(sizeof(ABT_Stream) * num_streams);

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

    /* Create threads */
    for (i = 0; i < num_streams; i++) {
        for (j = 0; j < num_threads; j++) {
            size_t tid = i * num_threads + j + 1;
            ret = ABT_Thread_create(streams[i],
                    thread_func, (void *)tid, 16384,
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

    /* Free streams */
    for (i = 1; i < num_streams; i++) {
        ret = ABT_Stream_free(&streams[i]);
        HANDLE_ERROR(ret, "ABT_Stream_free");
    }

    /* Finalize */
    ret = ABT_Finalize();
    HANDLE_ERROR(ret, "ABT_Finalize");

    free(streams);

    return EXIT_SUCCESS;
}
