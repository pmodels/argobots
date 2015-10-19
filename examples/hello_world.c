/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include "abt.h"

#define NUM_XSTREAMS    4

void task_hello(void *arg)
{
    printf("Task%d: Hello, world!\n", (int)(size_t)arg);
}

void thread_hello(void *arg)
{
    ABT_xstream xstream;
    ABT_pool pool;
    printf("ULT%d: Hello, world!\n", (int)(size_t)arg);

    /* Create a task */
    ABT_xstream_self(&xstream);
    ABT_xstream_get_main_pools(xstream, 1, &pool);
    ABT_task_create(pool, task_hello, arg, NULL);
}

int main(int argc, char *argv[])
{
    ABT_xstream xstreams[NUM_XSTREAMS];
    ABT_pool    pools[NUM_XSTREAMS];
    ABT_thread  threads[NUM_XSTREAMS];
    size_t i;

    ABT_init(argc, argv);

    /* Execution Streams */
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
        ABT_thread_create(pools[i], thread_hello, (void *)i,
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
