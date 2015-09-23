/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include "abt.h"

#define NUM_XSTREAMS    4

void thread_hello(void *arg)
{
    int rank, tid = (int)(size_t)arg;
    ABT_xstream_self_rank(&rank);
    printf("[U%d:E%d] Hello, world!\n", tid, rank);
}

int main(int argc, char *argv[])
{
    ABT_xstream xstreams[NUM_XSTREAMS];
    ABT_pool    pools[NUM_XSTREAMS];
    ABT_thread  threads[NUM_XSTREAMS];
    int i;

    ABT_init(argc, argv);

    /* Create ESs */
    ABT_xstream_self(&xstreams[0]);
    for (i = 1; i < NUM_XSTREAMS; i++) {
        ABT_xstream_create(ABT_SCHED_NULL, &xstreams[i]);
    }

    /* Get the first pool associated with each ES */
    for (i = 0; i < NUM_XSTREAMS; i++) {
        ABT_xstream_get_main_pools(xstreams[i], 1, &pools[i]);
    }

    /* Create ULTs */
    for (i = 0; i < NUM_XSTREAMS; i++) {
        ABT_thread_create(pools[i], thread_hello, (void *)(size_t)i,
                          ABT_THREAD_ATTR_NULL, &threads[i]);
    }

    /* Join & Free */
    for (i = 0; i < NUM_XSTREAMS; i++) {
        ABT_thread_join(threads[i]);
        ABT_thread_free(&threads[i]);
    }
    for (i = 1; i < NUM_XSTREAMS; i++) {
        ABT_xstream_join(xstreams[i]);
        ABT_xstream_free(&xstreams[i]);
    }

    ABT_finalize();

    return 0;
}
