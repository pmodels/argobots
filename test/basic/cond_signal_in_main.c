/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "abt.h"
#include "abttest.h"

ABT_mutex mutex = ABT_MUTEX_NULL;
ABT_cond cond = ABT_COND_NULL;

void wait_on_condition(void *arg)
{
    ABT_TEST_UNUSED(arg);
    ABT_mutex_lock(mutex);
    ABT_cond_wait(cond, mutex);
    ABT_mutex_unlock(mutex);
}

int main(int argc, char *argv[])
{
    int ret;

    ABT_xstream xstream;

    /* Initialize */
    ABT_test_init(argc, argv);

    /* Create Execution Streams */
    ret = ABT_xstream_self(&xstream);
    ABT_TEST_ERROR(ret, "ABT_xstream_self");

    /* Get the pools attached to the execution stream */
    ABT_pool pool;
    ret = ABT_xstream_get_main_pools(xstream, 1, &pool);
    ABT_TEST_ERROR(ret, "ABT_xstream_get_main_pools");

    /* Create a mutex */
    ret = ABT_mutex_create(&mutex);
    ABT_TEST_ERROR(ret, "ABT_mutex_create");

    /* Create condition variables */
    ret = ABT_cond_create(&cond);
    ABT_TEST_ERROR(ret, "ABT_cond_create");

    /* Create the ULT */
    ABT_thread thread;
    ret = ABT_thread_create(pool, wait_on_condition, NULL,
                            ABT_THREAD_ATTR_NULL, &thread);
    ABT_TEST_ERROR(ret, "ABT_thread_create");

    /* Switch to the other user level thread */
    ABT_thread_yield();

    ABT_mutex_lock(mutex);
    ret = ABT_cond_signal(cond);
    ABT_mutex_unlock(mutex);
    ABT_TEST_ERROR(ret, "ABT_cond_signal");

    /* Wait for the ULT and free it */
    ABT_thread_free(&thread);

    /* Free the mutex */
    ret = ABT_mutex_free(&mutex);
    ABT_TEST_ERROR(ret, "ABT_mutex_free");

    /* Free the condition variables */
    ret = ABT_cond_free(&cond);
    ABT_TEST_ERROR(ret, "ABT_cond_free");

    /* Finalize */
    ret = ABT_test_finalize(0);

    return ret;
}

