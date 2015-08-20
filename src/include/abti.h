/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#ifndef ABTI_H_INCLUDED
#define ABTI_H_INCLUDED

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <limits.h>

#include "abt_config.h"
#include "abt.h"
#include "abtu.h"
#include "abti_error.h"
#include "abti_valgrind.h"


/* Constants */
#define ABTI_SCHED_NUM_PRIO         3

#define ABTI_XSTREAM_REQ_JOIN       (1 << 0)
#define ABTI_XSTREAM_REQ_EXIT       (1 << 1)
#define ABTI_XSTREAM_REQ_CANCEL     (1 << 2)

#define ABTI_SCHED_REQ_FINISH       (1 << 0)
#define ABTI_SCHED_REQ_EXIT         (1 << 1)

#define ABTI_THREAD_REQ_JOIN        (1 << 0)
#define ABTI_THREAD_REQ_EXIT        (1 << 1)
#define ABTI_THREAD_REQ_CANCEL      (1 << 2)
#define ABTI_THREAD_REQ_MIGRATE     (1 << 3)
#define ABTI_THREAD_REQ_TERMINATE   (1 << 4)
#define ABTI_THREAD_REQ_BLOCK       (1 << 5)
#define ABTI_THREAD_REQ_ORPHAN      (1 << 6)
#define ABTI_THREAD_REQ_NOPUSH      (1 << 7)

#define ABTI_TASK_REQ_CANCEL        (1 << 0)

#define ABTI_THREAD_INIT_ID         0xFFFFFFFFFFFFFFFF
#define ABTI_TASK_INIT_ID           0xFFFFFFFFFFFFFFFF

#define ABTI_INDENT                 4

enum ABTI_xstream_type {
    ABTI_XSTREAM_TYPE_PRIMARY,
    ABTI_XSTREAM_TYPE_SECONDARY
};

enum ABTI_sched_used {
    ABTI_SCHED_NOT_USED,
    ABTI_SCHED_MAIN,
    ABTI_SCHED_IN_POOL
};

enum ABTI_thread_type {
    ABTI_THREAD_TYPE_MAIN,
    ABTI_THREAD_TYPE_MAIN_SCHED,
    ABTI_THREAD_TYPE_USER
};


/* Macro functions */
#define ABTI_UNUSED(a)              (void)(a)


/* Data Types */
typedef struct ABTI_global          ABTI_global;
typedef struct ABTI_local           ABTI_local;
typedef struct ABTI_contn           ABTI_contn;
typedef struct ABTI_elem            ABTI_elem;
typedef struct ABTI_xstream         ABTI_xstream;
typedef enum ABTI_xstream_type      ABTI_xstream_type;
typedef struct ABTI_xstream_contn   ABTI_xstream_contn;
typedef struct ABTI_sched           ABTI_sched;
typedef char *                      ABTI_sched_config;
typedef enum ABTI_sched_used        ABTI_sched_used;
typedef void *                      ABTI_sched_id;      /* Scheduler id */
typedef uint64_t                    ABTI_sched_kind;    /* Scheduler kind */
typedef struct ABTI_pool            ABTI_pool;
typedef struct ABTI_unit            ABTI_unit;
typedef struct ABTI_thread_attr     ABTI_thread_attr;
typedef struct ABTI_thread          ABTI_thread;
typedef enum ABTI_thread_type       ABTI_thread_type;
typedef struct ABTI_thread_req_arg  ABTI_thread_req_arg;
typedef struct ABTI_thread_list     ABTI_thread_list;
typedef struct ABTI_thread_entry    ABTI_thread_entry;
typedef struct ABTI_task            ABTI_task;
typedef struct ABTI_mutex           ABTI_mutex;
typedef struct ABTI_cond            ABTI_cond;
typedef struct ABTI_eventual        ABTI_eventual;
typedef struct ABTI_future          ABTI_future;
typedef struct ABTI_timer           ABTI_timer;


/* Architecture-Dependent Definitions */
#include "abtd.h"


/* Definitions */
struct ABTI_mutex {
    uint32_t val;
};

struct ABTI_global {
    ABTI_xstream_contn *p_xstreams;    /* ES container */

