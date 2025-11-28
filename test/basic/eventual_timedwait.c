/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include "abt.h"
#include "abttest.h"

#define DEFAULT_NUM_XSTREAMS 1
#define DEFAULT_NUM_THREADS 4

static ABT_eventual eventual = ABT_EVENTUAL_NULL;
static volatile int g_timeout_counter = 0;
static volatile int g_success_counter = 0;
static volatile int g_waiting_counter = 0;

void eventual_timeout_test(void *arg)
{
    int ret;
    struct timespec ts;
    struct timeval tv;
    int eid;
    ABT_unit_id tid;
    void *value = NULL;

    ret = ABT_xstream_self_rank(&eid);
    ATS_ERROR(ret, "ABT_xstream_self_rank");
    ret = ABT_thread_self_id(&tid);
    ATS_ERROR(ret, "ABT_thread_self_id");

    /* Set timeout to 1 second in the past to force timeout */
    ret = gettimeofday(&tv, NULL);
    assert(!ret);

    ts.tv_sec = tv.tv_sec - 1;
    ts.tv_nsec = tv.tv_usec * 1000;

    ATS_printf(1, "[U%d:E%d] waiting with past deadline (should timeout)\n",
               (int)tid, eid);
    ret = ABT_eventual_timedwait(eventual, &value, &ts);
    if (ret == ABT_ERR_COND_TIMEDOUT) {
        g_timeout_counter++;
        ATS_printf(1, "[U%d:E%d] eventual timed out\n", (int)tid, eid);
    } else {
        ATS_ERROR(ret, "ABT_eventual_timedwait should have timed out");
    }
}

void eventual_success_test(void *arg)
{
    int ret;
    struct timespec ts;
    struct timeval tv;
    int eid;
    ABT_unit_id tid;
    void *value = NULL;

    ret = ABT_xstream_self_rank(&eid);
    ATS_ERROR(ret, "ABT_xstream_self_rank");
    ret = ABT_thread_self_id(&tid);
    ATS_ERROR(ret, "ABT_thread_self_id");

    /* Set timeout to 10 seconds in the future */
    ret = gettimeofday(&tv, NULL);
    assert(!ret);

    ts.tv_sec = tv.tv_sec + 10;
    ts.tv_nsec = tv.tv_usec * 1000;

    ATS_printf(1, "[U%d:E%d] waiting with future deadline (should succeed)\n",
               (int)tid, eid);
    ret = ABT_eventual_timedwait(eventual, &value, &ts);
    if (ret == ABT_SUCCESS) {
        g_success_counter++;
        int *result = (int *)value;
        ATS_printf(1, "[U%d:E%d] eventual signaled, value=%d\n", (int)tid, eid,
                   result ? *result : -1);
    } else if (ret == ABT_ERR_COND_TIMEDOUT) {
        ATS_printf(1, "[U%d:E%d] unexpected timeout\n", (int)tid, eid);
    } else {
        ATS_ERROR(ret, "ABT_eventual_timedwait");
    }
}

