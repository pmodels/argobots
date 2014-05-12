/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#ifndef ABTI_H_INCLUDED
#define ABTI_H_INCLUDED

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "abt.h"
#include "abtu.h"

/* Type Definitions */
typedef unsigned int          unit;
typedef struct ABTI_Stream    ABTI_Stream;
typedef struct ABTI_Scheduler ABTI_Scheduler;
typedef struct ABTI_Pool      ABTI_Pool;
typedef struct ABTI_Unit      ABTI_Unit;
typedef struct ABTI_Thread    ABTI_Thread;
typedef struct ABTI_Task      ABTI_Task;
typedef struct ABTI_Task_pool ABTI_Task_pool;

#include "abtd.h"

typedef enum {
    ABTI_STREAM_TYPE_MAIN,       /* Main program stream */
    ABTI_STREAM_TYPE_CREATED     /* Explicitly created stream */
} ABTI_Stream_type;

/*S
  ABTI_Stream - Description of the Stream data structure

  Each stream internally has one thread, which schedules other threads.
  S*/
struct ABTI_Stream {
    ABT_Stream_id    id;      /* Stream ID */
    ABTI_Stream_type type;    /* Type of execution stream */
    char            *p_name;  /* Stream name */
    ABT_Stream_state state;   /* Stream state */
    ABTI_Scheduler  *p_sched; /* Scheduler */
    ABT_Pool         deads;   /* Work units terminated but still referenced */
    ABTD_ES_lock     lock;    /* Internal lock variable */
    ABTD_ES          es;      /* Internal ES data structure */
    ABTD_ULT         ult;     /* Internal ULT (scheduler) data structure */
    volatile int     joinreq; /* Whether join is requested */
};


typedef enum {
    ABTI_SCHEDULER_TYPE_BASE,    /* Runtime-provided scheduler */
    ABTI_SCHEDULER_TYPE_USER     /* User-provided scheduler */
} ABTI_Scheduler_type;

struct ABTI_Scheduler {
    ABTI_Scheduler_type type;
    ABT_Pool            pool;

    ABT_Unit_get_type_fn           u_get_type;
    ABT_Unit_get_thread_fn         u_get_thread;
    ABT_Unit_get_task_fn           u_get_task;
    ABT_Unit_create_from_thread_fn u_create_from_thread;
    ABT_Unit_create_from_task_fn   u_create_from_task;
    ABT_Unit_free_fn               u_free;

    ABT_Pool_get_size_fn p_get_size;
    ABT_Pool_push_fn     p_push;
    ABT_Pool_pop_fn      p_pop;
    ABT_Pool_remove_fn   p_remove;
};


struct ABTI_Pool {
    size_t     num_units;
    ABTI_Unit *p_head;
    ABTI_Unit *p_tail;
};


/* ABTI_Unit is maintained as a circular doubly-linked list. */
struct ABTI_Unit {
    ABTI_Pool    *p_pool;
    ABT_Unit_type type;
    void         *p_unit;    /* ABTI_Thread or ABTI_Task */
    ABTI_Unit    *p_prev;
    ABTI_Unit    *p_next;
};


#define ABTI_THREAD_DEFAULT_STACKSIZE   16384

/*S
  ABTI_Thread - Description of the Thread data structure

  All threads are linked in a circular doubly-linked list.
  S*/
struct ABTI_Thread {
    ABTI_Stream     *p_stream;  /* Stream to which this thread belongs */
    ABT_Thread_id    id;        /* Thread ID */
    char            *p_name;    /* Thread name */
    uint             refcount;  /* Reference count */
    ABT_Thread_state state;     /* Thread state */
    size_t           stacksize; /* Stack size in bytes */
    void            *p_stack;   /* Pointer to this thread's stack */
    ABT_Unit         unit;      /* Work unit enclosing this thread */
    ABTD_ULT         ult;       /* Internal ULT data structure */
};


