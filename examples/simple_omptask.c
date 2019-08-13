#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <string.h>
#include <omp.h>
#include <abt.h>

ABT_pool g_pool = ABT_POOL_NULL;
#define NUM_ABT_THREADS 2
#define NUM_OMP_THREADS 2

void print_hello(void* args) {
	#pragma omp parallel num_threads(NUM_OMP_THREADS)
	for(int i=0;i<1;i++)
		printf("Hello World from tid %d!\n", *((int*)args));
} 

void simple_thread(void* args) {
	ABT_thread *threads = malloc(sizeof(ABT_thread) * 5);	
	for(int i = 0; i < NUM_ABT_THREADS; i++) {
		int *tid = malloc(sizeof(int));
		*tid = i;
		ABT_thread_create(g_pool, print_hello, tid, ABT_THREAD_ATTR_NULL, &threads[i]);
	}
	for(int i = 0; i < NUM_ABT_THREADS; i++) {
            ABT_thread_free(&threads[i]);
        }
}

int main(int argc, char* argv[]) {

#ifdef USE_BOLT_ONLY
	#pragma omp parallel num_threads(NUM_OMP_THREADS)
	{
	#pragma omp single
	{
		#pragma omp task
		#pragma omp parallel num_threads(NUM_OMP_THREADS)
		for(int i = 0; i < 1; i++)
			printf("Hello World from tid %d\n", omp_get_thread_num()); 
	}
	}
#else
    ABT_thread thread;
    int num_xstreams = 1;
    ABT_xstream *xstreams;
    
    ABT_init(argc, argv);

    /* create a scheduler pool? */
    ABT_pool_create_basic(ABT_POOL_FIFO, ABT_POOL_ACCESS_MPMC, ABT_TRUE, &g_pool);
    
    /* Create execution streams */
    xstreams = (ABT_xstream *)malloc(sizeof(ABT_xstream) * num_xstreams);
    
    ABT_xstream_self(&xstreams[0]);
    ABT_xstream_set_main_sched_basic(xstreams[0], ABT_SCHED_DEFAULT, 1, &g_pool);
    
    int i;
    for(i = 1; i < num_xstreams; i++) {
    	ABT_xstream_create_basic(ABT_SCHED_DEFAULT, 1, &g_pool, ABT_SCHED_CONFIG_NULL, &xstreams[i]);
        ABT_xstream_start(xstreams[i]);
    }

    ABT_thread_create(g_pool, simple_thread, NULL, ABT_THREAD_ATTR_NULL, &thread);
    //ABT_thread_join(thread);
    ABT_thread_free(&thread);

    for (i = 1; i < num_xstreams; i++) {
    	ABT_xstream_join(xstreams[i]);
    	ABT_xstream_free(&xstreams[i]);
    }

    free(xstreams);

    ABT_finalize();
#endif
	return 0;
}
