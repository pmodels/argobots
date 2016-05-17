/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include "abt.h"
#include "abttest.h"

#define DEFAULT_NUM_XSTREAMS    4
#define DEFAULT_NUM_TASKS       4
#define DEFAULT_NUM_THREADS     (DEFAULT_NUM_TASKS * 3)
#define DEFAULT_NUM_ITER        100

static int num_xstreams;
static int num_threads;
static int num_tasks;
static int num_iter;
static ABT_pool *pools;

#define OP_WAIT     0
#define OP_SET      1

typedef struct {
    ABT_eventual ev;
    int nbytes;
    int op_type;
} arg_t;

void thread_func(void *arg)
{
    int ret;
    arg_t *my_arg = (arg_t *)arg;

    if (my_arg->op_type == OP_WAIT) {
        if (my_arg->nbytes == 0) {
            ret = ABT_eventual_wait(my_arg->ev, NULL);
        } else {
            void *val;
            ret = ABT_eventual_wait(my_arg->ev, &val);
            assert(*(int *)val == 1);
        }
        ABT_TEST_ERROR(ret, "ABT_eventual_wait");

    } else if (my_arg->op_type == OP_SET) {
        if (my_arg->nbytes == 0) {
            ret = ABT_eventual_set(my_arg->ev, NULL, 0);
        } else {
            int val = 1;
            ret = ABT_eventual_set(my_arg->ev, &val, sizeof(int));
        }
        ABT_TEST_ERROR(ret, "ABT_eventual_set");
    }
}

void task_func(void *arg)
{
    int ret;
    arg_t *my_arg = (arg_t *)arg;
    assert(my_arg->op_type == OP_SET);

    if (my_arg->nbytes == 0) {
        ret = ABT_eventual_set(my_arg->ev, NULL, 0);
    } else {
        int val = 1;
        ret = ABT_eventual_set(my_arg->ev, &val, sizeof(int));
    }
    ABT_TEST_ERROR(ret, "ABT_eventual_set");
}

void eventual_test(void *arg)
{
    int i, t, ret, nbytes;
    size_t pid = (size_t)arg;
    ABT_thread *threads;
    ABT_task *tasks;
    ABT_eventual *evs;
    arg_t *thread_args;
    arg_t *task_args;

    threads = (ABT_thread *)malloc(num_threads * sizeof(ABT_thread));
    tasks = (ABT_task *)malloc(num_tasks * sizeof(ABT_task));
    evs = (ABT_eventual *)malloc(num_tasks * 2 * sizeof(ABT_eventual));
    task_args = (arg_t *)malloc(num_tasks * sizeof(arg_t));
    thread_args = (arg_t *)malloc(num_threads * sizeof(arg_t));

    for (t = 0; t < num_tasks * 2; t++) {
        nbytes = (t & 1) ? sizeof(int) : 0;
        ret = ABT_eventual_create(nbytes, &evs[t]);
        ABT_TEST_ERROR(ret, "ABT_eventual_create");
    }

    for (i = 0; i < num_iter; i++) {
        for (t = 0; t < num_threads - num_tasks; t += 2) {
            nbytes = ((t/2) & 1) ? sizeof(int) : 0;

            thread_args[t].ev = evs[t/2];
            thread_args[t].nbytes = nbytes;
            thread_args[t].op_type = OP_WAIT;
            ret = ABT_thread_create(pools[pid], thread_func, &thread_args[t],
                                    ABT_THREAD_ATTR_NULL, &threads[t]);
            ABT_TEST_ERROR(ret, "ABT_thread_create");
            pid = (pid + 1) % num_xstreams;

            thread_args[t+1].ev = evs[t/2];
            thread_args[t+1].nbytes = nbytes;
            thread_args[t+1].op_type = OP_SET;
            ret = ABT_thread_create(pools[pid], thread_func, &thread_args[t+1],
                                    ABT_THREAD_ATTR_NULL, &threads[t+1]);
            ABT_TEST_ERROR(ret, "ABT_thread_create");
            pid = (pid + 1) % num_xstreams;
        }

        for (t = num_threads - num_tasks; t < num_threads; t++) {
            nbytes = ((t - num_tasks) & 1) ? sizeof(int) : 0;
            thread_args[t].ev = evs[t - num_tasks];
            thread_args[t].nbytes = nbytes;
            thread_args[t].op_type = OP_WAIT;
            ret = ABT_thread_create(pools[pid], thread_func, &thread_args[t],
                                    ABT_THREAD_ATTR_NULL, &threads[t]);
            ABT_TEST_ERROR(ret, "ABT_thread_create");
            pid = (pid + 1) % num_xstreams;
        }

        for (t = 0; t < num_tasks; t++) {
            nbytes = ((num_tasks + t) & 1) ? sizeof(int) : 0;
            task_args[t].ev = evs[num_tasks + t];
            task_args[t].nbytes = nbytes;
            task_args[t].op_type = OP_SET;
            ret = ABT_task_create(pools[pid], task_func, &task_args[t], &tasks[t]);
            ABT_TEST_ERROR(ret, "ABT_task_create");
            pid = (pid + 1) % num_xstreams;
        }

        for (t = 0; t < num_threads; t++) {
            ret = ABT_thread_free(&threads[t]);
            ABT_TEST_ERROR(ret, "ABT_thread_free");
        }

        for (t = 0; t < num_tasks; t++) {
            ret = ABT_task_free(&tasks[t]);
            ABT_TEST_ERROR(ret, "ABT_task_free");
        }

        for (t = 0; t < num_tasks * 2; t++) {
            ret = ABT_eventual_reset(evs[t]);
            ABT_TEST_ERROR(ret, "ABT_eventual_reset");
        }
    }

    for (t = 0; t < num_tasks * 2; t++) {
        ret = ABT_eventual_free(&evs[t]);
        ABT_TEST_ERROR(ret, "ABT_eventual_free");
    }

    free(threads);
    free(tasks);
    free(evs);
    free(thread_args);
    free(task_args);
}

