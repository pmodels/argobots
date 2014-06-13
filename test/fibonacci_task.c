/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

/**
 * This example shows the use of tasks to compute Fibonacci numbers.
 */

#include <stdio.h>
#include <stdlib.h>
#include <abt.h>

#define N 2

/* structure to pass arguments to threads */
typedef struct task_args_t {
	int n;
	int result;
	ABT_mutex mutex;
	struct task_args_t *parent;
} task_args;

/* Function to compute Fibonacci numbers */
void fibonacci(void *arguments){
	int n, result;
	task_args a1, a2, *parent;
	ABT_task t1, t2;
	ABT_xstream xstream;

	task_args *args = (task_args *) arguments;
	n = args->n;
	parent = args->parent;

printf("Computing Fibonacci of %d, result %d\n",n,args->result);
sleep(1);
	/* checking for base cases */
	if(n <= 2){
		args->result = 1;
		result = 1;
		int flag = 1;
		while(flag && parent != NULL){
			ABT_mutex_lock(parent->mutex);
			parent->result =+ result;
			if(result == parent->result) flag = 0;
			ABT_mutex_unlock(parent->mutex);
			result = parent->result;
			parent = parent->parent;
		}
	} else {
		ABT_xstream_self(&xstream);
		a1.n = n-1;
		a1.result = 0;
		ABT_mutex_create(&a1.mutex);
		a1.parent = args;
    	ABT_task_create(xstream, fibonacci, &a1, &t1);
		a2.n = n-2;
		a2.result = 0;
		ABT_mutex_create(&a2.mutex);
		a2.parent = args;
    	ABT_task_create(xstream, fibonacci, &a2, &t2);
	}

}

/* Main function */
int main(int argc, char *argv[])
{
	int n;
	ABT_xstream xstream;
	ABT_task task;
	task_args args;

	/* init and thread creation */
    ABT_init(argc, argv);
	if(argc > 1)
		n = atoi(argv[1]);
	else
		n = N;
	args.n = n;
	args.result = 0;
	ABT_mutex_create(&args.mutex);
	args.parent = NULL;
	ABT_xstream_self(&xstream);
    ABT_task_create(xstream, fibonacci, &args, &task);

	/* switch to other user-level threads */
	ABT_thread_yield();

	printf("The %d-th values in the Fibonacci sequence is: %d\n",n,args.result);
	
    ABT_finalize();
    return 0;
}
