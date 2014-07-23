/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <abt.h>

#define HANDLE_ERROR(ret,msg)                           \
    if (ret != ABT_SUCCESS) {                           \
        fprintf(stderr, "ERROR[%d]: %s\n", ret, msg);   \
        exit(EXIT_FAILURE);                             \
    }

void thread_func(void *arg)
{
    int i;
    size_t my_id = (size_t)arg;
    printf("[TH%lu]: Hello, world!\n", my_id);

    for(i=0; i<2; i++){
        printf("Thread %lu, it %d\n",my_id,i);
        ABT_thread_yield();
    }
}

int main(int argc, char *argv[])
{
    int ret;

    ABT_xstream xstream, myxstream;
    ABT_thread thread;

    /* Initialize */
    ret = ABT_init(argc, argv);
    HANDLE_ERROR(ret, "ABT_init");

    /* Create Execution Streams */
    ret = ABT_xstream_create(ABT_SCHED_NULL, &xstream);
    HANDLE_ERROR(ret, "ABT_xstream_create");
    ABT_xstream_self(&myxstream);

    /* Create threads */
    size_t tid = 1;
       ret = ABT_thread_create(myxstream,
          thread_func, (void *)tid, ABT_THREAD_ATTR_NULL,
          &thread);
    HANDLE_ERROR(ret, "ABT_thread_create");

    printf("[MAIN] Migrating thread\n");

    /* migrating threads from main xstream */
    ABT_thread_migrate_to(thread, xstream);

    /* Switch to other user level threads */
    ABT_thread_yield();

    /* Join Execution Streams */
    ret = ABT_xstream_join(xstream);
    HANDLE_ERROR(ret, "ABT_xstream_join");

    /* Free Execution Streams */
    ret = ABT_xstream_free(&xstream);
    HANDLE_ERROR(ret, "ABT_xstream_free");

    /* Finalize */
    ret = ABT_finalize();
    HANDLE_ERROR(ret, "ABT_finalize");

    return EXIT_SUCCESS;
}
