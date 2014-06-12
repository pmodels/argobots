/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#ifndef ABTI_H_INCLUDED
#define ABTI_H_INCLUDED

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>

#include "abt.h"
#include "abtu.h"

/* Data Types */
typedef struct ABTI_stream       ABTI_stream;
typedef enum ABTI_stream_type    ABTI_stream_type;
typedef struct ABTI_thread       ABTI_thread;
typedef enum ABTI_thread_type    ABTI_thread_type;
typedef struct ABTI_task         ABTI_task;
typedef struct ABTI_mutex        ABTI_mutex;
typedef struct ABTI_condition    ABTI_condition;
typedef struct ABTI_scheduler    ABTI_scheduler;
typedef enum ABTI_scheduler_type ABTI_scheduler_type;
typedef struct ABTI_unit         ABTI_unit;
typedef struct ABTI_pool         ABTI_pool;
typedef struct ABTI_stream_pool  ABTI_stream_pool;
typedef struct ABTI_task_pool    ABTI_task_pool;
typedef struct ABTI_global       ABTI_global;
typedef struct ABTI_local        ABTI_local;
typedef struct ABTI_future       ABTI_future;

/* Architecture-Dependent Definitions */
#include "abtd.h"


/* Constants and Enums */
#define ABTI_THREAD_DEFAULT_STACKSIZE   16384

#define ABTI_STREAM_REQ_JOIN        0x1
#define ABTI_STREAM_REQ_EXIT        0x2
#define ABTI_STREAM_REQ_CANCEL      0x4

#define ABTI_THREAD_REQ_JOIN        0x1
#define ABTI_THREAD_REQ_EXIT        0x2
#define ABTI_THREAD_REQ_CANCEL      0x4

#define ABTI_TASK_REQ_CANCEL        0x1

enum ABTI_stream_type {
    ABTI_STREAM_TYPE_PRIMARY,
    ABTI_STREAM_TYPE_SECONDARY
};

enum ABTI_thread_type {
    ABTI_THREAD_TYPE_MAIN,
    ABTI_THREAD_TYPE_USER
};

enum ABTI_scheduler_type {
    ABTI_SCHEDULER_TYPE_DEFAULT,
    ABTI_SCHEDULER_TYPE_USER
};


/* Definitions */
struct ABTI_stream {
    ABT_unit unit;             /* Unit enclosing this stream */
    uint64_t id;               /* ID */
    char *p_name;              /* Name */
    ABTI_stream_type type;     /* Type */
    ABT_stream_state state;    /* State */

    uint32_t request;          /* Request */
    ABT_mutex mutex;           /* Mutex */
    ABTI_scheduler *p_sched;   /* Scheduler */
    ABT_pool deads;            /* Units terminated but still referenced */

    ABTD_stream_context ctx;   /* Stream context */
};

struct ABTI_thread {
    ABT_unit unit;              /* Unit enclosing this thread */
    ABTI_stream *p_stream;      /* Associated stream */
    uint64_t id;                /* ID */
    char *p_name;               /* Name */
    ABTI_thread_type type;      /* Type */
    ABT_thread_state state;     /* State */
    size_t stacksize;           /* Stack size */
    uint32_t refcount;          /* Reference count */

    void (*f_callback)(void *); /* Callback function */
    void *p_cb_arg;             /* Argument for callback function */

    uint32_t request;           /* Request */
    void *p_req_arg;            /* Request argument */
    ABT_mutex mutex;            /* Mutex */
    void *p_stack;              /* Stack */

    ABTD_thread_context ctx;    /* Context */
};

struct ABTI_task {
    ABT_unit unit;             /* Unit enclosing this task */
    ABTI_stream *p_stream;     /* Associated stream */
    uint64_t id;               /* ID */
    char *p_name;              /* Name */
    ABT_task_state state;      /* State */
    uint32_t refcount;         /* Reference count */

    uint32_t request;          /* Request */
    ABT_mutex mutex;           /* Mutex */
    void (*f_task)(void *);    /* Task function */
    void *p_arg;               /* Task arguments */
};

struct ABTI_mutex {
    uint32_t val;
};

struct ABTI_condition {
};

struct ABTI_scheduler {
    ABTI_scheduler_type type; /* Type */
    ABT_mutex mutex;          /* Mutex */
    ABTD_thread_context ctx;  /* Scheduler context */
    ABT_pool pool;            /* Work unit pool */