    int num_cores;              /* Number of CPU cores */
    int set_affinity;           /* Whether CPU affinity is used */
    ABT_bool use_logging;       /* Whether logging is used */
    size_t thread_stacksize;    /* Default stack size for ULT (in bytes) */
    size_t sched_stacksize;     /* Default stack size for sched (in bytes) */
    uint32_t sched_event_freq;  /* Default check frequency for sched */
    ABTI_thread *p_thread_main; /* ULT of the main function */
};

struct ABTI_local {
    ABTI_xstream *p_xstream;    /* Current ES */
    ABTI_thread *p_thread;      /* Current running ULT */
    ABTI_task *p_task;          /* Current running tasklet */
};

struct ABTI_contn {
    size_t     num_elems; /* Number of elements */
    ABTI_elem *p_head;    /* The first element */
    ABTI_elem *p_tail;    /* The last element */
};

struct ABTI_elem {
    ABTI_contn   *p_contn; /* Container to which this element belongs */
    ABT_unit_type type;    /* Object type */
    void         *p_obj;   /* Object */
    ABTI_elem    *p_prev;  /* Previous element in list */
    ABTI_elem    *p_next;  /* Next element in list */
};

struct ABTI_xstream {
    ABTI_elem elem;             /* Elem enclosing this ES */
    uint64_t rank;              /* Rank */
    ABTI_xstream_type type;     /* Type */
    ABT_xstream_state state;    /* State */
    ABTI_sched **scheds;        /* Stack of running schedulers */
    int max_scheds;             /* Allocation size of the array scheds */
    int num_scheds;             /* Number of scheds */
    ABTI_mutex top_sched_mutex; /* Mutex for the top scheduler */

    uint32_t request;           /* Request */
    void *p_req_arg;            /* Request argument */
    ABTI_mutex mutex;           /* Mutex */
    ABTI_sched *p_main_sched;   /* Main scheduler */

    ABTD_xstream_context ctx;   /* ES context */
};

struct ABTI_xstream_contn {
    ABTI_contn *created; /* ESes in CREATED state */
    ABTI_contn *active;  /* ESes in READY or RUNNING state */
    ABTI_contn *deads;   /* ESes in TERMINATED state but not freed */
    ABTI_mutex mutex;    /* Mutex */
};

struct ABTI_sched {
    ABTI_sched_used used;       /* To know if it is used and how */
    ABT_bool automatic;         /* To know if automatic data free */
    ABTI_sched_kind kind;       /* Kind of the scheduler  */
    ABT_sched_type type;        /* Can yield or not (ULT or task) */
    ABT_sched_state state;      /* State */
    uint32_t request;           /* Request */
    ABT_pool *pools;            /* Work unit pools */
    int num_pools;              /* Number of work unit pools */
    ABTI_thread *p_thread;      /* Associated ULT */
    ABTI_task *p_task;          /* Associated tasklet */
    ABTD_thread_context *p_ctx; /* Context */
    void *data;                 /* Data for a specific scheduler */

    /* Scheduler functions */
    ABT_sched_init_fn init;
    ABT_sched_run_fn  run;
    ABT_sched_free_fn free;
    ABT_sched_get_migr_pool_fn get_migr_pool;

#ifdef ABT_CONFIG_USE_DEBUG_LOG
    uint64_t id;                /* ID */
#endif
};

struct ABTI_pool {
    ABT_pool_access access;  /* Access mode */
    ABT_bool automatic;      /* To know if automatic data free */
    int32_t num_scheds;      /* Number of associated schedulers */
                             /* NOTE: int32_t to check if still positive */
    ABTI_xstream *consumer;  /* Associated consumer ES */
#ifndef UNSAFE_MODE
    ABTI_xstream *producer;  /* Associated producer ES */
#endif
    uint32_t num_blocked;    /* Number of blocked ULTs */
    int32_t num_migrations;  /* Number of migrating ULTs */
    void *data;              /* Specific data */

    /* Functions to manage units */
    ABT_unit_get_type_fn           u_get_type;
    ABT_unit_get_thread_fn         u_get_thread;
    ABT_unit_get_task_fn           u_get_task;
    ABT_unit_is_in_pool_fn         u_is_in_pool;
    ABT_unit_create_from_thread_fn u_create_from_thread;
    ABT_unit_create_from_task_fn   u_create_from_task;
    ABT_unit_free_fn               u_free;

