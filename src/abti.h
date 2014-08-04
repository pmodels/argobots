/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#ifndef ABTI_H_INCLUDED
#define ABTI_H_INCLUDED

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>
#include <assert.h>

#include "abt.h"
#include "abtu.h"
#include "abti_error.h"

/* Data Types */
typedef struct ABTI_xstream        ABTI_xstream;
typedef enum ABTI_xstream_type     ABTI_xstream_type;
typedef struct ABTI_thread         ABTI_thread;
typedef enum ABTI_thread_type      ABTI_thread_type;
typedef struct ABTI_thread_attr    ABTI_thread_attr;
typedef struct ABTI_thread_req_arg ABTI_thread_req_arg;
typedef struct ABTI_task           ABTI_task;
typedef struct ABTI_mutex          ABTI_mutex;
typedef struct ABTI_cond           ABTI_cond;
typedef struct ABTI_sched          ABTI_sched;
typedef enum ABTI_sched_type       ABTI_sched_type;
typedef struct ABTI_unit           ABTI_unit;
typedef struct ABTI_pool           ABTI_pool;
typedef struct ABTI_xstream_pool   ABTI_xstream_pool;
typedef struct ABTI_task_pool      ABTI_task_pool;
typedef struct ABTI_global         ABTI_global;
typedef struct ABTI_local          ABTI_local;
typedef struct ABTI_future         ABTI_future;
typedef struct ABTI_eventual       ABTI_eventual;

/* Architecture-Dependent Definitions */
#include "abtd.h"


/* Constants and Enums */
#define ABTI_THREAD_DEFAULT_STACKSIZE   16384
#define ABTI_SCHED_PRIO_NUM             3

#define ABTI_XSTREAM_REQ_JOIN       0x1
#define ABTI_XSTREAM_REQ_EXIT       0x2
#define ABTI_XSTREAM_REQ_CANCEL     0x4

#define ABTI_THREAD_REQ_JOIN        0x1
#define ABTI_THREAD_REQ_EXIT        0x2
#define ABTI_THREAD_REQ_CANCEL      0x4
#define ABTI_THREAD_REQ_MIGRATE     0x8

#define ABTI_TASK_REQ_CANCEL        0x1

enum ABTI_xstream_type {
    ABTI_XSTREAM_TYPE_PRIMARY,
    ABTI_XSTREAM_TYPE_SECONDARY
};

enum ABTI_thread_type {
    ABTI_THREAD_TYPE_MAIN,
    ABTI_THREAD_TYPE_USER
};

enum ABTI_sched_type {
    ABTI_SCHED_TYPE_DEFAULT,
    ABTI_SCHED_TYPE_BASIC,
    ABTI_SCHED_TYPE_USER
};


/* Definitions */
struct ABTI_xstream {
    ABT_unit unit;             /* Unit enclosing this ES */
    uint64_t id;               /* ID */
    char *p_name;              /* Name */
    ABTI_xstream_type type;    /* Type */
    ABT_xstream_state state;   /* State */

    uint32_t request;          /* Request */
    ABT_mutex mutex;           /* Mutex */
    ABTI_sched *p_sched;       /* Scheduler */
    ABT_pool deads;            /* Units terminated but still referenced */

    ABTD_xstream_context ctx;  /* ES context */
};

struct ABTI_thread_attr {
    size_t stacksize;           /* Stack size */
    ABT_sched_prio prio;        /* Priority */
    void (*f_callback)(void *); /* Callback function */
    void *p_cb_arg;             /* Callback function argument */
};

struct ABTI_thread_req_arg {
    uint32_t request;
    void *p_arg;
    ABTI_thread_req_arg *next;
};

struct ABTI_thread {
    ABT_unit unit;                  /* Unit enclosing this thread */
    ABTI_xstream *p_xstream;        /* Associated ES */
    ABT_thread_id id;               /* ID */
    char *p_name;                   /* Name */
    ABTI_thread_type type;          /* Type */
    ABT_thread_state state;         /* State */
    ABTI_thread_attr attr;          /* Attributes */
    uint32_t refcount;              /* Reference count */

    uint32_t request;               /* Request */
    ABTI_thread_req_arg *p_req_arg; /* Request argument */
    ABT_mutex mutex;                /* Mutex */
    void *p_stack;                  /* Stack */

    ABTD_thread_context ctx;        /* Context */
};

