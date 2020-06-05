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
#define ABTI_SCHED_NUM_PRIO 3

#define ABTI_XSTREAM_REQ_JOIN (1 << 0)
#define ABTI_XSTREAM_REQ_EXIT (1 << 1)
#define ABTI_XSTREAM_REQ_CANCEL (1 << 2)
#define ABTI_XSTREAM_REQ_STOP (1 << 3)

#define ABTI_SCHED_REQ_FINISH (1 << 0)
#define ABTI_SCHED_REQ_EXIT (1 << 1)

#define ABTI_THREAD_REQ_JOIN (1 << 0)
#define ABTI_THREAD_REQ_EXIT (1 << 1)
#define ABTI_THREAD_REQ_CANCEL (1 << 2)
#define ABTI_THREAD_REQ_MIGRATE (1 << 3)
#define ABTI_THREAD_REQ_TERMINATE (1 << 4)
#define ABTI_THREAD_REQ_BLOCK (1 << 5)
#define ABTI_THREAD_REQ_ORPHAN (1 << 6)
#define ABTI_THREAD_REQ_NOPUSH (1 << 7)
#define ABTI_THREAD_REQ_STOP (ABTI_THREAD_REQ_EXIT | ABTI_THREAD_REQ_TERMINATE)
#define ABTI_THREAD_REQ_NON_YIELD                                              \
    (ABTI_THREAD_REQ_EXIT | ABTI_THREAD_REQ_CANCEL | ABTI_THREAD_REQ_MIGRATE | \
     ABTI_THREAD_REQ_TERMINATE | ABTI_THREAD_REQ_BLOCK |                       \
     ABTI_THREAD_REQ_ORPHAN | ABTI_THREAD_REQ_NOPUSH)

#define ABTI_TASK_REQ_CANCEL (1 << 0)

#define ABTI_THREAD_INIT_ID 0xFFFFFFFFFFFFFFFF
#define ABTI_TASK_INIT_ID 0xFFFFFFFFFFFFFFFF

#define ABTI_INDENT 4

#define ABT_THREAD_TYPE_FULLY_FLEDGED 0
#define ABT_THREAD_TYPE_DYNAMIC_PROMOTION 1

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

enum ABTI_mutex_attr_val {
    ABTI_MUTEX_ATTR_NONE = 0,
    ABTI_MUTEX_ATTR_RECURSIVE = 1 << 0
};

enum ABTI_stack_type {
    ABTI_STACK_TYPE_MEMPOOL = 0, /* Stack taken from the memory pool */
    ABTI_STACK_TYPE_MALLOC,      /* Stack allocated by malloc in Argobots */
    ABTI_STACK_TYPE_USER,        /* Stack given by a user */
    ABTI_STACK_TYPE_MAIN,        /* Stack of a main ULT. */
};

/* Macro functions */
#define ABTI_UNUSED(a) (void)(a)

/* Data Types */
typedef struct ABTI_global ABTI_global;
typedef struct ABTI_local_func ABTI_local_func;
typedef struct ABTI_xstream ABTI_xstream;
typedef enum ABTI_xstream_type ABTI_xstream_type;
typedef struct ABTI_sched ABTI_sched;
typedef char *ABTI_sched_config;
typedef enum ABTI_sched_used ABTI_sched_used;
typedef void *ABTI_sched_id;       /* Scheduler id */
typedef uintptr_t ABTI_sched_kind; /* Scheduler kind */
typedef struct ABTI_pool ABTI_pool;
typedef struct ABTI_unit ABTI_unit;
typedef struct ABTI_thread_attr ABTI_thread_attr;
typedef struct ABTI_thread ABTI_thread;
typedef enum ABTI_thread_type ABTI_thread_type;
typedef enum ABTI_stack_type ABTI_stack_type;
typedef struct ABTI_thread_req_arg ABTI_thread_req_arg;
typedef struct ABTI_thread_list ABTI_thread_list;
typedef struct ABTI_thread_entry ABTI_thread_entry;
typedef struct ABTI_thread_htable ABTI_thread_htable;
typedef struct ABTI_thread_queue ABTI_thread_queue;
typedef struct ABTI_task ABTI_task;
typedef struct ABTI_key ABTI_key;
typedef struct ABTI_ktelem ABTI_ktelem;
typedef struct ABTI_ktable ABTI_ktable;
typedef struct ABTI_mutex_attr ABTI_mutex_attr;
typedef struct ABTI_mutex ABTI_mutex;
typedef struct ABTI_cond ABTI_cond;
typedef struct ABTI_rwlock ABTI_rwlock;
typedef struct ABTI_eventual ABTI_eventual;
typedef struct ABTI_future ABTI_future;
typedef struct ABTI_barrier ABTI_barrier;
typedef struct ABTI_timer ABTI_timer;
#ifdef ABT_CONFIG_USE_MEM_POOL
typedef struct ABTI_stack_header ABTI_stack_header;
typedef struct ABTI_page_header ABTI_page_header;
typedef struct ABTI_sp_header ABTI_sp_header;
#endif
/* ID associated with native thread (e.g, Pthreads), which can distinguish
 * execution streams and external threads */
