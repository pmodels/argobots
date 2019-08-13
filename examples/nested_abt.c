#include <stdio.h>
#include <stdlib.h>
#include <abt.h>
#include <unistd.h>

#define SIZE 1024

static int main_num_es = 4;
static int inner_num_es = 4;
int matA[SIZE][SIZE]; 
int matB[SIZE][SIZE]; 
int matC[SIZE][SIZE];

typedef struct {
	int start_i;
	int end_i;
} threadData;

typedef void (*inner_f)(void*);

void empty_f(void* args) {
	printf("Hello World\n");
/*	threadData* data = (threadData*)args;
  
    for (int i = data->start_i; i < data->end_i; i++)  
         for (int j = 0; j < SIZE; j++)  
                for (int k = 0; k < SIZE; k++)  
    	          matC[i][j] += matA[i][k] * matB[k][j];
*/
}


typedef struct {
  ABT_thread thread;
  inner_f inner_func;
  threadData* arg;
} abt_thread_data_t;

void inner_func_wrapper(void *arg) {
  abt_thread_data_t *d = (abt_thread_data_t *)arg;
  d->inner_func(d->arg);
}

void abt_for(int num_threads, int loop_count, inner_f inner_func) {
  int i;
  ABT_pool pool;
  ABT_xstream *xstreams = NULL;
  abt_thread_data_t *threads = NULL;
  int set_main_sched_err;

  int initialized = ABT_initialized() != ABT_ERR_UNINITIALIZED;
  /* initialization */
  ABT_init(0, NULL);

  /* shared pool creation */
  ABT_pool_create_basic(ABT_POOL_FIFO, ABT_POOL_ACCESS_MPMC, ABT_TRUE, &pool);

  /* ES creation */
  xstreams = (ABT_xstream *)malloc(sizeof(ABT_xstream) * num_threads);
  ABT_xstream_self(&xstreams[0]);
  
  set_main_sched_err = ABT_xstream_set_main_sched_basic(xstreams[0],
                                                          ABT_SCHED_DEFAULT, 1,
                                                          &pool);
  int start_i = (set_main_sched_err != ABT_SUCCESS) ? 0 : 1;
  for (i = start_i; i < num_threads; i++) {
    ABT_xstream_create_basic(ABT_SCHED_DEFAULT, 1, &pool, ABT_SCHED_CONFIG_NULL,
                             &xstreams[i]);
    ABT_xstream_start(xstreams[i]);
  }

  /* ULT creation */
  threads = (abt_thread_data_t *)malloc(sizeof(abt_thread_data_t) * loop_count);
  int each = SIZE/loop_count;
  for (i = 0; i < loop_count; i++) {
    threads[i].inner_func = inner_func;
    threads[i].arg = (threadData*)malloc(sizeof(threadData));
    threads[i].arg->start_i = i * each;
    threads[i].arg->end_i = threads[i].arg->start_i + each - 1;
    //printf("start_i %d, end_i %d\n", threads[i].arg->start_i, threads[i].arg->end_i);
    ABT_thread_create(pool, inner_func_wrapper, &threads[i],
                      ABT_THREAD_ATTR_NULL, &threads[i].thread);
  }

  /* join ULTs */
  for (i = 0; i < loop_count; i++) {
    ABT_thread_free(&threads[i].thread);
  }

  /* join ESs */
#if 0
  // if the following starts from 1, ABT_finalize will cause an error since the
  // main ULT might be scheduled by a secondary execuntion stream.
  for (i = 1; i < num_threads; i++)
#else
    for (i = start_i; i < num_threads; i++)
#endif
      {
	ABT_xstream_join(xstreams[i]);
	ABT_xstream_free(&xstreams[i]);
      }

  free(threads);
  free(xstreams);
  ABT_finalize();
}

void inner2_par(int i) {
  abt_for(inner_num_es, inner_num_es, empty_f);
}

void inner_par(void* data) {
  abt_for(inner_num_es, inner_num_es, empty_f);
}

int main (int argc, char** argv) {
  if(argc == 3) {
	main_num_es = atoi(argv[1]);
	inner_num_es = atoi(argv[2]);
  } 
  abt_for(main_num_es, main_num_es, empty_f);
  //abt_for(main_num_es, main_num_es, empty_f);
  return 0;
}
