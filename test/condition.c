/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <abt.h>

#define DEFAULT_NUM_XSTREAMS    2
#define DEFAULT_NUM_THREADS     3

#define HANDLE_ERROR(ret,msg)                           \
    if (ret != ABT_SUCCESS) {                           \
        fprintf(stderr, "ERROR[%d]: %s\n", ret, msg);   \
        exit(EXIT_FAILURE);                             \
    }

/* Total number of threads (num_xstreams * num_threads) should be equal to
 * or larger than 3. */
int num_xstreams = DEFAULT_NUM_XSTREAMS;
int num_threads = DEFAULT_NUM_THREADS;

#define TCOUNT          10
#define COUNT_LIMIT     15
int g_counter = 0;
int g_num_incthreads = 0;

ABT_mutex mutex = ABT_MUTEX_NULL;
ABT_cond  cond  = ABT_COND_NULL;

typedef struct thread_arg {
    int sid;    /* stream ID */
    int tid;    /* thread ID */
} thread_arg_t;

void inc_counter(void *arg)
{
    int i;
    thread_arg_t *t_arg = (thread_arg_t *)arg;
    int es_id = t_arg->sid;
    int my_id = t_arg->tid;

    for (i = 0; i < TCOUNT; i++) {
        ABT_mutex_lock(mutex);
        g_counter++;

        if (g_counter == COUNT_LIMIT) {
            ABT_cond_signal(cond);
            printf("[ES%d:TH%d] inc_counter(): threshold(%d) reached\n",
                   es_id, my_id, g_counter);
        }

        ABT_mutex_unlock(mutex);

        ABT_thread_yield();
    }

    ABT_mutex_lock(mutex);
    g_num_incthreads++;
    ABT_cond_wait(cond, mutex);
    ABT_mutex_unlock(mutex);
    printf("[ES%d:TH%d] waken up\n", es_id, my_id);
}

void watch_counter(void *arg)
{
    thread_arg_t *t_arg = (thread_arg_t *)arg;
    int es_id = t_arg->sid;
    int my_id = t_arg->tid;

    printf("[ES%d:TH%d] watch_count(): starting\n", es_id, my_id);

    ABT_mutex_lock(mutex);
    while (g_counter < COUNT_LIMIT) {
        ABT_cond_wait(cond, mutex);
        printf("[ES%d:TH%d] watch_count(): condition signal received\n",
               es_id, my_id);
        g_counter += 100;
        printf("[ES%d:TH%d] watch_count(): count now = %d\n",
               es_id, my_id, g_counter);
    }
    ABT_mutex_unlock(mutex);

    while (g_num_incthreads != (num_xstreams * num_threads - 1)) {
        ABT_thread_yield();
    }
    ABT_mutex_lock(mutex);
    ABT_cond_broadcast(cond);
    printf("[ES%d:TH%d] broadcast the condition\n", es_id, my_id);
    ABT_mutex_unlock(mutex);
}

int main(int argc, char *argv[])
{
    int i, j;
    int ret;
    if (argc > 1) num_xstreams = atoi(argv[1]);
    assert(num_xstreams >= 0);
    if (argc > 2) num_threads = atoi(argv[2]);
    assert(num_threads >= 0);

    if (num_xstreams * num_threads < 3) {
        printf("num_xstreams (%d) * num_threads (%d) < 3\n",
               num_xstreams, num_threads);
        exit(-1);
    }

    ABT_xstream *xstreams;
    thread_arg_t **args;

    xstreams = (ABT_xstream *)malloc(sizeof(ABT_xstream) * num_xstreams);
    args = (thread_arg_t **)malloc(sizeof(thread_arg_t *) * num_xstreams);
    for (i = 0; i < num_xstreams; i++) {
        args[i] = (thread_arg_t *)malloc(sizeof(thread_arg_t) * num_threads);
    }

    /* Initialize */
    ret = ABT_init(argc, argv);
    HANDLE_ERROR(ret, "ABT_init");

    /* Create Execution Streams */
    ret = ABT_xstream_self(&xstreams[0]);
    HANDLE_ERROR(ret, "ABT_xstream_self");
    for (i = 1; i < num_xstreams; i++) {
        ret = ABT_xstream_create(ABT_SCHED_NULL, &xstreams[i]);
        HANDLE_ERROR(ret, "ABT_xstream_create");
    }

    /* Create a mutex */
    ret = ABT_mutex_create(&mutex);
    HANDLE_ERROR(ret, "ABT_mutex_create");

    /* Create a condition variable */
    ret = ABT_cond_create(&cond);
    HANDLE_ERROR(ret, "ABT_cond_create");

    /* Create threads */
    args[0][0].sid = 0;
    args[0][1].tid = 1;
    ret = ABT_thread_create(xstreams[0], watch_counter, (void *)&args[0][0],
            ABT_THREAD_ATTR_NULL, NULL);
    HANDLE_ERROR(ret, "ABT_thread_create");
    for (i = 0; i < num_xstreams; i++) {
        for (j = 0; j < num_threads; j++) {
            if (i == 0 && j == 0) continue;
            int tid = i * num_threads + j + 1;
            args[i][j].sid = i;
            args[i][j].tid = tid;
            ret = ABT_thread_create(xstreams[i],
                    inc_counter, (void *)&args[i][j], ABT_THREAD_ATTR_NULL,
                    NULL);
            HANDLE_ERROR(ret, "ABT_thread_create");
        }
    }

    /* Switch to other user level threads */
    ABT_thread_yield();

    /* Join Execution Streams */
    for (i = 1; i < num_xstreams; i++) {
        ret = ABT_xstream_join(xstreams[i]);
        HANDLE_ERROR(ret, "ABT_xstream_join");
    }

    /* Free the mutex */
    ret = ABT_mutex_free(&mutex);
    HANDLE_ERROR(ret, "ABT_mutex_free");

    /* Free the condition variable */
    ret = ABT_cond_free(&cond);
    HANDLE_ERROR(ret, "ABT_cond_free");

    /* Free Execution Streams */
    for (i = 1; i < num_xstreams; i++) {
        ret = ABT_xstream_free(&xstreams[i]);
        HANDLE_ERROR(ret, "ABT_xstream_free");
    }

    /* Finalize */
    ret = ABT_finalize();
    HANDLE_ERROR(ret, "ABT_finalize");

    printf("g_counter = %d (expected: %d)\n", g_counter,
            100 + (num_xstreams * num_threads - 1) * TCOUNT);

    for (i = 0; i < num_xstreams; i++) {
        free(args[i]);
    }
    free(args);
    free(xstreams);

    return EXIT_SUCCESS;
}