    /* Functions to manage the pool */
    ABT_pool_init_fn               p_init;
    ABT_pool_get_size_fn           p_get_size;
    ABT_pool_push_fn               p_push;
    ABT_pool_pop_fn                p_pop;
    ABT_pool_remove_fn             p_remove;
    ABT_pool_free_fn               p_free;

#ifdef ABT_CONFIG_USE_DEBUG_LOG
    uint64_t id;             /* ID */
#endif
};

struct ABTI_unit {
    ABTI_unit *p_prev;
    ABTI_unit *p_next;
    ABT_pool pool;
    union {
        ABT_thread thread;
        ABT_task   task;
    };
    ABT_unit_type type;
};

struct ABTI_thread_attr {
    size_t   stacksize : 63;            /* Stack size */
    ABT_bool migratable : 1;            /* Migratability */
    void (*f_cb)(ABT_thread, void *);   /* Callback function */
    void *p_cb_arg;                     /* Callback function argument */
};

struct ABTI_thread {
    ABT_unit unit;                  /* Unit enclosing this thread */
    ABTI_unit unit_def;             /* Internal unit definition */
    ABTI_thread_attr attr;          /* Attributes */
    void *p_stack;                  /* Stack */
    ABTD_thread_context ctx;        /* Context */
    ABTI_mutex mutex;               /* Mutex */

    ABTI_xstream *p_last_xstream;   /* Last ES where it ran */
    ABTI_sched *is_sched;           /* If it is a scheduler, its ptr */
    ABTI_pool *p_pool;              /* Associated pool */
    ABTI_thread_type type;          /* Type */
    ABT_thread_state state;         /* State */
    uint32_t refcount;              /* Reference count */
    uint32_t request;               /* Request */
    ABTI_thread_req_arg *p_req_arg; /* Request argument */

    ABT_thread_id id;               /* ID */
};

struct ABTI_thread_req_arg {
    uint32_t request;
    void *p_arg;
    ABTI_thread_req_arg *next;
};

struct ABTI_thread_list {
    ABTI_thread_entry *head;
    ABTI_thread_entry *tail;
};

struct ABTI_thread_entry {
    ABTI_thread *current;
    struct ABTI_thread_entry *next;
    ABT_unit_type type;
};

struct ABTI_task {
    ABT_unit unit;             /* Unit enclosing this task */
    ABTI_unit unit_def;        /* Internal unit definition */
    ABTI_xstream *p_xstream;   /* Associated ES */
    ABTI_sched *is_sched;      /* If it is a scheduler, its ptr */
    ABTI_pool *p_pool;         /* Associated pool */
    ABT_task_state state;      /* State */
    ABT_bool migratable;       /* Migratability */
    uint32_t refcount;         /* Reference count */
    uint32_t request;          /* Request */
    void (*f_task)(void *);    /* Task function */
    void *p_arg;               /* Task arguments */

    uint64_t id;               /* ID */
};

struct ABTI_cond {
    ABTI_mutex mutex;
    ABTI_mutex *p_waiter_mutex;
    size_t num_waiters;
    ABTI_thread_list waiters;
};

struct ABTI_eventual {
    ABTI_mutex mutex;
    ABT_bool ready;
    void *value;
    int nbytes;
    ABTI_thread_list waiters;
};

struct ABTI_future {
    ABTI_mutex mutex;
    ABT_bool ready;
    uint32_t counter;
    uint32_t compartments;
    void **array;
    void (*p_callback)(void **arg);
    ABTI_thread_list waiters;
};

struct ABTI_timer {
    ABTD_time start;
    ABTD_time end;
};


/* Global Data */
extern ABTI_global *gp_ABTI_global;

/* ES Local Data */
extern ABTD_XSTREAM_LOCAL ABTI_local *lp_ABTI_local;


/* Init & Finalize */
void ABTI_xstream_contn_init(ABTI_xstream_contn *p_xstreams);
int ABTI_xstream_contn_finalize(ABTI_xstream_contn *p_xstreams);

