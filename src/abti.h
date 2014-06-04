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
typedef struct ABTI_Stream       ABTI_Stream;
typedef enum ABTI_Stream_type    ABTI_Stream_type;
typedef struct ABTI_Thread       ABTI_Thread;
typedef enum ABTI_Thread_type    ABTI_Thread_type;
typedef struct ABTI_Task         ABTI_Task;
typedef struct ABTI_Mutex        ABTI_Mutex;
typedef struct ABTI_Condition    ABTI_Condition;
typedef struct ABTI_Barrier      ABTI_Barrier;
typedef struct ABTI_Scheduler    ABTI_Scheduler;
typedef enum ABTI_Scheduler_type ABTI_Scheduler_type;
typedef struct ABTI_Unit         ABTI_Unit;
typedef struct ABTI_Pool         ABTI_Pool;
typedef struct ABTI_Stream_pool  ABTI_Stream_pool;
typedef struct ABTI_Task_pool    ABTI_Task_pool;
typedef struct ABTI_Global       ABTI_Global;
typedef struct ABTI_Local        ABTI_Local;


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

enum ABTI_Stream_type {
    ABTI_STREAM_TYPE_PRIMARY,
    ABTI_STREAM_TYPE_SECONDARY
};

enum ABTI_Thread_type {
    ABTI_THREAD_TYPE_MAIN,
    ABTI_THREAD_TYPE_USER
};

enum ABTI_Scheduler_type {
    ABTI_SCHEDULER_TYPE_DEFAULT,
    ABTI_SCHEDULER_TYPE_USER
};


/* Definitions */
struct ABTI_Stream {
    ABT_Unit unit;             /* Unit enclosing this stream */
    uint64_t id;               /* ID */
    char *p_name;              /* Name */
    ABTI_Stream_type type;     /* Type */
    ABT_Stream_state state;    /* State */

    uint32_t request;          /* Request */
    ABT_Mutex mutex;           /* Mutex */
    ABTI_Scheduler *p_sched;   /* Scheduler */
    ABT_Pool deads;            /* Units terminated but still referenced */

    ABTD_Stream_context ctx;   /* Stream context */
};

struct ABTI_Thread {
    ABT_Unit unit;              /* Unit enclosing this thread */
    ABTI_Stream *p_stream;      /* Associated stream */
    uint64_t id;                /* ID */
    char *p_name;               /* Name */
    ABTI_Thread_type type;      /* Type */
    ABT_Thread_state state;     /* State */
    size_t stacksize;           /* Stack size */
    uint32_t refcount;          /* Reference count */

    void (*f_callback)(void *); /* Callback function */
    void *p_cb_arg;             /* Argument for callback function */

    uint32_t request;           /* Request */
    void *p_req_arg;            /* Request argument */
    ABT_Mutex mutex;            /* Mutex */
    void *p_stack;              /* Stack */

    ABTD_Thread_context ctx;    /* Context */
};

struct ABTI_Task {
    ABT_Unit unit;             /* Unit enclosing this task */
    ABTI_Stream *p_stream;     /* Associated stream */
    uint64_t id;               /* ID */
    char *p_name;              /* Name */
    ABT_Task_state state;      /* State */
    uint32_t refcount;         /* Reference count */

    uint32_t request;          /* Request */
    ABT_Mutex mutex;           /* Mutex */
    void (*f_task)(void *);    /* Task function */
    void *p_arg;               /* Task arguments */
};

struct ABTI_Mutex {
    uint32_t val;
};

struct ABTI_Condition {
};

struct ABTI_Scheduler {
    ABTI_Scheduler_type type; /* Type */
    ABT_Mutex mutex;          /* Mutex */
    ABTD_Thread_context ctx;  /* Scheduler context */
    ABT_Pool pool;            /* Work unit pool */

    /* Scheduler functions */
    ABT_Unit_get_type_fn           u_get_type;
    ABT_Unit_get_thread_fn         u_get_thread;
    ABT_Unit_get_task_fn           u_get_task;
    ABT_Unit_create_from_thread_fn u_create_from_thread;
    ABT_Unit_create_from_task_fn   u_create_from_task;
    ABT_Unit_free_fn               u_free;
    ABT_Pool_get_size_fn           p_get_size;
    ABT_Pool_push_fn               p_push;
    ABT_Pool_pop_fn                p_pop;
    ABT_Pool_remove_fn             p_remove;
};

