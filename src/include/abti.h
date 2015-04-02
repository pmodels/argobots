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
#include <assert.h>

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

#define ABTI_TASK_REQ_CANCEL        (1 << 0)

#define ABTI_THREAD_INIT_ID         0xFFFFFFFFFFFFFFFF
#define ABTI_TASK_INIT_ID           0xFFFFFFFFFFFFFFFF

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
struct ABTI_global {
    ABTI_xstream_contn *p_xstreams; /* ES container */

    int num_cores;                  /* Number of CPU cores */
    int set_affinity;               /* Whether CPU affinity is used */
    size_t default_stacksize;       /* Default stack size (in bytes) */
};

struct ABTI_local {
    ABTI_xstream *p_xstream;    /* Current ES */
    ABTI_thread *p_thread_main; /* ULT of the main function */
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
    ABTI_elem *elem;            /* Elem enclosing this ES */
    uint64_t rank;              /* Rank */
    char *p_name;               /* Name */
    ABTI_xstream_type type;     /* Type */
    ABT_xstream_state state;    /* State */
    ABTI_sched **scheds;        /* Stack of running schedulers */
    int max_scheds;             /* Allocation size of the array scheds */
    int num_scheds;             /* Number of scheds */
    ABT_mutex top_sched_mutex;  /* Mutex for the top scheduler */

    uint32_t request;           /* Request */
    ABT_mutex mutex;            /* Mutex */
    ABTI_sched *p_main_sched;   /* Main scheduler */
    ABTI_contn *deads;          /* Units terminated but still referenced */

    ABTD_xstream_context ctx;   /* ES context */
};

struct ABTI_xstream_contn {
    ABTI_contn *created; /* ESes in CREATED state */
    ABTI_contn *active;  /* ESes in READY or RUNNING state */
    ABTI_contn *deads;   /* ESes in TERMINATED state but not freed */
    ABT_mutex mutex;     /* Mutex */
};

struct ABTI_sched {
    ABT_mutex mutex;            /* Mutex */
    ABTI_sched_used used;       /* To know if it is used and how */
    ABT_bool automatic;         /* To know if automatic data free */
    ABTI_sched_kind kind;       /* Kind of the scheduler  */
    ABT_sched_type type;        /* Can yield or not (ULT or task) */
    ABT_sched_state state;      /* State */
    uint32_t request;           /* Request */
    ABT_pool *pools;            /* Work unit pools */
    int num_pools;              /* Number of work unit pools */
    ABT_thread thread;          /* Associated thread */
    ABT_task task;              /* Associated task */
    ABTD_thread_context *p_ctx; /* Context */
    void *data;                 /* Data for a specific scheduler */

    /* Scheduler functions */
    ABT_sched_init_fn init;
    ABT_sched_run_fn  run;
    ABT_sched_free_fn free;
    ABT_sched_get_migr_pool_fn get_migr_pool;
};

struct ABTI_pool {
    ABT_pool_access access;  /* Access mode */
    ABT_bool automatic;      /* To know if automatic data free */
    int32_t num_scheds;      /* Number of associated schedulers */
                             /* NOTE: int32_t to check if still positive */
    ABTI_xstream *reader;    /* Associated reader ES */
    ABTI_xstream *writer;    /* Associated writer ES */
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
};

struct ABTI_thread_attr {
    size_t         stacksize;           /* Stack size */
    ABT_bool       migratable;          /* Migratability */
    void (*f_cb)(ABT_thread, void *);   /* Callback function */
    void *p_cb_arg;                     /* Callback function argument */
};

struct ABTI_thread {
    ABT_unit unit;                  /* Unit enclosing this thread */
    ABTI_xstream *p_last_xstream;   /* Last ES where it ran */
    ABTI_sched *is_sched;           /* If it is a scheduler, its ptr */
    ABTI_pool *p_pool;              /* Associated pool */
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
    ABTI_xstream *p_xstream;   /* Associated ES */
    ABTI_sched *is_sched;      /* If it is a scheduler, its ptr */
    ABTI_pool *p_pool;         /* Associated pool */
    uint64_t id;               /* ID */
    char *p_name;              /* Name */
    ABT_task_state state;      /* State */
    ABT_bool migratable;       /* Migratability */
    uint32_t refcount;         /* Reference count */

    uint32_t request;          /* Request */
    ABT_mutex mutex;           /* Mutex */
    void (*f_task)(void *);    /* Task function */
    void *p_arg;               /* Task arguments */
};

struct ABTI_mutex {
    uint32_t val;
};