struct ABTI_native_thread_id_opaque;
typedef struct ABTI_native_thread_id_opaque *ABTI_native_thread_id;
/* ID associated with work unit (i.e., ULTs, tasklets, and external threads) */
struct ABTI_unit_id_opaque;
typedef struct ABTI_unit_id_opaque *ABTI_unit_id;

/* Architecture-Dependent Definitions */
#include "abtd.h"

/* Spinlock */
typedef struct ABTI_spinlock ABTI_spinlock;
#include "abti_spinlock.h"

/* Basic data structure and memory pool. */
#include "abti_sync_lifo.h"
#include "abti_mem_pool.h"

/* Definitions */
struct ABTI_mutex_attr {
    uint32_t attrs;         /* bit-or'ed attributes */
    uint32_t nesting_cnt;   /* nesting count */
    ABTI_unit_id owner_id;  /* owner's ID */
    uint32_t max_handovers; /* max. # of handovers */
    uint32_t max_wakeups;   /* max. # of wakeups */
};

struct ABTI_mutex {
    ABTD_atomic_uint32 val;       /* 0: unlocked, 1: locked */
    ABTI_mutex_attr attr;         /* attributes */
    ABTI_thread_htable *p_htable; /* a set of queues */
    ABTI_thread *p_handover;      /* next ULT for the mutex handover */
    ABTI_thread *p_giver;         /* current ULT that hands over the mutex */
};

struct ABTI_global {
    int max_xstreams;            /* Max. size of p_xstreams */
    int num_xstreams;            /* Current # of ESs */
    ABTI_xstream **p_xstreams;   /* ES array */
    ABTI_spinlock xstreams_lock; /* Spinlock protecting p_xstreams. Any write
                                  * to p_xstreams and p_xstreams[*] requires a
                                  * lock. Dereference does not require a lock.*/

    int num_cores;              /* Number of CPU cores */
    ABT_bool set_affinity;      /* Whether CPU affinity is used */
    ABT_bool use_logging;       /* Whether logging is used */
    ABT_bool use_debug;         /* Whether debug output is used */
    int key_table_size;         /* Default key table size */
    size_t thread_stacksize;    /* Default stack size for ULT (in bytes) */
    size_t sched_stacksize;     /* Default stack size for sched (in bytes) */
    uint32_t sched_event_freq;  /* Default check frequency for sched */
    long sched_sleep_nsec;      /* Default nanoseconds for scheduler sleep */
    ABTI_thread *p_thread_main; /* ULT of the main function */

    uint32_t mutex_max_handovers; /* Default max. # of local handovers */
    uint32_t mutex_max_wakeups;   /* Default max. # of wakeups */
    uint32_t os_page_size;        /* OS page size */
    uint32_t huge_page_size;      /* Huge page size */
#ifdef ABT_CONFIG_USE_MEM_POOL
    ABTI_spinlock mem_task_lock;    /* Spinlock protecting p_mem_task */
    uint32_t mem_page_size;         /* Page size for memory allocation */
    uint32_t mem_sp_size;           /* Stack page size */
    uint32_t mem_max_stacks;        /* Max. # of stacks kept in each ES */
    int mem_lp_alloc;               /* How to allocate large pages */
    ABTI_stack_header *p_mem_stack; /* List of ULT stack */
    ABTI_page_header *p_mem_task;   /* List of task block pages */
    ABTI_sp_header *p_mem_sph;      /* List of stack pages */
#endif