struct ABTI_task {
    ABT_unit unit;             /* Unit enclosing this task */
    ABTI_xstream *p_xstream;   /* Associated ES */
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

struct ABTI_sched {
    ABTI_xstream *p_xstream;  /* Associated ES */
    ABTI_sched_type type;     /* Type */
    ABT_sched_kind  kind;     /* Kind */
    ABT_mutex mutex;          /* Mutex */
    ABTD_thread_context ctx;  /* Scheduler context */
    ABT_pool pool;            /* Work unit pool */
    uint32_t num_blocked;     /* Number of blocked ULTs */

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
    void         *p_unit; /* Work unit object, e.g., ABTI_thread or ABTI_task */
    ABTI_unit    *p_prev; /* Previous unit in list */
    ABTI_unit    *p_next; /* Next unit in list */
};

struct ABTI_pool {
    size_t     num_units; /* Number of units */
    ABTI_unit *p_head;    /* The first unit */
    ABTI_unit *p_tail;    /* The last unit */
};

struct ABTI_xstream_pool {
    ABT_pool created;  /* ESes in CREATED state */
    ABT_pool active;   /* ESes in READY or RUNNING state */
    ABT_pool deads;    /* ESes in TERMINATED state but not freed */
    ABT_mutex mutex;   /* Mutex */
};

struct ABTI_task_pool {
    ABT_pool pool;     /* Tasks created but not assigned */
    ABT_mutex mutex;   /* Mutex */
};

struct ABTI_global {
    ABTI_xstream_pool *p_xstreams;  /* ES pool */
    ABTI_task_pool   *p_tasks;      /* Task pool */
};

struct ABTI_local {
    ABTI_xstream *p_xstream;  /* Current ES */
    ABTI_thread *p_thread;    /* Current running ULT */
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

struct ABTI_future {
    ABT_mutex mutex;
    int ready;
    int counter;
    void **array;
	void (*p_callback)(void **arg);
    ABTI_threads_list waiters;
};

struct ABTI_eventual {
    ABT_mutex mutex;
    int ready;
    void *value;
    int nbytes;
    ABTI_threads_list waiters;
};

struct ABTI_cond {
    ABT_mutex mutex;
    ABT_mutex waiter_mutex;
    size_t num_waiters;
    ABTI_threads_list waiters;
};

/* Global Data */
extern ABTI_global *gp_ABTI_global;


/* ES Local Data */
extern ABTD_XSTREAM_LOCAL ABTI_local *lp_ABTI_local;


/* Init & Finalize */
int ABTI_xstream_pool_init(ABTI_xstream_pool *p_xstreams);
int ABTI_xstream_pool_finalize(ABTI_xstream_pool *p_xstreams);
int ABTI_task_pool_init(ABTI_task_pool *p_tasks);
int ABTI_task_pool_finalize(ABTI_task_pool *p_tasks);

/* Global Data */
int ABTI_global_add_xstream(ABTI_xstream *p_xstream);
int ABTI_global_move_xstream(ABTI_xstream *p_xstream);
int ABTI_global_del_xstream(ABTI_xstream *p_xstream);
int ABTI_global_get_created_xstream(ABTI_xstream **p_xstream);
int ABTI_global_add_task(ABTI_task *p_task);
int ABTI_global_del_task(ABTI_task *p_task);
int ABTI_global_pop_task(ABTI_task **p_task);
int ABTI_global_has_task(int *result);

/* ES Local Data */
int ABTI_local_init(ABTI_xstream *p_xstream);
int ABTI_local_finalize();
static inline ABTI_xstream *ABTI_local_get_xstream() {
    return lp_ABTI_local->p_xstream;
}
static inline void ABTI_local_set_xstream(ABTI_xstream *p_xstream) {
    lp_ABTI_local->p_xstream = p_xstream;
}
static inline ABTI_thread *ABTI_local_get_thread() {
    return lp_ABTI_local->p_thread;
}
static inline void ABTI_local_set_thread(ABTI_thread *p_thread) {
    lp_ABTI_local->p_thread = p_thread;
}

/* Execution Stream (ES) */
ABTI_xstream *ABTI_xstream_get_ptr(ABT_xstream xstream);
ABT_xstream   ABTI_xstream_get_handle(ABTI_xstream *p_xstream);
int ABTI_xstream_free(ABTI_xstream *p_xstream);
int ABTI_xstream_start(ABTI_xstream *p_xstream);
int ABTI_xstream_start_any();
int ABTI_xstream_schedule(ABTI_xstream *p_xstream);
int ABTI_xstream_schedule_thread(ABTI_thread *p_thread);
int ABTI_xstream_schedule_task(ABTI_task *p_task);
int ABTI_xstream_terminate_thread(ABTI_thread *p_thread);
int ABTI_xstream_migrate_thread(ABTI_thread *p_thread);
int ABTI_xstream_terminate_task(ABTI_task *p_task);
int ABTI_xstream_add_thread(ABTI_thread *p_thread);
int ABTI_xstream_keep_thread(ABTI_thread *p_thread);
int ABTI_xstream_keep_task(ABTI_task *p_task);
int ABTI_xstream_print(ABTI_xstream *p_xstream);

/* User-level Thread (ULT) */
ABTI_thread *ABTI_thread_get_ptr(ABT_thread thread);
ABT_thread   ABTI_thread_get_handle(ABTI_thread *p_thread);
int ABTI_thread_create_main(ABTI_xstream *p_xstream, ABTI_thread **p_thread);
int ABTI_thread_free_main(ABTI_thread *p_thread);
int ABTI_thread_free(ABTI_thread *p_thread);
int ABTI_thread_suspend();
int ABTI_thread_set_ready(ABT_thread thread);
int ABTI_thread_set_attr(ABTI_thread *p_thread, ABTI_thread_attr *p_attr);
int ABTI_thread_print(ABTI_thread *p_thread);
void ABTI_thread_func_wrapper(int func_upper, int func_lower,
                              int arg_upper, int arg_lower);
ABT_thread *ABTI_thread_current();
void ABTI_thread_add_req_arg(ABTI_thread *p_thread, uint32_t req, void *arg);
void *ABTI_thread_extract_req_arg(ABTI_thread *p_thread, uint32_t req);

/* ULT Attributes */
ABTI_thread_attr *ABTI_thread_attr_get_ptr(ABT_thread_attr attr);
ABT_thread_attr ABTI_thread_attr_get_handle(ABTI_thread_attr *p_attr);
int ABTI_thread_attr_print(ABTI_thread_attr *p_attr);

/* Tasklet */
ABTI_task *ABTI_task_get_ptr(ABT_task task);
ABT_task   ABTI_task_get_handle(ABTI_task *p_task);
int ABTI_task_free(ABTI_task *p_task);
int ABTI_task_print(ABTI_task *p_task);

/* Mutex */
ABTI_mutex *ABTI_mutex_get_ptr(ABT_mutex mutex);
ABT_mutex   ABTI_mutex_get_handle(ABTI_mutex *p_mutex);
int ABTI_mutex_waitlock(ABT_mutex mutex);
int ABTI_mutex_equal(ABT_mutex mutex1, ABT_mutex mutex2, int *result);

/* Condition Variable */
ABTI_cond *ABTI_cond_get_ptr(ABT_cond cond);
ABT_cond   ABTI_cond_get_handle(ABTI_cond *p_cond);

/* Future */
ABTI_future *ABTI_future_get_ptr(ABT_future future);
ABT_future ABTI_future_get_handle(ABTI_future *p_future);

/* Eventual */
ABTI_eventual *ABTI_eventual_get_ptr(ABT_eventual eventual);
ABT_eventual ABTI_eventual_get_handle(ABTI_eventual *p_eventual);

/* Scheduler */
ABTI_sched *ABTI_sched_get_ptr(ABT_sched sched);
ABT_sched   ABTI_sched_get_handle(ABTI_sched *p_sched);
int ABTI_sched_create_default(ABTI_sched **p_newsched);
int ABTI_sched_create_fifo(ABTI_sched **p_newsched);
int ABTI_sched_create_lifo(ABTI_sched **p_newsched);
int ABTI_sched_create_prio(ABTI_sched **p_newsched);
int ABTI_sched_free_basic(ABTI_sched *p_sched);
int ABTI_sched_free_fifo(ABTI_sched *p_sched);
int ABTI_sched_free_lifo(ABTI_sched *p_sched);
int ABTI_sched_free_prio(ABTI_sched *p_sched);
void ABTI_sched_push(ABTI_sched *p_sched, ABT_unit unit);
ABT_unit ABTI_sched_pop(ABTI_sched *p_sched);
void ABTI_sched_remove(ABTI_sched *p_sched, ABT_unit unit);
int ABTI_sched_inc_num_blocked(ABTI_sched *p_sched);
int ABTI_sched_dec_num_blocked(ABTI_sched *p_sched);
int  ABTI_sched_print(ABTI_sched *p_sched);

/* Unit */
ABTI_unit *ABTI_unit_get_ptr(ABT_unit unit);
ABT_unit   ABTI_unit_get_handle(ABTI_unit *p_unit);
ABT_unit_type ABTI_unit_get_type(ABT_unit unit);
ABT_xstream   ABTI_unit_get_xstream(ABT_unit unit);
ABT_thread    ABTI_unit_get_thread(ABT_unit unit);
ABT_task      ABTI_unit_get_task(ABT_unit unit);
ABT_unit      ABTI_unit_get_next(ABT_unit unit);
ABT_unit      ABTI_unit_create_from_xstream(ABT_xstream xstream);
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


#define DEBUG 0
#if (DEBUG == 1)
#define DEBUG_PRINT(...)    fprintf(stderr, __VA_ARGS__); fflush(stderr)
#else
#define DEBUG_PRINT(...)
#endif

#endif /* ABTI_H_INCLUDED */