/* Global Data */
void ABTI_global_add_xstream(ABTI_xstream *p_xstream);
void ABTI_global_move_xstream(ABTI_xstream *p_xstream);
void ABTI_global_del_xstream(ABTI_xstream *p_xstream);
void ABTI_global_get_created_xstream(ABTI_xstream **p_xstream);

/* ES Local Data */
int ABTI_local_init(void);
int ABTI_local_finalize(void);

/* Container */
void       ABTI_contn_create(ABTI_contn **pp_contn);
int        ABTI_contn_free(ABTI_contn **pp_contn);
size_t     ABTI_contn_get_size(ABTI_contn *p_contn);
void       ABTI_contn_push(ABTI_contn *p_contn, ABTI_elem *p_elem);
ABTI_elem *ABTI_contn_pop(ABTI_contn *p_contn);
void       ABTI_contn_remove(ABTI_contn *p_contn, ABTI_elem *p_elem);
void       ABTI_contn_print(ABTI_contn *p_contn, FILE *p_os, int indent,
                            ABT_bool detail);

/* Element */
ABT_unit_type ABTI_elem_get_type(ABTI_elem *p_elem);
ABTI_xstream *ABTI_elem_get_xstream(ABTI_elem *p_elem);
ABTI_thread  *ABTI_elem_get_thread(ABTI_elem *p_elem);
ABTI_task    *ABTI_elem_get_task(ABTI_elem *p_elem);
ABTI_elem    *ABTI_elem_get_next(ABTI_elem *p_elem);
void          ABTI_elem_create_from_xstream(ABTI_xstream *p_xstream);
ABTI_elem    *ABTI_elem_create_from_thread(ABTI_thread *p_thread);
ABTI_elem    *ABTI_elem_create_from_task(ABTI_task *p_task);
void          ABTI_elem_free(ABTI_elem **pp_elem);
void          ABTI_elem_print(ABTI_elem *p_elem, FILE *p_os, int indent,
                              ABT_bool detail);

/* Execution Stream (ES) */
int ABTI_xstream_create(ABTI_sched *p_sched, ABTI_xstream **pp_xstream);
int ABTI_xstream_create_primary(ABTI_xstream **pp_xstream);
int ABTI_xstream_start(ABTI_xstream *p_xstream);
int ABTI_xstream_start_primary(ABTI_xstream *p_xstream, ABTI_thread *p_thread);
int ABTI_xstream_free(ABTI_xstream *p_xstream);
void ABTI_xstream_schedule(void *p_arg);
int ABTI_xstream_run_unit(ABTI_xstream *p_xstream, ABT_unit unit,
                          ABTI_pool *p_pool);
int ABTI_xstream_schedule_thread(ABTI_xstream *p_xstream,
                                 ABTI_thread *p_thread);
void ABTI_xstream_schedule_task(ABTI_xstream *p_xstream, ABTI_task *p_task);
int ABTI_xstream_migrate_thread(ABTI_thread *p_thread);
int ABTI_xstream_set_main_sched(ABTI_xstream *p_xstream, ABTI_sched *p_sched);
int ABTI_xstream_check_events(ABTI_xstream *p_xstream, ABT_sched sched);
void *ABTI_xstream_launch_main_sched(void *p_arg);
void ABTI_xstream_reset_rank(void);
void ABTI_xstream_free_ranks(void);
void ABTI_xstream_print(ABTI_xstream *p_xstream, FILE *p_os, int indent);

/* Scheduler */
ABT_sched_def *ABTI_sched_get_basic_def(void);
ABT_sched_def *ABTI_sched_get_prio_def(void);
int ABTI_sched_free(ABTI_sched *p_sched);
int ABTI_sched_get_migration_pool(ABTI_sched *, ABTI_pool *, ABTI_pool **);
ABTI_sched_kind ABTI_sched_get_kind(ABT_sched_def *def);
ABT_bool ABTI_sched_has_to_stop(ABTI_sched *p_sched, ABTI_xstream *p_xstream);
size_t ABTI_sched_get_size(ABTI_sched *p_sched);
size_t ABTI_sched_get_total_size(ABTI_sched *p_sched);
size_t ABTI_sched_get_effective_size(ABTI_sched *p_sched);
void ABTI_sched_print(ABTI_sched *p_sched, FILE *p_os, int indent);
void ABTI_sched_reset_id(void);