    ABT_bool print_config; /* Whether to print config on ABT_init */
};

struct ABTI_local_func {
    char padding1[ABT_CONFIG_STATIC_CACHELINE_SIZE];
    ABTI_xstream *(*get_local_xstream_f)(void);
    void (*set_local_xstream_f)(ABTI_xstream *);
    void *(*get_local_ptr_f)(void);
    char padding2[ABT_CONFIG_STATIC_CACHELINE_SIZE];
};

struct ABTI_xstream {
    int rank;                 /* Rank */
    ABTI_xstream_type type;   /* Type */
    ABTD_atomic_int state;    /* State (ABT_xstream_state) */
    ABTI_sched **scheds;      /* Stack of running schedulers */
    ABTI_spinlock sched_lock; /* Lock for the scheduler management */
    ABTI_sched *p_main_sched; /* Main scheduler, which is the bottom of the
                               * linked list of schedulers */
    ABTI_sched *p_sched_top;  /* The currently running scheduler. This is the
                               * top of the linked list of schedulers. */

    ABTD_atomic_uint32 request; /* Request */
    void *p_req_arg;            /* Request argument */

    ABTD_xstream_context ctx; /* ES context */

    __attribute__((aligned(ABT_CONFIG_STATIC_CACHELINE_SIZE)))
    ABTI_thread *p_thread; /* Current running ULT */
    ABTI_task *p_task;     /* Current running tasklet */

#ifdef ABT_CONFIG_USE_MEM_POOL
    uint32_t num_stacks;               /* Current # of stacks */
    ABTI_stack_header *p_mem_stack;    /* Free stack list */
    ABTI_page_header *p_mem_task_head; /* Head of page list */
    ABTI_page_header *p_mem_task_tail; /* Tail of page list */
#endif
};

struct ABTI_sched {
    ABTI_sched_used used;       /* To know if it is used and how */
    ABT_bool automatic;         /* To know if automatic data free */
    ABTI_sched_kind kind;       /* Kind of the scheduler  */
    ABT_sched_type type;        /* Can yield or not (ULT or task) */
    ABT_sched_state state;      /* State */
    ABTD_atomic_uint32 request; /* Request */
    ABT_pool *pools;            /* Work unit pools */
    int num_pools;              /* Number of work unit pools */
    ABTI_thread *p_thread;      /* Associated ULT */
    void *data;                 /* Data for a specific scheduler */

    /* Pointers for a scheduler linked list. */
    ABTI_sched *p_parent_sched;
    ABTI_sched *p_child_sched;

    /* Scheduler functions */
    ABT_sched_init_fn init;
    ABT_sched_run_fn run;
    ABT_sched_free_fn free;
    ABT_sched_get_migr_pool_fn get_migr_pool;

#ifdef ABT_CONFIG_USE_DEBUG_LOG
    uint64_t id; /* ID */
#endif
};

struct ABTI_pool {
    ABT_pool_access access; /* Access mode */
    ABT_bool automatic;     /* To know if automatic data free */
#ifndef ABT_CONFIG_DISABLE_POOL_CONSUMER_CHECK
    ABTI_native_thread_id consumer_id; /* Associated consumer ID */
#endif
#ifndef ABT_CONFIG_DISABLE_POOL_PRODUCER_CHECK
    ABTI_native_thread_id producer_id; /* Associated producer ID */
#endif
    /* NOTE: int32_t to check if still positive */
    ABTD_atomic_int32 num_scheds;     /* Number of associated schedulers */
    ABTD_atomic_int32 num_blocked;    /* Number of blocked ULTs */
    ABTD_atomic_int32 num_migrations; /* Number of migrating ULTs */
    void *data;                       /* Specific data */
    uint64_t id;                      /* ID */

    /* Functions to manage units */
    ABT_unit_get_type_fn u_get_type;
    ABT_unit_get_thread_fn u_get_thread;
    ABT_unit_get_task_fn u_get_task;
    ABT_unit_is_in_pool_fn u_is_in_pool;
    ABT_unit_create_from_thread_fn u_create_from_thread;
    ABT_unit_create_from_task_fn u_create_from_task;
    ABT_unit_free_fn u_free;

