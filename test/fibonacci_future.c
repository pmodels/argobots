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
	ABT_future future;
} thread_args;

/* Function to compute Fibonacci numbers */
void fibonacci(void *arguments){
	int n, result, *n1, *n2;
	thread_args a1, a2;
	ABT_thread t1, t2;
	ABT_xstream xstream;
	ABT_future future, f1, f2;

	thread_args *args = (thread_args *) arguments;
	n = args->n;
	future = args->future;

//printf("Computing Fibonacci of %d\n",n);
	/* checking for base cases */
	if(n <= 2)
		result = 1;
	else {
		ABT_xstream_self(&xstream);
		ABT_future_create(sizeof(int), &f1);
		a1.n = n-1;
		a1.future = f1;
    	ABT_thread_create(xstream, fibonacci, &a1, 16384, &t1);
		ABT_future_create(sizeof(int), &f2);
		a2.n = n-2;
		a2.future = f2;
    	ABT_thread_create(xstream, fibonacci, &a2, 16384, &t2);
		ABT_future_wait(f1, (void **) &n1);
		ABT_future_wait(f2, (void **) &n2);
		result = *n1 + *n2;
	}

	/* checking whether to signal the future */
	if(future != ABT_FUTURE_NULL){
		ABT_future_set(future, &result, sizeof(int));
	} else {
		printf("The %d-th number in the Fibonacci sequence is: %d\n",n,result);
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
    ABT_thread_create(xstream, fibonacci, &args, 16384, &thread);

	/* switch to other user-level threads */
	ABT_thread_yield();
	
	/* join other threads */
	ABT_thread_join(thread);

    ABT_finalize();
    return 0;
}