    /* Scheduler functions */
    ABT_unit_get_type_fn           u_get_type;
    ABT_unit_get_thread_fn         u_get_thread;
    ABT_unit_get_task_fn           u_get_task;
    ABT_unit_create_from_thread_fn u_create_from_thread;
    ABT_unit_create_from_task_fn   u_create_from_task;
    ABT_unit_free_fn               u_free;
    ABT_pool_get_size_fn           p_get_size;
    ABT_pool_push_fn               p_push;
    ABT_pool_pop_fn                p_pop;
    ABT_pool_remove_fn             p_remove;
};

struct ABTI_unit {
    ABTI_pool    *p_pool; /* Pool to which this unit belongs */
    ABT_unit_type type;   /* Work unit type */
    void         *p_unit; /* Work unit object,
                             e.g., ABTI_thread or ABTI_task */
    ABTI_unit    *p_prev; /* Previous unit in list */
    ABTI_unit    *p_next; /* Next unit in list */
};

struct ABTI_pool {
    size_t     num_units; /* Number of units */
    ABTI_unit *p_head;    /* The first unit */
    ABTI_unit *p_tail;    /* The last unit */
};

struct ABTI_stream_pool {
    ABT_pool created;  /* Streams in CREATED state */
    ABT_pool active;   /* Streams in READY or RUNNING state */
    ABT_pool deads;    /* Streams in TERMINATED state but not freed */
    ABT_mutex mutex;   /* Mutex */
};

struct ABTI_task_pool {
    ABT_pool pool;     /* Tasks created but not assigned */
    ABT_mutex mutex;   /* Mutex */
};

struct ABTI_global {
    ABTI_stream_pool *p_streams;  /* Stream pool */
    ABTI_task_pool   *p_tasks;    /* Task pool */
};

struct ABTI_local {
    ABTI_stream *p_stream;  /* Current stream */
    ABTI_thread *p_thread;  /* Current running thread */
};

/* Futures */
typedef struct ABTI_thread_entry_t {
    ABT_thread current;
    struct ABTI_thread_entry_t *next;
} ABTI_thread_entry;

typedef struct {
    ABTI_thread_entry *head;
    ABTI_thread_entry *tail;
} ABTI_threads_list;

struct ABTI_future{
	ABT_stream stream;
    int ready;
    void *value;
    int nbytes;
    ABTI_threads_list waiters;
};

/* Global Data */
extern ABTI_global *gp_ABTI_global;


/* ES Local Data */
extern ABTD_STREAM_LOCAL ABTI_local *lp_ABTI_local;


/* Init & Finalize */
int ABTI_stream_pool_init(ABTI_stream_pool *p_streams);
int ABTI_stream_pool_finalize(ABTI_stream_pool *p_streams);
int ABTI_task_pool_init(ABTI_task_pool *p_tasks);
int ABTI_task_pool_finalize(ABTI_task_pool *p_tasks);

/* Global Data */
int ABTI_global_add_stream(ABTI_stream *p_stream);
int ABTI_global_move_stream(ABTI_stream *p_stream);
int ABTI_global_del_stream(ABTI_stream *p_stream);
int ABTI_global_get_created_stream(ABTI_stream **p_stream);
int ABTI_global_add_task(ABTI_task *p_task);
int ABTI_global_del_task(ABTI_task *p_task);
int ABTI_global_pop_task(ABTI_task **p_task);
int ABTI_global_has_task(int *result);

/* ES Local Data */
int ABTI_local_init(ABTI_stream *p_stream);
int ABTI_local_finalize();
static inline ABTI_stream *ABTI_local_get_stream() {
    return lp_ABTI_local->p_stream;
}
static inline void ABTI_local_set_stream(ABTI_stream *p_stream) {
    lp_ABTI_local->p_stream = p_stream;
}
static inline ABTI_thread *ABTI_local_get_thread() {
    return lp_ABTI_local->p_thread;
}
static inline void ABTI_local_set_thread(ABTI_thread *p_thread) {
    lp_ABTI_local->p_thread = p_thread;
}

/* Execution Stream (ES) */
ABTI_stream *ABTI_stream_get_ptr(ABT_stream stream);
ABT_stream   ABTI_stream_get_handle(ABTI_stream *p_stream);
int ABTI_stream_free(ABTI_stream *p_stream);
int ABTI_stream_start(ABTI_stream *p_stream);
int ABTI_stream_start_any();
int ABTI_stream_schedule(ABTI_stream *p_stream);
int ABTI_stream_schedule_thread(ABTI_thread *p_thread);
int ABTI_stream_schedule_task(ABTI_task *p_task);
int ABTI_stream_terminate_thread(ABTI_thread *p_thread);
int ABTI_stream_terminate_task(ABTI_task *p_task);
int ABTI_stream_add_thread(ABTI_thread *p_thread);
int ABTI_stream_keep_thread(ABTI_thread *p_thread);
int ABTI_stream_keep_task(ABTI_task *p_task);
int ABTI_stream_print(ABTI_stream *p_stream);

