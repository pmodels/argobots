/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include "abt.h"
#include "abttest.h"

#define BUFFER_SIZE 10

ABT_thread th1, th2, th3;
ABT_future myfuture;

#define LOOP_CNT  10
void fn1(void *args)
{
    ABT_TEST_UNUSED(args);
    int i = 0;
    ABT_test_printf(1, "Thread 1 iteration %d waiting for future\n", i);
    ABT_future_wait(myfuture);
    ABT_test_printf(1, "Thread 1 continue iteration %d returning from "
            "future\n", i);
}

void fn2(void *args)
{
    ABT_TEST_UNUSED(args);
    int i = 0;
    ABT_test_printf(1, "Thread 2 iteration %d waiting from future\n", i);
    ABT_future_wait(myfuture);
    ABT_test_printf(1, "Thread 2 continue iteration %d returning from "
            "future\n", i);
}

void fn3(void *args)
{
    ABT_TEST_UNUSED(args);
    int i = 0;
    ABT_test_printf(1, "Thread 3 iteration %d signaling future\n", i);
    char *data = (char *) malloc(BUFFER_SIZE);
    ABT_future_set(myfuture, (void *)data);
    ABT_test_printf(1, "Thread 3 continue iteration %d\n", i);
}

int main(int argc, char *argv[])
{
    int ret;
    ABT_xstream xstream;

    /* init and thread creation */
    ABT_test_init(argc, argv);

    ret = ABT_xstream_self(&xstream);
    ABT_TEST_ERROR(ret, "ABT_xstream_self");

    /* Get the pools attached to an execution stream */
    ABT_pool pool;
    ret = ABT_xstream_get_main_pools(xstream, 1, &pool);
    ABT_TEST_ERROR(ret, "ABT_xstream_get_main_pools");

    ret = ABT_thread_create(pool, fn1, NULL, ABT_THREAD_ATTR_NULL, &th1);
    ABT_TEST_ERROR(ret, "ABT_thread_create");
    ret = ABT_thread_create(pool, fn2, NULL, ABT_THREAD_ATTR_NULL, &th2);
    ABT_TEST_ERROR(ret, "ABT_thread_create");
    ret = ABT_thread_create(pool, fn3, NULL, ABT_THREAD_ATTR_NULL, &th3);
    ABT_TEST_ERROR(ret, "ABT_thread_create");

    ret = ABT_future_create(1, NULL, &myfuture);
    ABT_TEST_ERROR(ret, "ABT_future_create");

    ABT_test_printf(1, "START\n");

    int i = 0;
    ABT_test_printf(1, "Thread main iteration %d waiting from future\n", i);
    ABT_future_wait(myfuture);
    ABT_test_printf(1, "Thread main continue iteration %d returning from "
            "future\n", i);

    /* switch to other user-level threads */
    ABT_thread_yield();

    /* join other threads */
    ret = ABT_thread_join(th1);
    ABT_TEST_ERROR(ret, "ABT_thread_join");
    ret = ABT_thread_join(th2);
    ABT_TEST_ERROR(ret, "ABT_thread_join");
    ret = ABT_thread_join(th3);
    ABT_TEST_ERROR(ret, "ABT_thread_join");

    /* release future */
    ret = ABT_future_free(&myfuture);
    ABT_TEST_ERROR(ret, "ABT_future_free");

    ABT_test_printf(1, "END\n");

    ret = ABT_test_finalize(0);

    return ret;
}
