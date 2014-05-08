/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#ifndef ABTI_H_INCLUDED
#define ABTI_H_INCLUDED

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "abt.h"
#include "abtmem.h"

/* Type Definitions */
typedef unsigned int          unit;
typedef struct ABTD_Stream    ABTD_Stream;
typedef struct ABTD_Scheduler ABTD_Scheduler;
typedef struct ABTD_Pool      ABTD_Pool;
typedef struct ABTD_Unit      ABTD_Unit;
typedef struct ABTD_Thread    ABTD_Thread;
typedef struct ABTD_Task      ABTD_Task;

#include "abtarch.h"

typedef enum {
    ABT_STREAM_TYPE_MAIN,       /* Main program stream */
    ABT_STREAM_TYPE_CREATED     /* Explicitly created stream */
} ABT_Stream_type;

typedef enum {
    ABT_STREAM_STATE_READY,
    ABT_STREAM_STATE_RUNNING,
    ABT_STREAM_STATE_JOIN,
    ABT_STREAM_STATE_TERMINATED
} ABT_Stream_state;

/*S
  ABTD_Stream - Description of the Stream data structure

  Each stream internally has one thread, which schedules other threads.
  S*/
struct ABTD_Stream {
    ABT_Stream_id   id;     /* Stream ID */
    ABT_Stream_type type;   /* Type of execution stream */
    char           *name;   /* Stream name */
    volatile ABT_Stream_state state;  /* Stream state */
    ABTD_Scheduler *sched;  /* Scheduler */
    ABTA_ES_lock_t  lock;   /* Internal lock variable */
    ABT_Pool        deads;  /* Work units terminated but still referenced */
    ABTA_ULT_t      ult;    /* Internal ULT (scheduler) data structure */
    ABTA_ES_t       es;     /* Internal ES data structure */
};


typedef enum {
    ABT_SCHEDULER_TYPE_BASE,    /* Runtime-provided scheduler */
    ABT_SCHEDULER_TYPE_USER     /* User-provided scheduler */
} ABT_Scheduler_type;

struct ABTD_Scheduler {
    ABT_Scheduler_type type;
    ABT_Pool           pool;

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


struct ABTD_Pool {
    size_t num_units;
    ABTD_Unit *head;
    ABTD_Unit *tail;
};


/* ABTD_Unit is maintained as a circular doubly-linked list. */
struct ABTD_Unit {
    ABTD_Pool    *pool;
    ABT_Unit_type type;
    void         *unit;     /* ABTD_Thread or ABTD_Task */
    ABTD_Unit    *prev;
    ABTD_Unit    *next;
};


/*S
  ABTD_Thread - Description of the Thread data structure

  All threads are linked in a circular doubly-linked list.
  S*/
struct ABTD_Thread {
    ABTD_Stream     *stream;    /* Stream to which this thread belongs */
    ABT_Thread_id    id;        /* Thread ID */
    char            *name;      /* Thread name */
    uint             refcount;  /* Reference count */
    ABT_Thread_state state;     /* Thread state */
    size_t           stacksize; /* Stack size in bytes */
    void            *stack;     /* Pointer to this thread's stack */
    ABTA_ULT_t       ult;       /* Internal ULT data structure */
    ABT_Unit         unit;      /* Work unit enclosing this thread */
};


struct ABTD_Task {
    /* TODO */
};


/* Internal functions for Execution Stream */
extern __thread ABTD_Stream *g_stream;
int   ABTI_Stream_start(ABTD_Stream *stream);
void *ABTI_Stream_loop(void *arg);
int   ABTI_Stream_schedule(ABTD_Stream *stream);
int   ABTI_Stream_keep_thread(ABTD_Stream *stream, ABTD_Thread *thread);
#define ABTI_Stream_get_ptr(a)      (ABTD_Stream *)(a)
#define ABTI_Stream_get_handle(a)   (ABT_Stream)(a)


/* Internal functions for Scheduler */
int ABTI_Scheduler_create_default(ABTD_Scheduler **newsched);
#define ABTI_Scheduler_get_ptr(a)       (ABTD_Scheduler *)(a)
#define ABTI_Scheduler_get_handle(a)    (ABT_Scheduler)(a)


/* Internal functions for Pool */
int      ABTI_Pool_create(ABTD_Pool **newpool);
int      ABTI_Pool_free(ABTD_Pool *pool);
size_t   ABTI_Pool_get_size(ABT_Pool pool);
void     ABTI_Pool_push(ABT_Pool pool, ABT_Unit unit);
ABT_Unit ABTI_Pool_pop(ABT_Pool pool);
void     ABTI_Pool_remove(ABT_Pool pool, ABT_Unit unit);
#define ABTI_Pool_get_ptr(a)        (ABTD_Pool *)(a)
#define ABTI_Pool_get_handle(a)     (ABT_Pool)(a)


/* Internal functions for Work Unit */
ABT_Unit_type ABTI_Unit_get_type(ABT_Unit unit);
ABT_Thread    ABTI_Unit_get_thread(ABT_Unit unit);
ABT_Task      ABTI_Unit_get_task(ABT_Unit unit);
ABT_Unit      ABTI_Unit_create_from_thread(ABT_Thread thread);
ABT_Unit      ABTI_Unit_create_from_task(ABT_Task task);
void          ABTI_Unit_free(ABT_Unit unit);
#define ABTI_Unit_get_ptr(a)            (ABTD_Unit *)(a)
#define ABTI_Unit_get_handle(a)         (ABT_Unit)(a)


/* Internal functions for User Level Thread */
extern __thread ABTD_Thread *g_thread;
#define ABTI_Thread_get_ptr(a)      (ABTD_Thread *)(a)
#define ABTI_Thread_get_handle(a)   (ABT_Thread)(a)


#define HANDLE_ERROR(msg) \
    fprintf(stderr, "[%s:%d] %s\n", __FILE__, __LINE__, msg)

/* #define DEBUG */
#ifdef DEBUG
#define DEBUG_PRINT(...)    fprintf(stderr, __VA_ARGS__); fflush(stderr)
#else
#define DEBUG_PRINT(...)
#endif

#endif /* ABTI_H_INCLUDED */
