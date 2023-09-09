/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "abt.h"

#define NUM_XSTREAMS 4
#define NUM_THREADS 4


static void create_threads(void *arg);
static void thread_hello(void *arg);

int main(int argc, char *argv[])
{
    ABT_xstream xstreams[NUM_XSTREAMS];
    ABT_sched scheds[NUM_XSTREAMS];
    ABT_pool pools[NUM_XSTREAMS];
    ABT_thread threads[NUM_XSTREAMS];
    int i;

    ABT_init(argc, argv);

    /* Create pools */
    for (i = 0; i < NUM_XSTREAMS; i++) {
        ABT_pool_create_basic(ABT_POOL_FIFO, ABT_POOL_ACCESS_MPMC, ABT_TRUE,
                              &pools[i]);
    }

    /* Create schedulers */
    //create_scheds(NUM_XSTREAMS, pools, scheds);
    ABT_sched_policy policies[NUM_XSTREAMS];
    for (int i = 0; i < NUM_XSTREAMS; i++) {
      policies[i].ready_at = 0;
      policies[i].ready_at_wt = ABT_get_wtime();
      policies[i].priority = i;
      policies[i].max_successes = 10;
      policies[i].min_successes = 2 * i + 2;
      policies[i].success_timeout = 5;
      policies[i].fail_timeout = 100;
      policies[i].success_timeout_wt = 0.0;
      policies[i].fail_timeout_wt = 5.0;
    }

    policies[0].max_successes = 100;
    policies[0].fail_timeout_wt=10.0;
    for (int i = 0; i < NUM_XSTREAMS; i++) {
      for (int j = 0; j < NUM_XSTREAMS;j++) {
        policies[(i+j) % NUM_XSTREAMS].priority = j;
      }
      ABT_sched_create_reg(NUM_XSTREAMS, pools, NUM_XSTREAMS, policies, &scheds[i]);
    }

    /* Create ESs */
    ABT_xstream_self(&xstreams[0]);
    ABT_xstream_set_main_sched(xstreams[0], scheds[0]);
    for (i = 1; i < NUM_XSTREAMS; i++) {
        ABT_xstream_create(scheds[i], &xstreams[i]);
    }

    /* Create ULTs */
    for (i = 0; i < NUM_XSTREAMS; i++) {
        size_t tid = (size_t)i;
        ABT_thread_create(pools[i], create_threads, (void *)tid,
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
    /* Free schedulers */
    /* Note that we do not need to free the scheduler for the primary ES,
     * i.e., xstreams[0], because its scheduler will be automatically freed in
     * ABT_finalize(). */
    for (i = 1; i < NUM_XSTREAMS; i++) {
        ABT_sched_free(&scheds[i]);
    }

    /* Finalize */
    ABT_finalize();

    return 0;
}

static void create_threads(void *arg)
{
    int i, rank, tid = (int)(size_t)arg;
    ABT_xstream xstream;
    ABT_pool pools[NUM_XSTREAMS];
    ABT_thread *threads;

    ABT_xstream_self(&xstream);
    ABT_xstream_get_main_pools(xstream, NUM_XSTREAMS, pools);

    ABT_xstream_get_rank(xstream, &rank);
    printf("[U%d:E%d] creating ULTs\n", tid, rank);

    threads = (ABT_thread *)malloc(sizeof(ABT_thread) * NUM_THREADS);
    for (i = 0; i < NUM_THREADS; i++) {
        size_t id = (rank + 1) * 10 + i;
        ABT_thread_create(pools[rank], thread_hello, (void *)id, ABT_THREAD_ATTR_NULL,
                          &threads[i]);
    }

    ABT_xstream_get_rank(xstream, &rank);
    printf("[U%d:E%d] freeing ULTs\n", tid, rank);
    for (i = 0; i < NUM_THREADS; i++) {
        ABT_thread_free(&threads[i]);
    }
    free(threads);
}

static void thread_hello(void *arg)
{
    int tid = (int)(size_t)arg;
    int old_rank, cur_rank;
    char *msg;

    ABT_xstream_self_rank(&cur_rank);

    printf("  [U%d:E%d] Hello, world!\n", tid, cur_rank);

    ABT_thread_yield();

    old_rank = cur_rank;
    ABT_xstream_self_rank(&cur_rank);
    msg = (cur_rank == old_rank) ? "" : " (stolen)";
    printf("  [U%d:E%d] Hello again.%s\n", tid, cur_rank, msg);

    ABT_thread_yield();

    old_rank = cur_rank;
    ABT_xstream_self_rank(&cur_rank);
    msg = (cur_rank == old_rank) ? "" : " (stolen)";
    printf("  [U%d:E%d] Goodbye, world!%s\n", tid, cur_rank, msg);
}
