/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

/**
 * This example shows the use of tasks to compute Fibonacci numbers. The execution proceeds in two phases.
 * 1) Expand phase. A binary tree of activation records is built in a top-down fashion. Each activation
 * record corresponds to a Fibonacci number Fib(n) computed recursively. A task computing Fib(n) will
 * create two subtasks to compute Fib(n-1) and Fib(n-2), respectively.
 * 2) Aggregrate phase. The final results is computed bottom-up. Once a base case is reached (n<=2), a
 * task is created to aggregate the result into the parent's activation record. Therefore, two tasks will
 * update the activation record for Fib(n). The last of the two aggregate tasks will spawn another task
 * that will aggregate the result up the binary tree.
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <abt.h>

#define N 10
#define DEFAULT_NUM_XSTREAMS    4

#define HANDLE_ERROR(ret,msg)                           \
    if (ret != ABT_SUCCESS) {                           \
        fprintf(stderr, "ERROR[%d]: %s\n", ret, msg);   \
        exit(EXIT_FAILURE);                             \
    }

/* global variables */
int num_xstreams;
ABT_xstream *xstreams;
ABT_eventual eventual;

/* forward declaration */
void aggregate_fibonacci(void *arguments);

/* structure to pass arguments to expand tasks */
typedef struct exp_task_args_t {
    int n;
    int result;
    char flag;
    ABT_mutex mutex;
    struct exp_task_args_t *parent;
} exp_task_args;

/* structure to pass arguments to aggregate tasks */
typedef struct agg_task_args_t {
    int result;
    exp_task_args *parent;
} agg_task_args;

/* Function to compute Fibonacci numbers during expand phase */
void expand_fibonacci(void *arguments){
    int n;
    exp_task_args *exp_args1, *exp_args2, *parent;
    agg_task_args *agg_args;

    exp_task_args *args = (exp_task_args *) arguments;
    n = args->n;
    parent = args->parent;

    /* checking for base cases */
    if(n <= 2){

        /* creating an aggregate task */
        args->result = 1;
        agg_args = (agg_task_args *) malloc (sizeof(agg_task_args));
        agg_args->result = 1;
        agg_args->parent = parent;
        ABT_task_create(ABT_XSTREAM_NULL, aggregate_fibonacci, agg_args, NULL);

    } else {

        /* creating task to compute Fib(n-1) */
        exp_args1 = (exp_task_args *) malloc (sizeof(exp_task_args));
        exp_args1->n = n-1;
        exp_args1->result = 0;
        exp_args1->flag = 0;
        ABT_mutex_create(&exp_args1->mutex);
        exp_args1->parent = args;
        ABT_task_create(xstreams[n%num_xstreams], expand_fibonacci, exp_args1, NULL);

        /* creating task to compute Fib(n-2) */
        exp_args2 = (exp_task_args *) malloc (sizeof(exp_task_args));
        exp_args2->n = n-2;
        exp_args2->result = 0;
        exp_args2->flag = 0;
        ABT_mutex_create(&exp_args2->mutex);
        exp_args2->parent = args;
        ABT_task_create(ABT_XSTREAM_NULL, expand_fibonacci, exp_args2, NULL);
    }
}

/* Function to compute Fibonacci numbers during aggregate phase */
void aggregate_fibonacci(void *arguments){
    exp_task_args *parent;
    agg_task_args *args, *agg_args;
    int data;

    args = (agg_task_args *) arguments;
    parent = args->parent;

    /* checking whether this is the root of the tree */
    if(parent != NULL){
        ABT_mutex_lock(parent->mutex);
        parent->result += args->result;
        if(parent->flag){
            /* creating an aggregate task */
            agg_args = (agg_task_args *) malloc (sizeof(agg_task_args));
            agg_args->result = parent->result;
            agg_args->parent = parent->parent;
            ABT_task_create(ABT_XSTREAM_NULL, aggregate_fibonacci, agg_args, NULL);
        } else {
            parent->flag = 1;
        }
        ABT_mutex_unlock(parent->mutex);
    } else {
        ABT_eventual_set(eventual, &data, sizeof(int));
    }

}

/* Control thread function */
void fibonacci_control(void *arguments){
    exp_task_args args;
    size_t n;
    int *data, ret;

    n = (size_t) arguments;

    /* creating eventual */
    ABT_eventual_create(sizeof(int), &eventual);

    /* creating parent task to compute Fib(n) */
    args.n = n;
    args.result = 0;
    args.flag = 0;
    ABT_mutex_create(&args.mutex);
    args.parent = NULL;
    ret = ABT_task_create(ABT_XSTREAM_NULL, expand_fibonacci, &args, NULL);
    HANDLE_ERROR(ret, "ABT_task_create");


    /* switch to other user-level threads */
    ABT_thread_yield();

    /* block until the eventual is signaled */
    ABT_eventual_wait(eventual, (void **)&data);

    printf("The %lu-th value in the Fibonacci sequence is: %d\n",n,args.result);
}

/* Main function */
int main(int argc, char *argv[]){
    size_t n, i;
    int ret;
    num_xstreams = DEFAULT_NUM_XSTREAMS;
    ABT_thread thread;

    /* initialization */
    ABT_init(argc, argv);
    if(argc > 1){
        n = atoi(argv[1]);
        assert(n>0);
    } else {
        n = N;
    }
    if(argc > 2) num_xstreams = atoi(argv[2]);
    srand48(time(NULL));

    /* stream creation */
    xstreams = (ABT_xstream *)malloc(sizeof(ABT_xstream) * num_xstreams);
    ret = ABT_xstream_self(&xstreams[0]);
    HANDLE_ERROR(ret, "ABT_xstream_self");
    for(i=1; i < num_xstreams; i++) {
        ret = ABT_xstream_create(ABT_SCHED_NULL, &xstreams[i]);
        HANDLE_ERROR(ret, "ABT_xstream_create");
    }

    /* control thread creation */
    ABT_thread_create(xstreams[0], fibonacci_control, (void *)n, ABT_THREAD_ATTR_NULL, &thread);
    ABT_thread_yield();

    /* joining control thread */
    ret = ABT_thread_join(thread);
       HANDLE_ERROR(ret, "ABT_thread_join");

    /* join streams */
    for(i=1; i < num_xstreams; i++) {
        ret = ABT_xstream_join(xstreams[i]);
        HANDLE_ERROR(ret, "ABT_xstream_join");
    }

    /* deallocating streams */
    for(i=1; i < num_xstreams; i++) {
        ret = ABT_xstream_free(&xstreams[i]);
        HANDLE_ERROR(ret, "ABT_xstream_free");
    }

    ABT_finalize();
    return 0;
}
