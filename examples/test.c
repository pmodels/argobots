/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <abt.h>

#define HANDLE_ERROR(ret,msg)                           \
    if (ret != ABT_SUCCESS) {                           \
        fprintf(stderr, "ERROR[%d]: %s\n", ret, msg);   \
        exit(EXIT_FAILURE);                             \
    }

/* global variables */


void hello(void *args){
	printf("Hello world\n");
}

/* Main function */
int main(int argc, char *argv[]){
    size_t n, i;
    int ret;
    ABT_thread thread;

    /* initialization */
    ABT_init(argc, argv);

    ABT_sched sched;
    ABT_pool pool;

		ret = ABT_pool_create(&ABT_pool_fifo, ABT_POOL_CONFIG_NULL, &pool);
    HANDLE_ERROR(ret, "ABT_pool_create");

    ret = ABT_sched_create(&ABT_sched_default, &pool, 1, NULL, &sched);
    HANDLE_ERROR(ret, "ABT_sched_create");

    /* stream creation */
    ABT_xstream xstream;
    ret = ABT_xstream_self(&xstream);
    HANDLE_ERROR(ret, "ABT_xstream_self");

		ret = ABT_xstream_create(sched, &xstream);
		HANDLE_ERROR(ret, "ABT_xstream_create");

    /* control thread creation */
    ABT_thread_create(pool, hello, (void *)n, ABT_THREAD_ATTR_NULL, &thread);
    ABT_thread_yield();

    /* joining control thread */
		ret = ABT_thread_join(thread);
		HANDLE_ERROR(ret, "ABT_thread_join");

    /* join streams */
		ret = ABT_xstream_join(xstream);
		HANDLE_ERROR(ret, "ABT_xstream_join");

    /* deallocating streams */
		ret = ABT_xstream_free(&xstream);
		HANDLE_ERROR(ret, "ABT_xstream_free");

    ABT_finalize();
    return 0;
}
