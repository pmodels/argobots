/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

/**
 * This example shows the use of futures to compute Fibonacci numbers.
 */

#include <stdio.h>
#include <stdlib.h>
#include <abt.h>

#define N 5

/* structure to pass arguments to threads */
typedef struct {
    int n;
	int result;
    ABT_future future;
} thread_args;

/* Callback function passed to future */
void callback(void **args){
	int n1,n2;

	n1 = ((int *)args[0])[0];
	n2 = ((int *)args[1])[0];
	((int *)args[2])[0] = n1+n2;
}

/* Function to compute Fibonacci numbers */
void fibonacci(void *arguments){
    int n;
    thread_args a1, a2;
    ABT_thread t1, t2;
    ABT_xstream xstream;
    ABT_future future, fut;

    thread_args *args = (thread_args *) arguments;
    n = args->n;
    future = args->future;

    /* checking for base cases */
    if(n <= 2)
        args->result = 1;
    else {
        ABT_xstream_self(&xstream);
        ABT_future_create(3,&callback,&fut);
		ABT_future_set(fut, (void *)&args->result);
        a1.n = n-1;
        a1.future = fut;
        ABT_thread_create(xstream, fibonacci, &a1, ABT_THREAD_ATTR_NULL, &t1);
        a2.n = n-2;
        a2.future = fut;
        ABT_thread_create(xstream, fibonacci, &a2, ABT_THREAD_ATTR_NULL, &t2);
        ABT_future_wait(fut);
		ABT_future_free(&fut);
    }

    /* checking whether to signal the future */
    if(future != ABT_FUTURE_NULL){
        ABT_future_set(future, (void *)&args->result);
    } 
}

/* Main function */
int main(int argc, char *argv[])
{
    int n;
    ABT_xstream xstream;
    ABT_thread thread;
    thread_args args;

    /* init and thread creation */
    ABT_init(argc, argv);
    if(argc > 1)
        n = atoi(argv[1]);
    else
        n = N;
    args.n = n;
    args.future = ABT_FUTURE_NULL;
    ABT_xstream_self(&xstream);
    ABT_thread_create(xstream, fibonacci, &args, ABT_THREAD_ATTR_NULL, &thread);

    /* switch to other user-level threads */
    ABT_thread_yield();

    /* join other threads */
    ABT_thread_join(thread);

	printf("The %d-th number in the Fibonacci sequence is: %d\n",n,args.result);

    ABT_finalize();
    return 0;
}
