/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <assert.h>
#include <abt.h>

#define DEFAULT_NUM_STREAMS     4
#define DEFAULT_NUM_THREADS     4
#define DEFAULT_NUM_TASKS       4
#define DEFAULT_POOL_SIZE       16

#define HANDLE_ERROR(ret,msg)                           \
    if (ret != ABT_SUCCESS) {                           \
        fprintf(stderr, "ERROR[%d]: %s\n", ret, msg);   \
        exit(EXIT_FAILURE);                             \
    }

typedef struct {
    size_t num;
    unsigned long long result;
} task_arg_t;

void thread_func(void *arg)
{
    size_t my_id = (size_t)arg;
    printf("[TH%lu]: brefore yield\n", my_id);
    ABT_Thread_yield();
    printf("[TH%lu]: doing something ...\n", my_id);
    ABT_Thread_yield();
    printf("[TH%lu]: after yield\n", my_id);
}

void task_func1(void *arg)
{
    int i;
    size_t num = (size_t)arg;
    unsigned long long result = 1;
    for (i = 2; i <= num; i++) {
        result += i;
    }
    printf("task_func1: num=%lu result=%llu\n", num, result);
}

void task_func2(void *arg)
{
    size_t i;
    task_arg_t *my_arg = (task_arg_t *)arg;
    unsigned long long result = 1;
    for (i = 2; i <= my_arg->num; i++) {
        result += i;
    }
    my_arg->result = result;
}

/* For a new scheduler */
typedef struct unit unit_t;
typedef struct pool pool_t;

struct unit {
    ABT_Unit_type type;
    union {
        ABT_Thread thread;
        ABT_Task   task;
    };

    pool_t *pool;
    size_t pos;
};

struct pool {
    size_t num_max;
    size_t num_units;
    size_t empty_pos;
    size_t unit_pos;
    unit_t **units;
};

void pool_init(pool_t *pool)
{
    pool->num_max = DEFAULT_POOL_SIZE;
    pool->num_units = 0;
    pool->empty_pos = 0;
    pool->unit_pos = 0;
    pool->units = (unit_t **)calloc(pool->num_max, sizeof(unit_t *));
}

void pool_set_next_empty_pos(pool_t *pool)
{
    if (pool->num_units == pool->num_max) {
        /* Increase the space for units */
        pool->num_max += DEFAULT_POOL_SIZE;
        pool->units = (unit_t **)realloc(pool->units,
                                         sizeof(unit_t *) * pool->num_max);
        bzero(pool->units + pool->num_units,
              sizeof(unit_t *) * DEFAULT_POOL_SIZE);
        pool->empty_pos = pool->num_units;
    } else {
        size_t i;
        for (i = 1; i <= pool->num_max; i++) {
            size_t pos = (i + pool->empty_pos) % pool->num_max;
            if (pool->units[pos] == NULL) {
                pool->empty_pos = pos;
                return;
            }
        }
    }
    fprintf(stderr, "ERROR: should not reach here\n");
}

void pool_set_next_unit_pos(pool_t *pool)
{
    if (pool->num_units == 0) return;

    size_t i;
    for (i = 1; i <= pool->num_max; i++) {
        size_t pos = (i + pool->unit_pos) % pool->num_max;
        if (pool->units[pos] != NULL) {
            pool->unit_pos = pos;
            return;
        }
    }
    fprintf(stderr, "ERROR: should not reach here\n");
}

ABT_Unit_type unit_get_type(ABT_Unit unit)
{
    unit_t *my_unit = (unit_t *)unit;
    return my_unit->type;
}

ABT_Thread unit_get_thread(ABT_Unit unit)
{
    unit_t *my_unit = (unit_t *)unit;
    if (my_unit->type == ABT_UNIT_TYPE_THREAD)
        return my_unit->thread;
    else
        return ABT_THREAD_NULL;
}