int main(int argc, char *argv[])
{
    ABT_xstream *xstreams;
    ABT_pool *pools;
    ABT_thread *timeout_threads;
    ABT_thread *success_threads;
    int num_xstreams;
    int num_timeout_threads;
    int num_success_threads;
    int ret, i, pidx = 0;
    int eid;
    ABT_unit_id tid;
    int test_value = 42;

    /* Initialize */
    ATS_read_args(argc, argv);
    if (argc < 2) {
        num_xstreams = DEFAULT_NUM_XSTREAMS;
        num_timeout_threads = DEFAULT_NUM_THREADS / 2;
        num_success_threads = DEFAULT_NUM_THREADS / 2;
    } else {
        num_xstreams = ATS_get_arg_val(ATS_ARG_N_ES);
        int total_threads = ATS_get_arg_val(ATS_ARG_N_ULT);
        num_timeout_threads = total_threads / 2;
        num_success_threads = total_threads - num_timeout_threads;
    }
    ATS_init(argc, argv, num_xstreams);

    ATS_printf(1, "# of ESs : %d\n", num_xstreams);
    ATS_printf(1, "# of timeout ULTs: %d\n", num_timeout_threads);
    ATS_printf(1, "# of success ULTs: %d\n", num_success_threads);

    ret = ABT_xstream_self_rank(&eid);
    ATS_ERROR(ret, "ABT_xstream_self_rank");
    ret = ABT_thread_self_id(&tid);
    ATS_ERROR(ret, "ABT_thread_self_id");

    xstreams = (ABT_xstream *)malloc(num_xstreams * sizeof(ABT_xstream));
    pools = (ABT_pool *)malloc(num_xstreams * sizeof(ABT_pool));
    timeout_threads =
        (ABT_thread *)malloc(num_timeout_threads * sizeof(ABT_thread));
    success_threads =
        (ABT_thread *)malloc(num_success_threads * sizeof(ABT_thread));

    /* Create an eventual */
    ret = ABT_eventual_create(sizeof(int), &eventual);
    ATS_ERROR(ret, "ABT_eventual_create");

    /* Create ESs */
    ret = ABT_xstream_self(&xstreams[0]);
    ATS_ERROR(ret, "ABT_xstream_self");
    for (i = 1; i < num_xstreams; i++) {
        ret = ABT_xstream_create(ABT_SCHED_NULL, &xstreams[i]);
        ATS_ERROR(ret, "ABT_xstream_create");
    }

    /* Get the pools */
    for (i = 0; i < num_xstreams; i++) {
        ret = ABT_xstream_get_main_pools(xstreams[i], 1, &pools[i]);
        ATS_ERROR(ret, "ABT_xstream_get_main_pools");
    }

    /* Test 1: Timeout threads - should all timeout */
    ATS_printf(1, "\n=== Test 1: Timeout test ===\n");
    for (i = 0; i < num_timeout_threads; i++) {
        ret = ABT_thread_create(pools[pidx], eventual_timeout_test, NULL,
                                ABT_THREAD_ATTR_NULL, &timeout_threads[i]);
        ATS_ERROR(ret, "ABT_thread_create");
        pidx = (pidx + 1) % num_xstreams;
    }

    /* Give threads time to start */
    for (i = 0; i < num_timeout_threads; i++) {
        ABT_thread_yield();
    }
    /* Give extra time for cross-stream scheduling */
    ABT_xstream_check_events(ABT_SCHED_NULL);

    /* Wait for timeout threads to complete */
    for (i = 0; i < num_timeout_threads; i++) {
        ret = ABT_thread_free(&timeout_threads[i]);
        ATS_ERROR(ret, "ABT_thread_free");
    }

    /* Reset the eventual for next test */
    ret = ABT_eventual_reset(eventual);
    ATS_ERROR(ret, "ABT_eventual_reset");

    /* Test 2: Success threads - create threads that wait with future deadline
     */
    ATS_printf(1, "\n=== Test 2: Success test (with signal) ===\n");
    for (i = 0; i < num_success_threads; i++) {
        ret = ABT_thread_create(pools[pidx], eventual_success_test, NULL,
                                ABT_THREAD_ATTR_NULL, &success_threads[i]);
        ATS_ERROR(ret, "ABT_thread_create");
        pidx = (pidx + 1) % num_xstreams;
    }

    /* Give threads time to start waiting */
    for (i = 0; i < num_success_threads; i++) {
        ABT_thread_yield();
    }
    /* Give extra time for cross-stream scheduling */
    ABT_xstream_check_events(ABT_SCHED_NULL);

    /* Signal the eventual */
    ret = ABT_eventual_set(eventual, &test_value, sizeof(int));
    ATS_ERROR(ret, "ABT_eventual_set");
    ATS_printf(1, "[U%d:E%d] eventual_set with value=%d\n", (int)tid, eid,
               test_value);

    /* Wait for success threads to complete */
    for (i = 0; i < num_success_threads; i++) {
        ret = ABT_thread_free(&success_threads[i]);
        ATS_ERROR(ret, "ABT_thread_free");
    }

    /* Test 3: Wait on already ready eventual (should return immediately) */
    ATS_printf(1, "\n=== Test 3: Already ready test ===\n");
    struct timespec ts;
    struct timeval tv;
    void *value = NULL;

    ret = gettimeofday(&tv, NULL);
    assert(!ret);
    ts.tv_sec = tv.tv_sec + 1;
    ts.tv_nsec = tv.tv_usec * 1000;

    ret = ABT_eventual_timedwait(eventual, &value, &ts);
    if (ret == ABT_SUCCESS) {
        int *result = (int *)value;
        ATS_printf(1,
                   "[U%d:E%d] already ready eventual returned immediately, "
                   "value=%d\n",
                   (int)tid, eid, result ? *result : -1);
        if (result && *result == test_value) {
            ATS_printf(1, "Value matches expected value\n");
        }
    } else {
        ATS_ERROR(ret, "ABT_eventual_timedwait on ready eventual");
    }

    /* Join and free ESs */
    for (i = 1; i < num_xstreams; i++) {
        ret = ABT_xstream_join(xstreams[i]);
        ATS_ERROR(ret, "ABT_xstream_join");
        ret = ABT_xstream_free(&xstreams[i]);
        ATS_ERROR(ret, "ABT_xstream_free");
    }

    /* Free the eventual */
    ret = ABT_eventual_free(&eventual);
    ATS_ERROR(ret, "ABT_eventual_free");

    /* Validation */
    int expected_timeouts = num_timeout_threads;
    int expected_success = num_success_threads;
    int failed = 0;

    if (g_timeout_counter != expected_timeouts) {
        printf("ERROR: timeout_counter = %d (expected: %d)\n",
               g_timeout_counter, expected_timeouts);
        failed = 1;
    }
    if (g_success_counter != expected_success) {
        printf("ERROR: success_counter = %d (expected: %d)\n",
               g_success_counter, expected_success);
        failed = 1;
    }

    if (!failed) {
        ATS_printf(1, "\n=== All tests passed ===\n");
        ATS_printf(1, "Timeouts: %d/%d\n", g_timeout_counter,
                   expected_timeouts);
        ATS_printf(1, "Success: %d/%d\n", g_success_counter, expected_success);
    }

    /* Finalize */
    ret = ATS_finalize(failed);

    free(success_threads);
    free(timeout_threads);
    free(pools);
    free(xstreams);

    return ret;
}