    /* Functions to manage the pool */
    ABT_pool_init_fn p_init;
    ABT_pool_get_size_fn p_get_size;
    ABT_pool_push_fn p_push;
    ABT_pool_pop_fn p_pop;
    ABT_pool_pop_timedwait_fn p_pop_timedwait;
    ABT_pool_remove_fn p_remove;
    ABT_pool_free_fn p_free;
    ABT_pool_print_all_fn p_print_all;
};

struct ABTI_unit {
    ABTI_unit *p_prev;
    ABTI_unit *p_next;
    union {
        ABT_thread thread;
        ABT_task task;
    } handle;
    ABTD_atomic_int is_in_pool;
    ABT_unit_type type;
};

struct ABTI_thread_attr {
    void *p_stack;             /* Stack address */
    size_t stacksize;          /* Stack size (in bytes) */
    ABTI_stack_type stacktype; /* Stack type */
#ifndef ABT_CONFIG_DISABLE_MIGRATION
    ABT_bool migratable;              /* Migratability */
    void (*f_cb)(ABT_thread, void *); /* Callback function */
    void *p_cb_arg;                   /* Callback function argument */
#endif
};

struct ABTI_thread {
    ABTD_thread_context ctx;      /* Context */
    ABTI_unit unit_def;           /* Internal unit definition */
    ABTD_atomic_int state;        /* State (ABT_thread_state) */
    ABTD_atomic_uint32 request;   /* Request */
    ABTI_xstream *p_last_xstream; /* Last ES where it ran */
#ifndef ABT_CONFIG_DISABLE_STACKABLE_SCHED
    ABTI_sched *p_sched; /* Scheduler */
#endif
    ABT_unit unit;         /* Unit enclosing this thread */
    ABTI_pool *p_pool;     /* Associated pool */
    uint32_t refcount;     /* Reference count */
    ABTI_thread_type type; /* Type */
#ifndef ABT_CONFIG_DISABLE_MIGRATION
    ABTI_thread_req_arg *p_req_arg; /* Request argument */
    ABTI_spinlock lock;             /* Spinlock */
#endif
    ABTI_ktable *p_keytable; /* ULT-specific data */
    ABTI_thread_attr attr;   /* Attributes */
    ABT_thread_id id;        /* ID */
};

#ifndef ABT_CONFIG_DISABLE_MIGRATION
struct ABTI_thread_req_arg {
    uint32_t request;
    void *p_arg;
    ABTI_thread_req_arg *next;
};
#endif

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
    ABTI_xstream *p_xstream;    /* Associated ES */
    ABTD_atomic_int state;      /* State (ABT_task_state) */
    ABTD_atomic_uint32 request; /* Request */
    void (*f_task)(void *);     /* Task function */
    void *p_arg;                /* Task arguments */
    ABTI_pool *p_pool;          /* Associated pool */
    ABT_unit unit;              /* Unit enclosing this task */
    ABTI_unit unit_def;         /* Internal unit definition */
    uint32_t refcount;          /* Reference count */
    ABTI_ktable *p_keytable;    /* Tasklet-specific data */
#ifndef ABT_CONFIG_DISABLE_MIGRATION
    ABT_bool migratable; /* Migratability */
#endif
    uint64_t id; /* ID */
};

struct ABTI_key {
    void (*f_destructor)(void *value);
    uint32_t id;
    ABTD_atomic_uint32 refcount; /* Reference count */
    ABT_bool freed;              /* TRUE: freed, FALSE: not */
};

struct ABTI_ktelem {
    ABTI_key *p_key;
    void *value;
    struct ABTI_ktelem *p_next;
};

struct ABTI_ktable {
    int size;              /* size of the table */
    int num;               /* number of elements stored */
    ABTI_ktelem **p_elems; /* element array */
};

struct ABTI_cond {
    ABTI_spinlock lock;
    ABTI_mutex *p_waiter_mutex;
    size_t num_waiters;
    ABTI_unit *p_head; /* Head of waiters */
    ABTI_unit *p_tail; /* Tail of waiters */
};

struct ABTI_rwlock {
    ABTI_mutex mutex;
    ABTI_cond cond;
    size_t reader_count;
    int write_flag;
};