ABT_Task unit_get_task(ABT_Unit unit)
{
    unit_t *my_unit = (unit_t *)unit;
    if (my_unit->type == ABT_UNIT_TYPE_TASK)
        return my_unit->task;
    else
        return ABT_TASK_NULL;
}

ABT_Unit unit_create_from_thread(ABT_Thread thread)
{
    unit_t *my_unit = (unit_t *)malloc(sizeof(unit_t));
    my_unit->type = ABT_UNIT_TYPE_THREAD;
    my_unit->thread = thread;
    my_unit->pool = NULL;
    return (ABT_Unit)my_unit;
}

ABT_Unit unit_create_from_task(ABT_Task task)
{
    unit_t *my_unit = (unit_t *)malloc(sizeof(unit_t));
    my_unit->type = ABT_UNIT_TYPE_TASK;
    my_unit->task = task;
    my_unit->pool = NULL;
    return (ABT_Unit)my_unit;
}

void unit_free(ABT_Unit *unit)
{
    unit_t *my_unit = (unit_t *)(*unit);
    free(my_unit);
    *unit = ABT_UNIT_NULL;
}

size_t pool_get_size(ABT_Pool pool)
{
    pool_t *my_pool = (pool_t *)pool;
    return my_pool->num_units;
}

void pool_push(ABT_Pool pool, ABT_Unit unit)
{
    pool_t *my_pool = (pool_t *)pool;
    unit_t *my_unit = (unit_t *)unit;

    my_unit->pool = my_pool;
    my_unit->pos = my_pool->empty_pos;
    my_pool->units[my_pool->empty_pos] = my_unit;
    my_pool->num_units++;

    pool_set_next_empty_pos(my_pool);
}

ABT_Unit pool_pop(ABT_Pool pool)
{
    pool_t *my_pool = (pool_t *)pool;
    if (my_pool->num_units == 0)
        return ABT_UNIT_NULL;

    unit_t *my_unit = my_pool->units[my_pool->unit_pos];
    my_unit->pool = NULL;
    my_pool->units[my_pool->unit_pos] = NULL;
    my_pool->num_units--;

    pool_set_next_unit_pos(my_pool);

    return (ABT_Unit)my_unit;
}

void pool_remove(ABT_Pool pool, ABT_Unit unit)
{
    pool_t *my_pool = (pool_t *)pool;
    unit_t *my_unit = (unit_t *)unit;
    if (my_unit->pool != my_pool) {
        fprintf(stderr, "ERROR: not my pool\n");
        exit(EXIT_FAILURE);
    }

    my_pool->units[my_unit->pos] = NULL;
    if (my_unit->pos == my_pool->unit_pos)
        pool_set_next_unit_pos(my_pool);
    my_unit->pool = NULL;
}

