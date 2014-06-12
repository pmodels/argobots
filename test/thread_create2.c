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

int num_threads = DEFAULT_NUM_THREADS;

void thread_func(void *arg)
{
    size_t my_id = (size_t)arg;
    printf("[TH%lu]: Hello, world!\n", my_id);
}

void thread_create(void *arg)
{
    int i, ret;
    size_t my_id = (size_t)arg;
    ABT_xstream my_xstream;

    ret = ABT_xstream_self(&my_xstream);
    HANDLE_ERROR(ret, "ABT_xstream_self");

    /* Create threads */
    for (i = 0; i < num_threads; i++) {
        size_t tid = 100 * my_id + i;
        ret = ABT_thread_create(my_xstream,
                thread_func, (void *)tid, 16384,
                NULL);
        HANDLE_ERROR(ret, "ABT_thread_create");
    }

    printf("[TH%lu]: created %d threads\n", my_id, num_threads);
}

int main(int argc, char *argv[])
{
    int i;
    int ret;
    int num_xstreams = DEFAULT_NUM_XSTREAMS;
    if (argc > 1) num_xstreams = atoi(argv[1]);
    assert(num_xstreams >= 0);
    if (argc > 2) num_threads = atoi(argv[2]);
    assert(num_threads >= 0);

    ABT_xstream *xstreams;
    xstreams = (ABT_xstream *)malloc(sizeof(ABT_xstream) * num_xstreams);

    /* Initialize */
    ret = ABT_init(argc, argv);
    HANDLE_ERROR(ret, "ABT_init");

    /* Create Execution Streams */
    ret = ABT_xstream_self(&xstreams[0]);
    HANDLE_ERROR(ret, "ABT_xstream_self");
    for (i = 1; i < num_xstreams; i++) {
        ret = ABT_xstream_create(ABT_SCHEDULER_NULL, &xstreams[i]);
        HANDLE_ERROR(ret, "ABT_xstream_create");
    }

    /* Create one thread for each ES */
    for (i = 0; i < num_xstreams; i++) {
        size_t tid = i + 1;
        ret = ABT_thread_create(xstreams[i],
                thread_create, (void *)tid, 16384,
                NULL);
        HANDLE_ERROR(ret, "ABT_thread_create");
    }

    /* Switch to other user level threads */
    ABT_thread_yield();

    /* Join Execution Streams */
    for (i = 1; i < num_xstreams; i++) {
        ret = ABT_xstream_join(xstreams[i]);
        HANDLE_ERROR(ret, "ABT_xstream_join");
    }

    /* Free Execution Streams */
    for (i = 1; i < num_xstreams; i++) {
        ret = ABT_xstream_free(&xstreams[i]);
        HANDLE_ERROR(ret, "ABT_xstream_free");
    }

    /* Finalize */
    ret = ABT_finalize();
    HANDLE_ERROR(ret, "ABT_finalize");

    free(xstreams);

    return EXIT_SUCCESS;
}