struct ABTI_eventual {
    ABTI_spinlock lock;
    ABT_bool ready;
    void *value;
    int nbytes;
    ABTI_unit *p_head; /* Head of waiters */
    ABTI_unit *p_tail; /* Tail of waiters */
};

struct ABTI_future {
    ABTI_spinlock lock;
    ABTD_atomic_uint32 counter;
    uint32_t compartments;
    void **array;
    void (*p_callback)(void **arg);
    ABTI_unit *p_head; /* Head of waiters */
    ABTI_unit *p_tail; /* Tail of waiters */
};

struct ABTI_barrier {
    uint32_t num_waiters;
    volatile uint32_t counter;
    ABTI_thread **waiters;
    ABT_unit_type *waiter_type;
    ABTI_spinlock lock;
};

struct ABTI_timer {
    ABTD_time start;
    ABTD_time end;
};

/* Global Data */
extern ABTI_global *gp_ABTI_global;
extern ABTI_local_func gp_ABTI_local_func;

/* ES Local Data */
extern ABTD_XSTREAM_LOCAL ABTI_xstream *lp_ABTI_xstream;

/* Global */
void ABTI_global_update_max_xstreams(int new_size);

/* Execution Stream (ES) */
int ABTI_xstream_create(ABTI_sched *p_sched, ABTI_xstream **pp_xstream);
int ABTI_xstream_create_primary(ABTI_xstream **pp_xstream);
int ABTI_xstream_start(ABTI_xstream *p_local_xstream, ABTI_xstream *p_xstream);
int ABTI_xstream_start_primary(ABTI_xstream **pp_local_xstream,
                               ABTI_xstream *p_xstream, ABTI_thread *p_thread);
int ABTI_xstream_free(ABTI_xstream *p_local_xstream, ABTI_xstream *p_xstream);
int ABTI_xstream_join(ABTI_xstream **pp_local_xstream, ABTI_xstream *p_xstream);
void ABTI_xstream_schedule(void *p_arg);
int ABTI_xstream_run_unit(ABTI_xstream **pp_local_xstream, ABT_unit unit,
                          ABTI_pool *p_pool);
int ABTI_xstream_schedule_thread(ABTI_xstream **pp_local_xstream,
                                 ABTI_thread *p_thread);
void ABTI_xstream_schedule_task(ABTI_xstream *p_local_xstream,
                                ABTI_task *p_task);
int ABTI_xstream_migrate_thread(ABTI_xstream *p_local_xstream,
                                ABTI_thread *p_thread);
int ABTI_xstream_init_main_sched(ABTI_xstream *p_xstream, ABTI_sched *p_sched);
int ABTI_xstream_update_main_sched(ABTI_xstream **pp_local_xstream,
                                   ABTI_xstream *p_xstream,
                                   ABTI_sched *p_sched);
int ABTI_xstream_check_events(ABTI_xstream *p_xstream, ABT_sched sched);
void *ABTI_xstream_launch_main_sched(void *p_arg);
void ABTI_xstream_print(ABTI_xstream *p_xstream, FILE *p_os, int indent,
                        ABT_bool print_sub);

/* Scheduler */
ABT_sched_def *ABTI_sched_get_basic_def(void);
ABT_sched_def *ABTI_sched_get_basic_wait_def(void);
ABT_sched_def *ABTI_sched_get_prio_def(void);
ABT_sched_def *ABTI_sched_get_randws_def(void);
void ABTI_sched_finish(ABTI_sched *p_sched);
void ABTI_sched_exit(ABTI_sched *p_sched);
int ABTI_sched_create(ABT_sched_def *def, int num_pools, ABT_pool *pools,
                      ABT_sched_config config, ABT_bool automatic,
                      ABTI_sched **pp_newsched);
int ABTI_sched_create_basic(ABT_sched_predef predef, int num_pools,
                            ABT_pool *pools, ABT_sched_config config,
                            ABTI_sched **pp_newsched);
int ABTI_sched_free(ABTI_xstream *p_local_xstream, ABTI_sched *p_sched);
int ABTI_sched_get_migration_pool(ABTI_sched *, ABTI_pool *, ABTI_pool **);
ABTI_sched_kind ABTI_sched_get_kind(ABT_sched_def *def);
ABT_bool ABTI_sched_has_to_stop(ABTI_xstream **pp_local_xstream,
                                ABTI_sched *p_sched);
