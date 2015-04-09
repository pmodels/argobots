/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include "abt.h"
#include "abttest.h"


enum {
    T_CREATE_JOIN = 0,
    T_CREATE_UNNAMED,
    T_LAST
};
static char *t_names[] = {
    "create/join",
    "create (unnamed)",
};

static int iter;
static int num_xstreams;
static int num_tasks;

static ABT_xstream *g_xstreams;
static ABT_pool *g_pools;
static ABT_task **g_tasks;

static uint64_t t_times[T_LAST];


void task_func(void *arg)
{
    ABT_TEST_UNUSED(arg);
}

void test_create_join(void *arg)
{
    int eid = (int)(size_t)arg;
    ABT_pool  my_pool  = g_pools[eid];
    ABT_task *my_tasks = g_tasks[eid];
    int i, t;

    for (i = 0; i < iter; i++) {
        for (t = 0; t < num_tasks; t++) {
            ABT_task_create(my_pool, task_func, NULL, &my_tasks[t]);
        }

        for (t = 0; t < num_tasks; t++) {
            ABT_task_free(&my_tasks[t]);
        }
    }
}

void test_create_unnamed(void *arg)
{
    int eid = (int)(size_t)arg;
    ABT_pool  my_pool  = g_pools[eid];
    int i, t;

    for (i = 0; i < iter; i++) {
        for (t = 0; t < num_tasks; t++) {
            ABT_task_create(my_pool, task_func, NULL, NULL);
        }
        ABT_thread_yield();
    }
}

int main(int argc, char *argv[])
{
    ABT_pool (*all_pools)[2];
    ABT_sched *scheds;
    ABT_thread *top_threads;
    size_t i, t;
    uint64_t t_start;

    /* initialize */
    ABT_test_init(argc, argv);

    for (i = 0; i < T_LAST; i++) {
        t_times[i] = 0;
    }

    /* read command-line arguments */
    num_xstreams = ABT_test_get_arg_val(ABT_TEST_ARG_N_ES);
    num_tasks    = ABT_test_get_arg_val(ABT_TEST_ARG_N_TASK);
    iter = ABT_test_get_arg_val(ABT_TEST_ARG_N_ITER);

    g_xstreams = (ABT_xstream *)malloc(num_xstreams * sizeof(ABT_xstream));
    g_pools    = (ABT_pool *)malloc(num_xstreams * sizeof(ABT_pool));
    g_tasks    = (ABT_task **)malloc(num_xstreams * sizeof(ABT_task *));
    for (i = 0; i < num_xstreams; i++) {
        g_tasks[i] = (ABT_task *)malloc(num_tasks * sizeof(ABT_task));
    }
    all_pools = (ABT_pool (*)[2])malloc(num_xstreams * sizeof(ABT_pool) * 2);
    scheds = (ABT_sched *)malloc(num_xstreams * sizeof(ABT_sched));
    top_threads = (ABT_thread *)malloc(num_xstreams * sizeof(ABT_thread));

    /* create g_pools and schedulers */
    for (i = 0; i < num_xstreams; i++) {
        ABT_pool_create_basic(ABT_POOL_FIFO, ABT_POOL_ACCESS_MPSC, ABT_TRUE,
                              &all_pools[i][0]);
        ABT_pool_create_basic(ABT_POOL_FIFO, ABT_POOL_ACCESS_PRIV, ABT_TRUE,
                              &all_pools[i][1]);
        g_pools[i] = all_pools[i][1];

        ABT_sched_create_basic(ABT_SCHED_DEFAULT, 2, all_pools[i],
                               ABT_SCHED_CONFIG_NULL, &scheds[i]);
    }

    /* create ESs */
    ABT_xstream_self(&g_xstreams[0]);
    ABT_xstream_set_main_sched(g_xstreams[0], scheds[0]);
    for (i = 1; i < num_xstreams; i++) {
        ABT_xstream_create(scheds[i], &g_xstreams[i]);
    }

    /* benchmarking */
    for (t = 0; t < T_LAST; t++) {
        void (*test_fn)(void *);

        switch (t) {
            case T_CREATE_JOIN:    test_fn = test_create_join;
                                   break;
            case T_CREATE_UNNAMED: test_fn = test_create_unnamed;
                                   break;
            default: assert(0);
        }

        /* warm-up */
        for (i = 0; i < num_xstreams; i++) {
            ABT_thread_create(all_pools[i][0], test_fn, (void *)i,
                              ABT_THREAD_ATTR_NULL, &top_threads[i]);
        }
        for (i = 0; i < num_xstreams; i++) {
            ABT_thread_free(&top_threads[i]);
        }

        /* measurement */
        t_start = ABT_test_get_cycles();
        for (i = 0; i < num_xstreams; i++) {
            ABT_thread_create(all_pools[i][0], test_fn, (void *)i,
                              ABT_THREAD_ATTR_NULL, &top_threads[i]);
        }
        for (i = 0; i < num_xstreams; i++) {
            ABT_thread_free(&top_threads[i]);
        }
        t_times[t] = ABT_test_get_cycles() - t_start;
    }

    /* join and free */
    for (i = 1; i < num_xstreams; i++) {
        ABT_xstream_join(g_xstreams[i]);
        ABT_xstream_free(&g_xstreams[i]);
    }

    /* finalize */
    ABT_test_finalize(0);

    /* compute the execution time for one iteration */
    for (i = 0; i < T_LAST; i++) {
        t_times[i] = t_times[i] / iter / num_tasks;
    }

    /* output */
    int line_size = 59;
    ABT_test_print_line(stdout, '-', line_size);
    printf("# of ESs            : %d\n", num_xstreams);
    printf("# of tasklets per ES: %d\n", num_tasks);
    ABT_test_print_line(stdout, '-', line_size);
    printf("Avg. execution time (in seconds, %d times)\n", iter);
    ABT_test_print_line(stdout, '-', line_size);
    printf("%-20s %-s\n", "operation", "time");
    ABT_test_print_line(stdout, '-', line_size);
    for (i = 0; i < T_LAST; i++) {
        printf("%-19s  %11" PRIu64 "\n", t_names[i], t_times[i]);
    }
    ABT_test_print_line(stdout, '-', line_size);

    free(g_xstreams);
    free(g_pools);
    for (i = 0; i < num_xstreams; i++) {
        free(g_tasks[i]);
    }
    free(g_tasks);
    free(all_pools);
    free(scheds);
    free(top_threads);

    return EXIT_SUCCESS;
}