/* User-level Thread (ULT) */
ABTI_thread *ABTI_thread_get_ptr(ABT_thread thread);
ABT_thread   ABTI_thread_get_handle(ABTI_thread *p_thread);
int ABTI_thread_create_main(ABTI_stream *p_stream, ABTI_thread **p_thread);
int ABTI_thread_free_main(ABTI_thread *p_thread);
int ABTI_thread_free(ABTI_thread *p_thread);
int ABTI_thread_suspend();
int ABTI_thread_set_ready(ABT_thread thread);
int ABTI_thread_print(ABTI_thread *p_thread);
void ABTI_thread_func_wrapper(void (*thread_func)(void *), void *p_arg);
ABT_thread *ABTI_thread_current();

/* Tasklet */
ABTI_task *ABTI_task_get_ptr(ABT_task task);
ABT_task   ABTI_task_get_handle(ABTI_task *p_task);
int ABTI_task_free(ABTI_task *p_task);
int ABTI_task_print(ABTI_task *p_task);

/* Mutex */
ABTI_mutex *ABTI_mutex_get_ptr(ABT_mutex mutex);
ABT_mutex   ABTI_mutex_get_handle(ABTI_mutex *p_mutex);
int ABTI_mutex_waitlock(ABT_mutex mutex);

/* Future */
ABTI_future *ABTI_future_get_ptr(ABT_future future);

/* Condition */

/* Scheduler */
ABTI_scheduler *ABTI_scheduler_get_ptr(ABT_scheduler sched);
ABT_scheduler   ABTI_scheduler_get_handle(ABTI_scheduler *p_sched);
int  ABTI_scheduler_create_default(ABTI_scheduler **p_newsched);
void ABTI_scheduler_push(ABTI_scheduler *p_sched, ABT_unit unit);
ABT_unit ABTI_scheduler_pop(ABTI_scheduler *p_sched);
void ABTI_scheduler_remove(ABTI_scheduler *p_sched, ABT_unit unit);
int  ABTI_scheduler_print(ABTI_scheduler *p_sched);

/* Unit */
ABTI_unit *ABTI_unit_get_ptr(ABT_unit unit);
ABT_unit   ABTI_unit_get_handle(ABTI_unit *p_unit);
ABT_unit_type ABTI_unit_get_type(ABT_unit unit);
ABT_stream    ABTI_unit_get_stream(ABT_unit unit);
ABT_thread    ABTI_unit_get_thread(ABT_unit unit);
ABT_task      ABTI_unit_get_task(ABT_unit unit);
ABT_unit      ABTI_unit_create_from_stream(ABT_stream stream);
ABT_unit      ABTI_unit_create_from_thread(ABT_thread thread);
ABT_unit      ABTI_unit_create_from_task(ABT_task task);
void          ABTI_unit_free(ABT_unit *unit);
int           ABTI_unit_print(ABT_unit unit);

/* Pool */
ABTI_pool *ABTI_pool_get_ptr(ABT_pool pool);
ABT_pool   ABTI_pool_get_handle(ABTI_pool *p_pool);
int ABTI_pool_create(ABT_pool *newpool);
int ABTI_pool_free(ABT_pool *pool);
size_t   ABTI_pool_get_size(ABT_pool pool);
void     ABTI_pool_push(ABT_pool pool, ABT_unit unit);
ABT_unit ABTI_pool_pop(ABT_pool pool);
void     ABTI_pool_remove(ABT_pool pool, ABT_unit unit);
int      ABTI_pool_print(ABT_pool pool);


#define ABTI_CHECK_ERROR(abt_errno)  \
    if (abt_errno != ABT_SUCCESS) goto fn_fail

#define HANDLE_ERROR(msg) \
    fprintf(stderr, "[%s:%d] %s\n", __FILE__, __LINE__, msg); exit(-1)

#define HANDLE_ERROR_WITH_CODE(msg,n) \
    fprintf(stderr, "[%s:%d] %s: %d\n", __FILE__, __LINE__, msg, n); exit(-1)

#define DEBUG 0
#if (DEBUG == 1)
#define DEBUG_PRINT(...)    fprintf(stderr, __VA_ARGS__); fflush(stderr)
#else
#define DEBUG_PRINT(...)
#endif

#endif /* ABTI_H_INCLUDED */