int main(int argc, char *argv[])
{
    int i, j;
    int ret;
    int num_streams = DEFAULT_NUM_STREAMS;
    int num_threads = DEFAULT_NUM_THREADS;
    int num_tasks = DEFAULT_NUM_TASKS;
    if (argc > 1) num_streams = atoi(argv[1]);
    assert(num_streams >= 0);
    if (argc > 2) num_threads = atoi(argv[2]);
    assert(num_threads >= 0);
    if (argc > 3) num_tasks = atoi(argv[3]);
    assert(num_tasks >= 0);

    pool_t *pools;
    ABT_Scheduler *scheds;
    ABT_Scheduler_funcs sched_funcs;
    ABT_Stream *streams;
    ABT_Task *tasks;
    task_arg_t *task_args;

    pools = (pool_t *)malloc(sizeof(pool_t) * num_streams);
    scheds = (ABT_Scheduler *)malloc(sizeof(ABT_Scheduler) * num_streams);
    streams = (ABT_Stream *)malloc(sizeof(ABT_Stream) * num_streams);
    tasks = (ABT_Task *)malloc(sizeof(ABT_Task) * num_tasks);
    task_args = (task_arg_t *)malloc(sizeof(task_arg_t) * num_tasks);

    /* Initialize */
    ret = ABT_Init(argc, argv);
    HANDLE_ERROR(ret, "ABT_Init");

    /* Create a scheduler */
    sched_funcs.u_get_type = unit_get_type;
    sched_funcs.u_get_thread = unit_get_thread;
    sched_funcs.u_get_task = unit_get_task;
    sched_funcs.u_create_from_thread = unit_create_from_thread;
    sched_funcs.u_create_from_task = unit_create_from_task;
    sched_funcs.u_free = unit_free;
    sched_funcs.p_get_size = pool_get_size;
    sched_funcs.p_push = pool_push;
    sched_funcs.p_pop = pool_pop;
    sched_funcs.p_remove = pool_remove;

    for (i = 0; i < num_streams; i++) {
        /* Create a work unit pool */
        pool_init(&pools[i]);

        ret = ABT_Scheduler_create(&pools[i], &sched_funcs, &scheds[i]);
        HANDLE_ERROR(ret, "ABT_Scheduler_create");
    }

    /* Create streams */
    ret = ABT_Stream_self(&streams[0]);
    HANDLE_ERROR(ret, "ABT_Stream_self");
    ABT_Stream_set_scheduler(streams[0], scheds[0]);
    for (i = 1; i < num_streams; i++) {
        ret = ABT_Stream_create(scheds[i], &streams[i]);
        HANDLE_ERROR(ret, "ABT_Stream_create");
    }

    /* Create tasks with task_func1 */
    for (i = 0; i < num_tasks; i++) {
        size_t num = 100 + i;
        ret = ABT_Task_create(ABT_STREAM_NULL,
                              task_func1, (void *)num,
                              NULL);
        HANDLE_ERROR(ret, "ABT_Task_create");
    }

    /* Create threads */
    for (i = 0; i < num_streams; i++) {
        for (j = 0; j < num_threads; j++) {
            size_t tid = i * num_threads + j + 1;
            ret = ABT_Thread_create(streams[i],
                    thread_func, (void *)tid, 16384,
                    NULL);
            HANDLE_ERROR(ret, "ABT_Thread_create");
        }
    }

    /* Create tasks with task_func2 */
    for (i = 0; i < num_tasks; i++) {
        task_args[i].num = 100 + i;
        ret = ABT_Task_create(streams[i % num_streams],
                              task_func2, (void *)&task_args[i],
                              &tasks[i]);
        HANDLE_ERROR(ret, "ABT_Task_create");
    }

    /* Switch to other user level threads */
    ABT_Thread_yield();

    /* Results of task_funcs2 */
    for (i = 0; i < num_tasks; i++) {
        ABT_Task_state state;
        do {
            ABT_Task_get_state(tasks[i], &state);
            ABT_Thread_yield();
        } while (state != ABT_TASK_STATE_TERMINATED);

        printf("task_func2: num=%lu result=%llu\n",
               task_args[i].num, task_args[i].result);

        /* Free named tasks */
        ret = ABT_Task_free(&tasks[i]);
        HANDLE_ERROR(ret, "ABT_Task_free");
    }

    /* Join streams */
    for (i = 1; i < num_streams; i++) {
        ret = ABT_Stream_join(streams[i]);
        HANDLE_ERROR(ret, "ABT_Stream_join");
    }

    /* Free streams */
    for (i = 1; i < num_streams; i++) {
        ret = ABT_Stream_free(&streams[i]);
        HANDLE_ERROR(ret, "ABT_Stream_free");
    }

    /* Finalize */
    ret = ABT_Finalize();
    HANDLE_ERROR(ret, "ABT_Finalize");

    free(task_args);
    free(tasks);
    free(streams);
    free(scheds);
    for (i = 0; i < num_streams; i++) {
        free(pools[i].units);
    }
    free(pools);

    return EXIT_SUCCESS;
}

