#include <stdio.h>
#include <stdlib.h>
#include <abt.h>

typedef void (*inner_f)(int);

void empty_f(int x) {
	printf("Hello %d\n", x);
}

typedef struct {
  ABT_thread thread;
  inner_f inner_func;
  int arg;
} abt_thread_data_t;

void inner_func_wrapper(void *arg) {
  abt_thread_data_t *d = (abt_thread_data_t *)arg;
  d->inner_func(d->arg);
}

int is_initialized = 0;

void abt_for(int num_threads, int loop_count, inner_f inner_func) {
  int i;
  ABT_pool pool;
  ABT_xstream *xstreams;
  abt_thread_data_t *threads;
  /* initialization */
  ABT_init(0, NULL);

  /* shared pool creation */
  ABT_pool_create_basic(ABT_POOL_FIFO, ABT_POOL_ACCESS_MPMC, ABT_TRUE, &pool);

  /* ES creation */
  xstreams = (ABT_xstream *)malloc(sizeof(ABT_xstream) * num_threads);
  int j = 0; 
  //if(!is_initialized) {
  ABT_xstream_self(&xstreams[0]);
  int error = ABT_xstream_set_main_sched_basic(xstreams[0], ABT_SCHED_DEFAULT, 1, &pool);
  if(error == -1) i = j = 0;
  else i = j = 1;
	//is_initialized = 1;
  //}
//else i = j = 0;
  while(i < num_threads) {  
    ABT_xstream_create_basic(ABT_SCHED_DEFAULT, 1, &pool, ABT_SCHED_CONFIG_NULL,
                             &xstreams[i]);
    ABT_xstream_start(xstreams[i]);
    i++;
  }

  /* ULT creation */
  threads = (abt_thread_data_t *)malloc(sizeof(abt_thread_data_t) * loop_count);
  for (i = 0; i < loop_count; i++) {
    threads[i].inner_func = inner_func;
    threads[i].arg = i;
    ABT_thread_create(pool, inner_func_wrapper, &threads[i],
                      ABT_THREAD_ATTR_NULL, &threads[i].thread);
  }

  /* join ULTs */
  for (i = 0; i < loop_count; i++) {
    ABT_thread_free(&threads[i].thread);
  }

  /* join ESs */
  //for (i = 1; i < num_threads; i++) {
  while(j < num_threads) {
    ABT_xstream_join(xstreams[j]);
    ABT_xstream_free(&xstreams[j]);
    j++;
  }

  free(threads);
  free(xstreams);

  ABT_finalize();
} 

void another_par(int i) {
  abt_for(2, 2, empty_f);
}

void inner_par(int i) {
  abt_for(2, 2, another_par);
}

int main () {
  abt_for(1, 1, inner_par);
  return 0;
}
