/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <abt.h>

#define DEFAULT_NUM_THREADS     4

#define HANDLE_ERROR(ret,msg)                           \
    if (ret != ABT_SUCCESS) {                           \
        fprintf(stderr, "ERROR[%d]: %s\n", ret, msg);   \
        exit(EXIT_FAILURE);                             \
    }

void thread_func(void *arg)
{
    size_t my_id = (size_t) arg;
    printf("[TH%lu]: brefore yield\n", my_id);
    ABT_thread_yield();
    printf("[TH%lu]: doing something ...\n", my_id);
    ABT_thread_yield();
    printf("[TH%lu]: after yield\n", my_id);
}

int main(int argc, char *argv[])
{
    int i;
    int ret;
    int num_threads = DEFAULT_NUM_THREADS;
    if (argc > 1)
        num_threads = atoi(argv[1]);
    assert(num_threads >= 0);

    ABT_xstream xstreams;

    /* Initialize */
    ret = ABT_init(argc, argv);
    HANDLE_ERROR(ret, "ABT_init");

    /* Create Execution Streams */
    ret = ABT_xstream_self(&xstreams);
    HANDLE_ERROR(ret, "ABT_xstream_self");

    /* Create threads */
    for (i = 0; i < num_threads; i++) {
        size_t tid = i + 1;
        ret = ABT_thread_create(xstreams,
                                thread_func, (void *) tid,
                                ABT_THREAD_ATTR_NULL, NULL);
        HANDLE_ERROR(ret, "ABT_thread_create");
    }

    /* Switch to other user level threads */
    ABT_thread_yield();

    /* Finalize */
    ret = ABT_finalize();
    HANDLE_ERROR(ret, "ABT_finalize");

    return EXIT_SUCCESS;
}