struct ABTI_Task {
    ABTI_Stream   *p_stream;    /* Stream to which this task belongs to */
    ABT_Task_id    id;          /* Task ID */
    char          *p_name;      /* Task name */
    unit           refcount;    /* Reference count */
    ABT_Task_state state;       /* Task state */
    void (*f_task)(void *);     /* Task function */
    void          *p_arg;       /* Arguments for the task function */
    ABT_Unit       unit;        /* Work unit enclosing this task */
};

struct ABTI_Task_pool {
    ABT_Pool     pool;
    ABT_Pool     deads;
    ABTD_ES_lock lock;
};

/* Internal functions for Execution Stream */
extern __thread ABTI_Stream *gp_stream;
int   ABTI_Stream_start(ABTI_Stream *p_stream);
void *ABTI_Stream_loop(void *p_arg);
int   ABTI_Stream_schedule(ABTI_Stream *p_stream);
int   ABTI_Stream_keep_thread(ABTI_Stream *p_stream, ABTI_Thread *p_thread);
int   ABTI_Stream_keep_task(ABTI_Stream *p_stream, ABTI_Task *p_task);
#define ABTI_Stream_get_ptr(a)      (ABTI_Stream *)(a)
#define ABTI_Stream_get_handle(a)   (ABT_Stream)(a)


/* Internal functions for Scheduler */
int ABTI_Scheduler_create_default(ABTI_Scheduler **newsched);
#define ABTI_Scheduler_get_ptr(a)       (ABTI_Scheduler *)(a)
#define ABTI_Scheduler_get_handle(a)    (ABT_Scheduler)(a)


/* Internal functions for Pool */
int      ABTI_Pool_create(ABTI_Pool **newpool);
int      ABTI_Pool_free(ABTI_Pool *p_pool);
size_t   ABTI_Pool_get_size(ABT_Pool pool);
void     ABTI_Pool_push(ABT_Pool pool, ABT_Unit unit);
ABT_Unit ABTI_Pool_pop(ABT_Pool pool);
void     ABTI_Pool_remove(ABT_Pool pool, ABT_Unit unit);
#define ABTI_Pool_get_ptr(a)        (ABTI_Pool *)(a)
#define ABTI_Pool_get_handle(a)     (ABT_Pool)(a)


/* Internal functions for Work Unit */
ABT_Unit_type ABTI_Unit_get_type(ABT_Unit unit);
ABT_Thread    ABTI_Unit_get_thread(ABT_Unit unit);
ABT_Task      ABTI_Unit_get_task(ABT_Unit unit);
ABT_Unit      ABTI_Unit_create_from_thread(ABT_Thread thread);
ABT_Unit      ABTI_Unit_create_from_task(ABT_Task task);
void          ABTI_Unit_free(ABT_Unit unit);
#define ABTI_Unit_get_ptr(a)            (ABTI_Unit *)(a)
#define ABTI_Unit_get_handle(a)         (ABT_Unit)(a)


/* Internal functions for User Level Thread */
extern __thread ABTI_Thread *gp_thread;
void ABTI_Thread_func_wrapper(void (*thread_func)(void *), void *p_arg);
#define ABTI_Thread_get_ptr(a)      (ABTI_Thread *)(a)
#define ABTI_Thread_get_handle(a)   (ABT_Thread)(a)


/* Internal functions for Tasklet */
extern ABTI_Task_pool *gp_tasks;
int  ABTI_Task_execute();
void ABTI_Task_keep(ABTI_Task *p_task);
#define ABTI_Task_get_ptr(a)        (ABTI_Task *)(a)
#define ABTI_Task_get_handle(a)     (ABT_Task)(a)


#define HANDLE_ERROR(msg) \
    fprintf(stderr, "[%s:%d] %s\n", __FILE__, __LINE__, msg)

/* #define DEBUG */
#ifdef DEBUG
#define DEBUG_PRINT(...)    fprintf(stderr, __VA_ARGS__); fflush(stderr)
#else
#define DEBUG_PRINT(...)
#endif

#endif /* ABTI_H_INCLUDED */
