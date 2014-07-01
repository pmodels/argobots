/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

/**
 * This example shows the interleaving of threads and tasks to compute Fibonacci numbers.
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

/* structure to pass arguments to threads */
typedef struct task_args_t {
	int n;
	int result;
	ABT_mutex mutex;
	struct task_args_t *parent;
} task_args;

/* Function to compute Fibonacci numbers */
void fibonacci_thread(void *arguments){
	int n, result, *n1, *n2;
	thread_args a1, a2;
	ABT_thread t1, t2;
	ABT_xstream xstream;
	ABT_future future, f1, f2;

	thread_args *args = (thread_args *) arguments;
	n = args->n;
	future = args->future;

	printf("Thread computing Fibonacci of %d\n",n);
	/* checking for base cases */
	if(n <= 2)
		result = 1;
	else {
		ABT_xstream_self(&xstream);
		ABT_future_create(sizeof(int), &f1);
		a1.n = n-1;
		a1.future = f1;
    	ABT_thread_create(xstream, fibonacci_thread, &a1, ABT_THREAD_ATTR_NULL, &t1);
		ABT_future_create(sizeof(int), &f2);
		a2.n = n-2;
		a2.future = f2;
    	ABT_thread_create(xstream, fibonacci_thread, &a2, ABT_THREAD_ATTR_NULL, &t2);
		ABT_future_wait(f1, (void **) &n1);
		ABT_future_wait(f2, (void **) &n2);
		result = *n1 + *n2;
	}

	/* checking whether to signal the future */
	if(future != ABT_FUTURE_NULL){
		ABT_future_set(future, &result, sizeof(int));
	}
}

/* Function to compute Fibonacci numbers */
void fibonacci_task(void *arguments){
	int n, result;
	task_args *a1, *a2, *parent;
	ABT_task t1, t2;
	ABT_xstream xstream;

	task_args *args = (task_args *) arguments;
	n = args->n;
	parent = args->parent;

	printf("Task computing Fibonacci of %d\n",n);
	/* checking for base cases */
	if(n <= 2){
		args->result = 1;
		result = 1;
		int flag = 1;
		while(flag && parent != NULL){
			ABT_mutex_lock(parent->mutex);
			parent->result = parent->result + result;
			if(result == parent->result) flag = 0;
			ABT_mutex_unlock(parent->mutex);
			result = parent->result;
			parent = parent->parent;
		}
	} else {
		a1 = (task_args *) malloc (sizeof(task_args));
		a2 = (task_args *) malloc (sizeof(task_args));
		ABT_xstream_self(&xstream);
		a1->n = n-1;
		a1->result = 0;
		ABT_mutex_create(&a1->mutex);
		a1->parent = args;
    	ABT_task_create(xstream, fibonacci_task, a1, &t1);
		a2->n = n-2;
		a2->result = 0;
		ABT_mutex_create(&a2->mutex);
		a2->parent = args;
    	ABT_task_create(xstream, fibonacci_task, a2, &t2);
	}

}

/* Main function */
int main(int argc, char *argv[])
{
	int n;
	ABT_xstream xstream;
	ABT_thread thread;
	thread_args args_thread;
	ABT_task task;
	task_args args_task;

	/* init and thread creation */
    ABT_init(argc, argv);
	if(argc > 1)
		n = atoi(argv[1]);
	else
		n = N;

	/* creating thread */
	args_thread.n = n-1;
	args_thread.future = ABT_FUTURE_NULL;
	ABT_xstream_self(&xstream);
    ABT_thread_create(xstream, fibonacci_thread, &args_thread, ABT_THREAD_ATTR_NULL, &thread);

	/* creating task */
	args_task.n = n-2;
	args_task.result = 0;
	ABT_mutex_create(&args_task.mutex);
	args_task.parent = NULL;
	ABT_xstream_self(&xstream);
    ABT_task_create(xstream, fibonacci_task, &args_task, &task);

	/* switch to other user-level threads */
	ABT_thread_yield();
	
	/* join other threads */
	ABT_thread_join(thread);

    ABT_finalize();
    return 0;
}