int main(int argc, char *argv[])
{
    ABT_xstream *xstreams;
    ABT_xstream *masters;
    int i, ret;

    /* Initialize */
    ABT_test_init(argc, argv);
    if (argc < 2) {
        num_xstreams = DEFAULT_NUM_XSTREAMS;
        num_threads  = DEFAULT_NUM_THREADS;
        num_tasks    = DEFAULT_NUM_TASKS;
        num_iter     = DEFAULT_NUM_ITER;
    } else {
        num_xstreams = ABT_test_get_arg_val(ABT_TEST_ARG_N_ES);
        num_tasks    = ABT_test_get_arg_val(ABT_TEST_ARG_N_TASK);
        num_threads  = num_tasks * 3;
        num_iter     = ABT_test_get_arg_val(ABT_TEST_ARG_N_ITER);
    }

    ABT_test_printf(1, "# of ESs        : %d\n", num_xstreams);
    ABT_test_printf(1, "# of ULTs/ES    : %d\n", num_threads);
    ABT_test_printf(1, "# of tasklets/ES: %d\n", num_tasks);
    ABT_test_printf(1, "# of iter       : %d\n", num_iter);

    xstreams = (ABT_xstream *)malloc(num_xstreams * sizeof(ABT_xstream));
    pools    = (ABT_pool *)malloc(num_xstreams * sizeof(ABT_pool));
    masters  = (ABT_thread *)malloc(num_xstreams * sizeof(ABT_thread));

    /* Create Execution Streams */
    ret = ABT_xstream_self(&xstreams[0]);
    ABT_TEST_ERROR(ret, "ABT_xstream_self");
    for (i = 1; i < num_xstreams; i++) {
        ret = ABT_xstream_create(ABT_SCHED_NULL, &xstreams[i]);
        ABT_TEST_ERROR(ret, "ABT_xstream_create");
    }

    /* Get the main pool of each ES */
    for (i = 0; i < num_xstreams; i++) {
        ret = ABT_xstream_get_main_pools(xstreams[i], 1, &pools[i]);
        ABT_TEST_ERROR(ret, "ABT_xstream_get_main_pools");
    }

    /* Create a master ULT for each ES */
    for (i = 1; i < num_xstreams; i++) {
        ret = ABT_thread_create(pools[i], eventual_test, (void *)(size_t)i,
                                ABT_THREAD_ATTR_NULL, &masters[i]);
        ABT_TEST_ERROR(ret, "ABT_thread_create");
    }

    eventual_test((void *)0);

    /* Join master ULTs */
    for (i = 1; i < num_xstreams; i++) {
        ret = ABT_thread_free(&masters[i]);
        ABT_TEST_ERROR(ret, "ABT_thread_free");
    }

    /* Join and free Execution Streams */
    for (i = 1; i < num_xstreams; i++) {
        ret = ABT_xstream_join(xstreams[i]);
        ABT_TEST_ERROR(ret, "ABT_xstream_join");
        ret = ABT_xstream_free(&xstreams[i]);
        ABT_TEST_ERROR(ret, "ABT_xstream_free");
    }

    /* Finalize */
    ret = ABT_test_finalize(0);

    free(xstreams);
    free(pools);
    free(masters);

    return ret;
}