struct ABTI_Unit {
    ABTI_Pool    *p_pool; /* Pool to which this unit belongs */
    ABT_Unit_type type;   /* Work unit type */
    void         *p_unit; /* Work unit object,
                             e.g., ABTI_Thread or ABTI_Task */
    ABTI_Unit    *p_prev; /* Previous unit in list */
    ABTI_Unit    *p_next; /* Next unit in list */
};

struct ABTI_Pool {
    size_t     num_units; /* Number of units */
    ABTI_Unit *p_head;    /* The first unit */
    ABTI_Unit *p_tail;    /* The last unit */
};

struct ABTI_Stream_pool {
    ABT_Pool created;  /* Streams in CREATED state */
    ABT_Pool active;   /* Streams in READY or RUNNING state */
    ABT_Pool deads;    /* Streams in TERMINATED state but not freed */
    ABT_Mutex mutex;   /* Mutex */
};

struct ABTI_Task_pool {
    ABT_Pool pool;     /* Tasks created but not assigned */
    ABT_Mutex mutex;   /* Mutex */
};

struct ABTI_Global {
    ABTI_Stream_pool *p_streams;  /* Stream pool */
    ABTI_Task_pool   *p_tasks;    /* Task pool */
};

struct ABTI_Local {
    ABTI_Stream *p_stream;  /* Current stream */
    ABTI_Thread *p_thread;  /* Current running thread */
};


/* Global Data */
extern ABTI_Global *gp_ABTI_Global;


/* ES Local Data */
extern ABTD_STREAM_LOCAL ABTI_Local *lp_ABTI_Local;


/* Init & Finalize */
int ABTI_Stream_pool_init(ABTI_Stream_pool *p_streams);
int ABTI_Stream_pool_finalize(ABTI_Stream_pool *p_streams);
int ABTI_Task_pool_init(ABTI_Task_pool *p_tasks);
int ABTI_Task_pool_finalize(ABTI_Task_pool *p_tasks);

/* Global Data */
int ABTI_Global_add_stream(ABTI_Stream *p_stream);
int ABTI_Global_move_stream(ABTI_Stream *p_stream);
int ABTI_Global_del_stream(ABTI_Stream *p_stream);
int ABTI_Global_get_created_stream(ABTI_Stream **p_stream);
int ABTI_Global_add_task(ABTI_Task *p_task);
int ABTI_Global_del_task(ABTI_Task *p_task);
int ABTI_Global_pop_task(ABTI_Task **p_task);
int ABTI_Global_has_task(int *result);

/* ES Local Data */
int ABTI_Local_init(ABTI_Stream *p_stream);
int ABTI_Local_finalize();
static inline ABTI_Stream *ABTI_Local_get_stream() {
    return lp_ABTI_Local->p_stream;
}
static inline void ABTI_Local_set_stream(ABTI_Stream *p_stream) {
    lp_ABTI_Local->p_stream = p_stream;
}
static inline ABTI_Thread *ABTI_Local_get_thread() {
    return lp_ABTI_Local->p_thread;
}
static inline void ABTI_Local_set_thread(ABTI_Thread *p_thread) {
    lp_ABTI_Local->p_thread = p_thread;
}

/* Execution Stream (ES) */
ABTI_Stream *ABTI_Stream_get_ptr(ABT_Stream stream);
ABT_Stream   ABTI_Stream_get_handle(ABTI_Stream *p_stream);
int ABTI_Stream_free(ABTI_Stream *p_stream);
int ABTI_Stream_start(ABTI_Stream *p_stream);
int ABTI_Stream_start_any();
int ABTI_Stream_schedule(ABTI_Stream *p_stream);
int ABTI_Stream_schedule_thread(ABTI_Thread *p_thread);
int ABTI_Stream_schedule_task(ABTI_Task *p_task);
int ABTI_Stream_terminate_thread(ABTI_Thread *p_thread);
int ABTI_Stream_terminate_task(ABTI_Task *p_task);
int ABTI_Stream_add_thread(ABTI_Thread *p_thread);
int ABTI_Stream_keep_thread(ABTI_Thread *p_thread);
int ABTI_Stream_keep_task(ABTI_Task *p_task);
int ABTI_Stream_print(ABTI_Stream *p_stream);

