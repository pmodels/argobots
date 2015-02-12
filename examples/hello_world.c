/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include "abt.h"

void task_hello(void *arg)
{
    printf("Task%d: Hello, world!\n", (int)(size_t)arg);
}

void thread_hello(void *arg)
{
    ABT_xstream xstream;
    printf("ULT%d: Hello, world!\n", (int)(size_t)arg);

    /* Create a task */
    ABT_xstream_self(&xstream);
    ABT_task_create(xstream, task_hello, arg, NULL);
}

int main(int argc, char *argv[])
{
    ABT_xstream xstreams[2];
    ABT_thread threads[2];
    size_t i;

    ABT_init(argc, argv);

    /* Execution Streams */
    ABT_xstream_self(&xstreams[0]);
    ABT_xstream_create(ABT_SCHED_NULL, &xstreams[1]);

    /* Create ULTs */
    for (i = 0; i < 2; i++) {
        ABT_thread_create(xstreams[i], thread_hello, (void *)i,
                          ABT_THREAD_ATTR_NULL, &threads[i]);
    }

    /* Switch to other work units */
    ABT_thread_yield();

    /* Join & Free */
    for (i = 0; i < 2; i++) {
        ABT_thread_join(threads[i]);
        ABT_thread_free(&threads[i]);
    }
    ABT_xstream_join(xstreams[1]);
    ABT_xstream_free(&xstreams[1]);

    ABT_finalize();

    return 0;
}