struct ABTI_cond {
    ABT_mutex mutex;
    ABT_mutex waiter_mutex;
    size_t num_waiters;
    ABTI_thread_list waiters;
};

struct ABTI_eventual {
    ABT_mutex mutex;
    ABT_bool ready;
    void *value;
    int nbytes;
    ABTI_thread_list waiters;
};

struct ABTI_future {
    ABT_mutex mutex;
    ABT_bool ready;
    int counter;
    int compartments;
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
int ABTI_xstream_contn_init(ABTI_xstream_contn *p_xstreams);
int ABTI_xstream_contn_finalize(ABTI_xstream_contn *p_xstreams);

/* Global Data */
int ABTI_global_add_xstream(ABTI_xstream *p_xstream);
int ABTI_global_move_xstream(ABTI_xstream *p_xstream);
int ABTI_global_del_xstream(ABTI_xstream *p_xstream);
int ABTI_global_get_created_xstream(ABTI_xstream **p_xstream);
size_t ABTI_global_get_default_stacksize();

/* ES Local Data */
int ABTI_local_init(void);
int ABTI_local_finalize(void);
static inline ABTI_xstream *ABTI_local_get_xstream(void) {
    return lp_ABTI_local->p_xstream;
}
static inline void ABTI_local_set_xstream(ABTI_xstream *p_xstream) {
    lp_ABTI_local->p_xstream = p_xstream;
}
static inline ABTI_thread *ABTI_local_get_thread(void) {
    return lp_ABTI_local->p_thread;
}
static inline void ABTI_local_set_thread(ABTI_thread *p_thread) {
    lp_ABTI_local->p_thread = p_thread;
}
static inline ABTI_thread *ABTI_local_get_main(void) {
    return lp_ABTI_local->p_thread_main;
}
static inline void ABTI_local_set_main(ABTI_thread *p_thread) {
    lp_ABTI_local->p_thread_main = p_thread;
}
static inline ABTI_task *ABTI_local_get_task(void) {
    return lp_ABTI_local->p_task;
}
static inline void ABTI_local_set_task(ABTI_task *p_task) {
    lp_ABTI_local->p_task = p_task;
}

/* Container */
int        ABTI_contn_create(ABTI_contn **pp_contn);
int        ABTI_contn_free(ABTI_contn **pp_contn);
size_t     ABTI_contn_get_size(ABTI_contn *p_contn);
void       ABTI_contn_push(ABTI_contn *p_contn, ABTI_elem *p_elem);
ABTI_elem *ABTI_contn_pop(ABTI_contn *p_contn);
void       ABTI_contn_remove(ABTI_contn *p_contn, ABTI_elem *p_elem);
int        ABTI_contn_print(ABTI_contn *p_contn);

/* Element */
ABT_unit_type ABTI_elem_get_type(ABTI_elem *p_elem);
ABTI_xstream *ABTI_elem_get_xstream(ABTI_elem *p_elem);
ABTI_thread  *ABTI_elem_get_thread(ABTI_elem *p_elem);
ABTI_task    *ABTI_elem_get_task(ABTI_elem *p_elem);
ABTI_elem    *ABTI_elem_get_next(ABTI_elem *p_elem);
ABTI_elem    *ABTI_elem_create_from_xstream(ABTI_xstream *p_xstream);
ABTI_elem    *ABTI_elem_create_from_thread(ABTI_thread *p_thread);
ABTI_elem    *ABTI_elem_create_from_task(ABTI_task *p_task);
void          ABTI_elem_free(ABTI_elem **pp_elem);
int           ABTI_elem_print(ABTI_elem *p_elem);

/* Execution Stream (ES) */
int ABTI_xstream_free(ABTI_xstream *p_xstream);
int ABTI_xstream_start_any(void);
int ABTI_xstream_schedule(ABTI_xstream *p_xstream);
int ABTI_xstream_schedule_thread(ABTI_thread *p_thread);
int ABTI_xstream_schedule_task(ABTI_task *p_task);
int ABTI_xstream_migrate_thread(ABTI_thread *p_thread);
int ABTI_xstream_terminate_thread(ABTI_thread *p_thread);
int ABTI_xstream_terminate_task(ABTI_task *p_task);
int ABTI_xstream_add_thread(ABTI_thread *p_thread);
int ABTI_xstream_keep_thread(ABTI_thread *p_thread);
int ABTI_xstream_keep_task(ABTI_task *p_task);
int ABTI_xstream_push_sched(ABTI_xstream *p_xstream, ABTI_sched *p_sched);
int ABTI_xstream_pop_sched(ABTI_xstream *p_xstream);
void ABTI_xstream_loop(void *p_arg);
void *ABTI_xstream_launch_main_sched(void *p_arg);
ABTI_sched *ABTI_xstream_get_top_sched(ABTI_xstream *p_xstream);
ABTD_thread_context *ABTI_xstream_get_sched_ctx(void);
void ABTI_xstream_reset_rank(void);
int ABTI_xstream_print(ABTI_xstream *p_xstream);

/* Scheduler */
int ABTI_sched_create_prio(int num_pools, ABT_pool *p_pools,
                           ABT_sched *newsched);
int ABTI_sched_associate(ABTI_sched *p_sched, ABTI_sched_used use);
int ABTI_sched_get_migration_pool(ABTI_sched *, ABTI_pool *, ABTI_pool **);
ABTI_sched_kind ABTI_sched_get_kind(ABT_sched_def *def);
int ABTI_sched_print(ABTI_sched *p_sched);

/* Scheduler config */
int ABTI_sched_config_read_global(ABT_sched_config config,
                                  ABT_pool_access *access, ABT_bool *automatic);

/* Pool */
int ABTI_pool_get_fifo_def(ABT_pool_access access, ABT_pool_def *p_def);
int ABTI_pool_retain(ABTI_pool *p_pool);
int ABTI_pool_release(ABTI_pool *p_pool);
int ABTI_pool_set_reader(ABTI_pool *p_pool, ABTI_xstream *p_xstream);
int ABTI_pool_set_writer(ABTI_pool *p_pool, ABTI_xstream *p_xstream);
int ABTI_pool_inc_num_blocked(ABTI_pool *p_pool);
int ABTI_pool_dec_num_blocked(ABTI_pool *p_pool);
int ABTI_pool_inc_num_migrations(ABTI_pool *p_pool);
int ABTI_pool_dec_num_migrations(ABTI_pool *p_pool);
int ABTI_pool_accept_migration(ABTI_pool *p_pool, ABTI_pool *source);
int ABTI_pool_print(ABTI_pool *p_pool);

/* User-level Thread (ULT)  */
int   ABTI_thread_migrate_to_pool(ABTI_thread *p_thread, ABTI_pool *p_pool);
int   ABTI_thread_create_main(ABTI_xstream *p_xstream, ABTI_thread **p_thread);
int   ABTI_thread_create_main_sched(ABTI_sched *p_sched, ABT_thread *newthread);
int   ABTI_thread_free_main(ABTI_thread *p_thread);
int   ABTI_thread_free(ABTI_thread *p_thread);
int   ABTI_thread_set_blocked(ABTI_thread *p_thread);
void  ABTI_thread_suspend(ABTI_thread *p_thread);
int   ABTI_thread_set_ready(ABTI_thread *p_thread);
ABT_bool ABTI_thread_is_ready(ABTI_thread *p_thread);
void  ABTI_thread_set_attr(ABTI_thread *p_thread, ABT_thread_attr attr);
int   ABTI_thread_print(ABTI_thread *p_thread);
ABTI_thread *ABTI_thread_current(void);
void  ABTI_thread_add_req_arg(ABTI_thread *p_thread, uint32_t req, void *arg);
void *ABTI_thread_extract_req_arg(ABTI_thread *p_thread, uint32_t req);
void  ABTI_thread_retain(ABTI_thread *p_thread);
void  ABTI_thread_release(ABTI_thread *p_thread);
void  ABTI_thread_reset_id(void);
ABT_thread_id ABTI_thread_get_id(ABTI_thread *p_thread);

/* ULT Attributes */
int ABTI_thread_attr_print(ABTI_thread_attr *p_attr);

/* Tasklet */
int  ABTI_task_free(ABTI_task *p_task);
int  ABTI_task_print(ABTI_task *p_task);
ABTI_task *ABTI_task_current(void);
void ABTI_task_retain(ABTI_task *p_task);
void ABTI_task_release(ABTI_task *p_task);
void ABTI_task_reset_id(void);
uint64_t ABTI_task_get_id(ABTI_task *p_task);

/* Eventual */
void ABTI_eventual_signal(ABTI_eventual *p_eventual);

/* Future */
void ABTI_future_signal(ABTI_future *p_future);

#include "abti_handle.h"

#define DEBUG 0
#if (DEBUG == 1)
#define DEBUG_PRINT(...)    fprintf(stderr, __VA_ARGS__); fflush(stderr)
#else
#define DEBUG_PRINT(...)
#endif

#endif /* ABTI_H_INCLUDED */