/* User-level Thread (ULT) */
ABTI_Thread *ABTI_Thread_get_ptr(ABT_Thread thread);
ABT_Thread   ABTI_Thread_get_handle(ABTI_Thread *p_thread);
int ABTI_Thread_create_main(ABTI_Stream *p_stream, ABTI_Thread **p_thread);
int ABTI_Thread_free_main(ABTI_Thread *p_thread);
int ABTI_Thread_free(ABTI_Thread *p_thread);
int ABTI_Thread_print(ABTI_Thread *p_thread);
void ABTI_Thread_func_wrapper(void (*thread_func)(void *), void *p_arg);
int ABTI_Thread_set_ready(ABT_Thread thread);
int ABTI_Thread_suspend();
ABT_Thread *ABTI_Thread_current();

/* Tasklet */
ABTI_Task *ABTI_Task_get_ptr(ABT_Task task);
ABT_Task   ABTI_Task_get_handle(ABTI_Task *p_task);
int ABTI_Task_free(ABTI_Task *p_task);
int ABTI_Task_print(ABTI_Task *p_task);

/* Mutex */
ABTI_Mutex *ABTI_Mutex_get_ptr(ABT_Mutex mutex);
ABT_Mutex   ABTI_Mutex_get_handle(ABTI_Mutex *p_mutex);
int ABTI_Mutex_waitlock(ABT_Mutex mutex);

/* Condition */

/* Scheduler */
ABTI_Scheduler *ABTI_Scheduler_get_ptr(ABT_Scheduler sched);
ABT_Scheduler   ABTI_Scheduler_get_handle(ABTI_Scheduler *p_sched);
int  ABTI_Scheduler_create_default(ABTI_Scheduler **p_newsched);
void ABTI_Scheduler_push(ABTI_Scheduler *p_sched, ABT_Unit unit);
ABT_Unit ABTI_Scheduler_pop(ABTI_Scheduler *p_sched);
void ABTI_Scheduler_remove(ABTI_Scheduler *p_sched, ABT_Unit unit);
int  ABTI_Scheduler_print(ABTI_Scheduler *p_sched);

/* Unit */
ABTI_Unit *ABTI_Unit_get_ptr(ABT_Unit unit);
ABT_Unit   ABTI_Unit_get_handle(ABTI_Unit *p_unit);
ABT_Unit_type ABTI_Unit_get_type(ABT_Unit unit);
ABT_Stream    ABTI_Unit_get_stream(ABT_Unit unit);
ABT_Thread    ABTI_Unit_get_thread(ABT_Unit unit);
ABT_Task      ABTI_Unit_get_task(ABT_Unit unit);
ABT_Unit      ABTI_Unit_create_from_stream(ABT_Stream stream);
ABT_Unit      ABTI_Unit_create_from_thread(ABT_Thread thread);
ABT_Unit      ABTI_Unit_create_from_task(ABT_Task task);
void          ABTI_Unit_free(ABT_Unit *unit);
int           ABTI_Unit_print(ABT_Unit unit);

/* Pool */
ABTI_Pool *ABTI_Pool_get_ptr(ABT_Pool pool);
ABT_Pool   ABTI_Pool_get_handle(ABTI_Pool *p_pool);
int ABTI_Pool_create(ABT_Pool *newpool);
int ABTI_Pool_free(ABT_Pool *pool);
size_t   ABTI_Pool_get_size(ABT_Pool pool);
void     ABTI_Pool_push(ABT_Pool pool, ABT_Unit unit);
ABT_Unit ABTI_Pool_pop(ABT_Pool pool);
void     ABTI_Pool_remove(ABT_Pool pool, ABT_Unit unit);
int      ABTI_Pool_print(ABT_Pool pool);


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