size_t ABTI_sched_get_size(ABTI_sched *p_sched);
size_t ABTI_sched_get_total_size(ABTI_sched *p_sched);
size_t ABTI_sched_get_effective_size(ABTI_xstream *p_local_xstream,
                                     ABTI_sched *p_sched);
void ABTI_sched_print(ABTI_sched *p_sched, FILE *p_os, int indent,
                      ABT_bool print_sub);
void ABTI_sched_reset_id(void);

/* Scheduler config */
size_t ABTI_sched_config_type_size(ABT_sched_config_type type);
int ABTI_sched_config_read(ABT_sched_config config, int type, int num_vars,
                           void **variables);
int ABTI_sched_config_read_global(ABT_sched_config config,
                                  ABT_pool_access *access, ABT_bool *automatic);

/* Pool */
int ABTI_pool_create(ABT_pool_def *def, ABT_pool_config config,
                     ABT_bool automatic, ABTI_pool **pp_newpool);
int ABTI_pool_create_basic(ABT_pool_kind kind, ABT_pool_access access,
                           ABT_bool automatic, ABTI_pool **pp_newpool);
void ABTI_pool_free(ABTI_pool *p_pool);
int ABTI_pool_get_fifo_def(ABT_pool_access access, ABT_pool_def *p_def);
int ABTI_pool_get_fifo_wait_def(ABT_pool_access access, ABT_pool_def *p_def);
#ifndef ABT_CONFIG_DISABLE_POOL_CONSUMER_CHECK
int ABTI_pool_set_consumer(ABTI_pool *p_pool,
                           ABTI_native_thread_id consumer_id);
#endif
#ifndef ABT_CONFIG_DISABLE_POOL_PRODUCER_CHECK
int ABTI_pool_set_producer(ABTI_pool *p_pool,
                           ABTI_native_thread_id producer_id);
#endif
int ABTI_pool_accept_migration(ABTI_pool *p_pool, ABTI_pool *source);
void ABTI_pool_print(ABTI_pool *p_pool, FILE *p_os, int indent);
void ABTI_pool_reset_id(void);

/* Work Unit */
void ABTI_unit_set_associated_pool(ABT_unit unit, ABTI_pool *p_pool);

/* User-level Thread (ULT)  */
int ABTI_thread_migrate_to_pool(ABTI_xstream **pp_local_xstream,
                                ABTI_thread *p_thread, ABTI_pool *p_pool);
int ABTI_thread_create(ABTI_xstream *p_local_xstream, ABTI_pool *p_pool,
                       void (*thread_func)(void *), void *arg,
                       ABTI_thread_attr *p_attr, ABTI_thread **pp_newthread);
int ABTI_thread_create_main(ABTI_xstream *p_local_xstream,
                            ABTI_xstream *p_xstream, ABTI_thread **p_thread);
int ABTI_thread_create_main_sched(ABTI_xstream *p_local_xstream,
                                  ABTI_xstream *p_xstream, ABTI_sched *p_sched);
int ABTI_thread_create_sched(ABTI_xstream *p_local_xstream, ABTI_pool *p_pool,
                             ABTI_sched *p_sched);
void ABTI_thread_free(ABTI_xstream *p_local_xstream, ABTI_thread *p_thread);
void ABTI_thread_free_main(ABTI_xstream *p_local_xstream,
                           ABTI_thread *p_thread);
void ABTI_thread_free_main_sched(ABTI_xstream *p_local_xstream,
                                 ABTI_thread *p_thread);
int ABTI_thread_set_blocked(ABTI_thread *p_thread);
void ABTI_thread_suspend(ABTI_xstream **pp_local_xstream,
                         ABTI_thread *p_thread);
