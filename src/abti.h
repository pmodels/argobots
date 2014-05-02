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
typedef struct ABTD_Stream ABTD_Stream;
typedef struct ABTD_Thread ABTD_Thread;
typedef size_t ABTD_Stream_id;
typedef size_t ABTD_Thread_id;

#include "abtarch.h"

typedef enum ABT_Stream_type {
    ABT_STREAM_TYPE_MAIN,       /* Main program stream */
    ABT_STREAM_TYPE_CREATED     /* Explicitly created stream */
} ABT_Stream_type;

typedef enum ABT_Stream_state {
    ABT_STREAM_STATE_READY,
    ABT_STREAM_STATE_RUNNING,
    ABT_STREAM_STATE_JOIN,
    ABT_STREAM_STATE_TERMINATED
} ABT_Stream_state;

/* Data Structures */
/*S
  ABTD_Stream - Description of the Stream data structure

  Each stream internally has one thread, which schedules other threads.
  S*/
struct ABTD_Stream {
    ABTD_Stream_id id;          /* Stream ID */
    ABTA_ES_t      es;          /* Internal ES data structure */
    ABTA_ES_lock_t lock;        /* Internal lock variable */
    size_t num_threads;         /* Number of created threads */
    ABTD_Thread *first_thread;  /* The first in circular doubly-linked list */
    ABTD_Thread *last_thread;   /* The last in circular doubly-linked list */
    ABTA_ULT_t ult;             /* Internal ULT (scheduler) data structure */
    ABT_Stream_type type;       /* Type of execution stream */
    volatile ABT_Stream_state state;  /* Stream state */
    char *name;                 /* Stream name */
};

/*S
  ABTD_Thread - Description of the Thread data structure

  All threads are linked in a circular doubly-linked list.
  S*/
struct ABTD_Thread {
    ABTD_Thread_id id;          /* Thread ID */
    ABTD_Stream *stream;        /* Stream to which this thread belongs */
    ABTA_ULT_t ult;             /* Internal ULT data structure */
    ABT_Thread_state state;     /* Thread state */
    size_t stacksize;           /* Stack size in bytes */
    void *stack;                /* Pointer to this thread's stack */
    ABTD_Thread *prev;          /* Previous thread in list */
    ABTD_Thread *next;          /* Next thread in list */
    char *name;                 /* Thread name */
};


/* Internal functions for Execution Stream */
extern __thread ABTD_Stream *g_stream;
int ABTI_Stream_start(ABTD_Stream *stream);
int ABTI_Stream_schedule_main(ABTD_Thread *thread);
int ABTI_Stream_schedule_to(ABTD_Stream *stream, ABTD_Thread *thread);
void ABTI_Stream_add_thread(ABTD_Stream *stream, ABTD_Thread *thread);
#define ABTI_Stream_get_ptr(a)      (ABTD_Stream *)(a)
#define ABTI_Stream_get_handle(a)   (ABT_Stream)(a)

/* Internal functions for User Level Thread */
extern __thread ABTD_Thread *g_thread;
#define ABTI_Thread_get_ptr(a)      (ABTD_Thread *)(a)
#define ABTI_Thread_get_handle(a)   (ABT_Thread)(a)


#define HANDLE_ERROR(msg) \
    fprintf(stderr, "[%s:%d] %s\n", __FILE__, __LINE__, msg)

//#define DEBUG
#ifdef DEBUG
#define DEBUG_PRINT(...)    fprintf(stderr, __VA_ARGS__)
#else
#define DEBUG_PRINT(...)
#endif

#endif /* ABTI_H_INCLUDED */