/* Scheduler config */
size_t ABTI_sched_config_type_size(ABT_sched_config_type type);
int ABTI_sched_config_read(ABT_sched_config config, int type, int num_vars,
                           void **variables);
int ABTI_sched_config_read_global(ABT_sched_config config,
                                  ABT_pool_access *access, ABT_bool *automatic);

/* Pool */
int ABTI_pool_get_fifo_def(ABT_pool_access access, ABT_pool_def *p_def);
int ABTI_pool_set_consumer(ABTI_pool *p_pool, ABTI_xstream *p_xstream);
#ifndef UNSAFE_MODE
int ABTI_pool_set_producer(ABTI_pool *p_pool, ABTI_xstream *p_xstream);
#endif
int ABTI_pool_accept_migration(ABTI_pool *p_pool, ABTI_pool *source);
void ABTI_pool_print(ABTI_pool *p_pool, FILE *p_os, int indent);
void ABTI_pool_reset_id(void);

/* User-level Thread (ULT)  */
int   ABTI_thread_migrate_to_pool(ABTI_thread *p_thread, ABTI_pool *p_pool);
int   ABTI_thread_create_main(ABTI_xstream *p_xstream, ABTI_thread **p_thread);
int   ABTI_thread_create_main_sched(ABTI_xstream *p_xstream, ABTI_sched *p_sched);
int   ABTI_thread_create_sched(ABTI_pool *p_pool, ABTI_sched *p_sched);
void  ABTI_thread_free(ABTI_thread *p_thread);
void  ABTI_thread_free_main(ABTI_thread *p_thread);
void  ABTI_thread_free_main_sched(ABTI_thread *p_thread);
int   ABTI_thread_set_blocked(ABTI_thread *p_thread);
void  ABTI_thread_suspend(ABTI_thread *p_thread);
int   ABTI_thread_set_ready(ABTI_thread *p_thread);
ABT_bool ABTI_thread_is_ready(ABTI_thread *p_thread);
void  ABTI_thread_set_attr(ABTI_thread *p_thread, ABT_thread_attr attr);
void  ABTI_thread_print(ABTI_thread *p_thread, FILE *p_os, int indent);
void  ABTI_thread_add_req_arg(ABTI_thread *p_thread, uint32_t req, void *arg);
void *ABTI_thread_extract_req_arg(ABTI_thread *p_thread, uint32_t req);
void  ABTI_thread_retain(ABTI_thread *p_thread);
void  ABTI_thread_release(ABTI_thread *p_thread);
void  ABTI_thread_reset_id(void);
ABT_thread_id ABTI_thread_get_id(ABTI_thread *p_thread);

/* ULT Attributes */
void ABTI_thread_attr_print(ABTI_thread_attr *p_attr, FILE *p_os, int indent);
void ABTI_thread_attr_get_str(ABTI_thread_attr *p_attr, char *p_buf);

/* Tasklet */
int ABTI_task_create_sched(ABTI_pool *p_pool, ABTI_sched *p_sched);
void ABTI_task_free(ABTI_task *p_task);
void ABTI_task_print(ABTI_task *p_task, FILE *p_os, int indent);
void ABTI_task_retain(ABTI_task *p_task);
void ABTI_task_release(ABTI_task *p_task);
void ABTI_task_reset_id(void);
uint64_t ABTI_task_get_id(ABTI_task *p_task);

/* Eventual */
void ABTI_eventual_signal(ABTI_eventual *p_eventual);

/* Future */
void ABTI_future_signal(ABTI_future *p_future);

#include "abti_log.h"
#include "abti_local.h"
#include "abti_global.h"
#include "abti_sched.h"
#include "abti_config.h"
#include "abti_pool.h"
#include "abti_stream.h"
#include "abti_thread.h"
#include "abti_thread_attr.h"
#include "abti_task.h"
#include "abti_mutex.h"
#include "abti_cond.h"
#include "abti_eventual.h"
#include "abti_future.h"
#include "abti_timer.h"

#endif /* ABTI_H_INCLUDED */