int ABTI_thread_set_ready(ABTI_xstream *p_local_xstream, ABTI_thread *p_thread);
void ABTI_thread_print(ABTI_thread *p_thread, FILE *p_os, int indent);
int ABTI_thread_print_stack(ABTI_thread *p_thread, FILE *p_os);
#ifndef ABT_CONFIG_DISABLE_MIGRATION
void ABTI_thread_add_req_arg(ABTI_thread *p_thread, uint32_t req, void *arg);
void *ABTI_thread_extract_req_arg(ABTI_thread *p_thread, uint32_t req);
#endif
void ABTI_thread_reset_id(void);
ABT_thread_id ABTI_thread_get_id(ABTI_thread *p_thread);
ABT_thread_id ABTI_thread_self_id(ABTI_xstream *p_local_xstream);
int ABTI_thread_get_xstream_rank(ABTI_thread *p_thread);
int ABTI_thread_self_xstream_rank(ABTI_xstream *p_local_xstream);

/* ULT Attributes */
void ABTI_thread_attr_print(ABTI_thread_attr *p_attr, FILE *p_os, int indent);
void ABTI_thread_attr_get_str(ABTI_thread_attr *p_attr, char *p_buf);
ABTI_thread_attr *ABTI_thread_attr_dup(ABTI_thread_attr *p_attr);

/* ULT hash table */
ABTI_thread_htable *ABTI_thread_htable_create(uint32_t num_rows);
void ABTI_thread_htable_free(ABTI_thread_htable *p_htable);
void ABTI_thread_htable_push(ABTI_thread_htable *p_htable, int idx,
                             ABTI_thread *p_thread);
ABT_bool ABTI_thread_htable_add(ABTI_thread_htable *p_htable, int idx,
                                ABTI_thread *p_thread);
void ABTI_thread_htable_push_low(ABTI_thread_htable *p_htable, int idx,
                                 ABTI_thread *p_thread);
ABT_bool ABTI_thread_htable_add_low(ABTI_thread_htable *p_htable, int idx,
                                    ABTI_thread *p_thread);
ABTI_thread *ABTI_thread_htable_pop(ABTI_thread_htable *p_htable,
                                    ABTI_thread_queue *p_queue);
ABTI_thread *ABTI_thread_htable_pop_low(ABTI_thread_htable *p_htable,
                                        ABTI_thread_queue *p_queue);
ABT_bool ABTI_thread_htable_switch_low(ABTI_xstream **pp_local_xstream,
                                       ABTI_thread_queue *p_queue,
                                       ABTI_thread *p_thread,
                                       ABTI_thread_htable *p_htable);

/* Tasklet */
void ABTI_task_free(ABTI_xstream *p_local_xstream, ABTI_task *p_task);
void ABTI_task_print(ABTI_task *p_task, FILE *p_os, int indent);
void ABTI_task_reset_id(void);
uint64_t ABTI_task_get_id(ABTI_task *p_task);

/* Key */
ABTI_ktable *ABTI_ktable_alloc(int size);
void ABTI_ktable_free(ABTI_ktable *p_ktable);

/* Mutex */
void ABTI_mutex_wait(ABTI_xstream **pp_local_xstream, ABTI_mutex *p_mutex,
                     int val);
void ABTI_mutex_wait_low(ABTI_xstream **pp_local_xstream, ABTI_mutex *p_mutex,
                         int val);
void ABTI_mutex_wake_se(ABTI_mutex *p_mutex, int num);
void ABTI_mutex_wake_de(ABTI_xstream *p_local_xstream, ABTI_mutex *p_mutex);

/* Mutex Attributes */
void ABTI_mutex_attr_print(ABTI_mutex_attr *p_attr, FILE *p_os, int indent);
void ABTI_mutex_attr_get_str(ABTI_mutex_attr *p_attr, char *p_buf);

/* Information */
int ABTI_info_print_config(FILE *fp);
void ABTI_info_check_print_all_thread_stacks(void);

#include "abti_log.h"
#include "abti_local.h"
#include "abti_global.h"
#include "abti_pool.h"
#include "abti_sched.h"
#include "abti_config.h"
#include "abti_stream.h"
#include "abti_self.h"
#include "abti_thread.h"
#include "abti_thread_attr.h"
#include "abti_task.h"
#include "abti_key.h"
#include "abti_mutex.h"
#include "abti_mutex_attr.h"
#include "abti_cond.h"
#include "abti_rwlock.h"
#include "abti_eventual.h"
#include "abti_future.h"
#include "abti_barrier.h"
#include "abti_timer.h"
#include "abti_mem.h"

#endif /* ABTI_H_INCLUDED */
