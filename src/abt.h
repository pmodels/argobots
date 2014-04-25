#ifndef ABT_H_INCLUDED
#define ABT_H_INCLUDED

#include <pthread.h>
#include <ucontext.h>

/* Constants */
#define ABT_SUCCESS      0
#define ABT_FAILURE      1

typedef enum ABT_thread_state {
    ABT_STATE_READY,
    ABT_STATE_RUNNING,
    ABT_STATE_BLOCKED,
    ABT_STATE_TERMINATED
} ABT_thread_state_t;

/* Definitions */
typedef struct ABT_stream ABT_stream_t;
typedef struct ABT_thread ABT_thread_t;
typedef size_t ABT_stream_id_t;
typedef size_t ABT_thread_id_t;


/* Data Structures */
struct ABT_stream {
    ABT_stream_id_t id;
    pthread_t es;
    ucontext_t ctx;
    size_t num_threads;
    ABT_thread_t *first_thread;
    ABT_thread_t *last_thread;
};

struct ABT_thread {
    ABT_stream_t *stream;

    ABT_thread_id_t id;
    ABT_thread_state_t state;
    void *stack;
    size_t stacksize;
    ucontext_t ctx;
    ABT_thread_t *prev;
    ABT_thread_t *next;
};


/* Execution Stream (ES) */
ABT_stream_t *ABT_stream_create(int *err);
int ABT_stream_start(ABT_stream_t *stream);
int ABT_stream_join(ABT_stream_t *stream);
int ABT_stream_free(ABT_stream_t *stream);
int ABT_stream_cancel(ABT_stream_t *stream);
int ABT_stream_exit();


/* User Level Thread (ULT) */
ABT_thread_t *ABT_thread_create(ABT_stream_t *stream,
                              void (*thread_func)(void *), void *arg,
                              size_t stacksize,
                              int *err);
int ABT_thread_yield();
int ABT_thread_yield_to(ABT_thread_t *thread);

#endif /* ABT_H_INCLUDED */
