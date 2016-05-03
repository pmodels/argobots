/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <assert.h>
#include "abt.h"
#include "dyn_event.h"

#define NUM_COMPS   100
#define NUM_ITERS   1000

typedef struct {
    int max_xstreams;           /* maximum # of ESs */
    int num_xstreams;           /* current # of ESs */
    ABT_xstream *xstreams;      /* ESs for this runtime */
    ABT_pool pool;
    ABT_mutex mutex;
    ABT_barrier bar;
    int stop_cb_id;
    int add_cb_id;

    int num_comps;
    int num_iters;
    double *app_data;
    unsigned int cnt;
} rt1_data_t;

static rt1_data_t *rt1_data;
extern double g_timeout;
extern ABT_xstream *g_xstreams;

static void rt1_app(int eid);
static void rt1_app_compute(void *arg);
static ABT_bool rt1_ask_stop_xstream(void *user_arg, void *abt_arg);
static ABT_bool rt1_act_stop_xstream(void *user_arg, void *abt_arg);
static ABT_bool rt1_ask_add_xstream(void *user_arg, void *abt_arg);
static ABT_bool rt1_act_add_xstream(void *user_arg, void *abt_arg);
static int sched_init(ABT_sched sched, ABT_sched_config config);
static void sched_run(ABT_sched sched);
static int sched_free(ABT_sched sched);


void rt1_init(int max_xstreams, ABT_xstream *xstreams)
{
    int i;
    char *env;

    rt1_data = (rt1_data_t *)calloc(1, sizeof(rt1_data_t));
    rt1_data->max_xstreams = max_xstreams;
    rt1_data->num_xstreams = max_xstreams;
    rt1_data->xstreams = (ABT_xstream *)malloc(max_xstreams*sizeof(ABT_xstream));
    for (i = 0; i < max_xstreams; i++) {
        rt1_data->xstreams[i] = xstreams[i];
    }

    ABT_pool_create_basic(ABT_POOL_FIFO, ABT_POOL_ACCESS_MPMC, ABT_TRUE,
                          &rt1_data->pool);
    ABT_mutex_create(&rt1_data->mutex);
    ABT_barrier_create(rt1_data->num_xstreams, &rt1_data->bar);

    /* Add event callbacks */
    /* NOTE: Each runtime needs to register callbacks only once for each event.
     * If it registers more than once, callbacks will be invoked as many times
     * as they are registered. */
    ABT_event_add_callback(ABT_EVENT_STOP_XSTREAM,
                           rt1_ask_stop_xstream, rt1_data,
                           rt1_act_stop_xstream, rt1_data,
                           &rt1_data->stop_cb_id);
    ABT_event_add_callback(ABT_EVENT_ADD_XSTREAM,
                           rt1_ask_add_xstream, rt1_data,
                           rt1_act_add_xstream, rt1_data,
                           &rt1_data->add_cb_id);

    /* application data */
    env = getenv("APP_NUM_COMPS");
    if (env) {
        rt1_data->num_comps = atoi(env);
    } else {
        rt1_data->num_comps = NUM_COMPS;
    }

    env = getenv("APP_NUM_ITERS");
    if (env) {
        rt1_data->num_iters = atoi(env);
    } else {
        rt1_data->num_iters = NUM_ITERS;
    }

    size_t num_elems = rt1_data->max_xstreams * rt1_data->num_comps * 2;
    rt1_data->app_data = (double *)calloc(num_elems, sizeof(double));

    printf("# of WUs created per ES: %d\n", rt1_data->num_comps);
    printf("# of iterations per WU : %d\n", rt1_data->num_iters);
}

void rt1_finalize(void)
{
    int i;

    while (rt1_data->num_xstreams > 0) {
        ABT_thread_yield();
    }

    for (i = 0; i < rt1_data->max_xstreams; i++) {
        assert(rt1_data->xstreams[i] == ABT_XSTREAM_NULL);
    }

    /* Delete registered callbacks */
    ABT_event_del_callback(ABT_EVENT_STOP_XSTREAM, rt1_data->stop_cb_id);
    ABT_event_del_callback(ABT_EVENT_ADD_XSTREAM, rt1_data->add_cb_id);

    ABT_mutex_lock(rt1_data->mutex);
    ABT_mutex_free(&rt1_data->mutex);
    ABT_barrier_free(&rt1_data->bar);
    free(rt1_data->xstreams);
    free(rt1_data->app_data);
    free(rt1_data);
    rt1_data = NULL;
}

void rt1_launcher(void *arg)
{
    int idx = (int)(intptr_t)arg;
    ABT_thread cur_thread;
    ABT_pool cur_pool;
    ABT_sched_config config;
    ABT_sched sched;
    size_t size;
    double t_start, t_end;

    ABT_sched_config_var cv_event_freq = {
        .idx = 0,
        .type = ABT_SCHED_CONFIG_INT
    };

    ABT_sched_config_var cv_idx = {
        .idx = 1,
        .type = ABT_SCHED_CONFIG_INT
    };

    ABT_sched_def sched_def = {
        .type = ABT_SCHED_TYPE_ULT,
        .init = sched_init,
        .run = sched_run,
        .free = sched_free,
        .get_migr_pool = NULL
    };

    /* Create a scheduler */
    ABT_sched_config_create(&config,
                            cv_event_freq, 10,
                            cv_idx, idx,
                            ABT_sched_config_var_end);
    ABT_sched_create(&sched_def, 1, &rt1_data->pool, config, &sched);

    /* Push the scheduler to the current pool */
    ABT_thread_self(&cur_thread);
    ABT_thread_get_last_pool(cur_thread, &cur_pool);
    ABT_pool_add_sched(cur_pool, sched);

    /* Free */
    ABT_sched_config_free(&config);

    t_start = ABT_get_wtime();
    while (1) {
        rt1_app(idx);

        ABT_pool_get_total_size(cur_pool, &size);
        if (size == 0) {
            ABT_sched_free(&sched);
            int rank;
            ABT_xstream_self_rank(&rank);
            printf("ES%d: finished\n", rank);
            ABT_mutex_lock(rt1_data->mutex);
            rt1_data->xstreams[rank] = ABT_XSTREAM_NULL;
            rt1_data->num_xstreams--;
            ABT_mutex_unlock(rt1_data->mutex);
            break;
        }

        t_end = ABT_get_wtime();
        if ((t_end - t_start) > g_timeout) {
            ABT_sched_finish(sched);
        }
    }
}

static void rt1_app(int eid)
{
    int i, num_comps;
    size_t size;
    ABT_thread cur_thread;
    ABT_pool cur_pool;

    ABT_thread_self(&cur_thread);
    ABT_thread_get_last_pool(cur_thread, &cur_pool);

    if (eid == 0) ABT_event_prof_start();

    num_comps = rt1_data->num_comps;
    for (i = 0; i < num_comps * 2; i += 2) {
        ABT_thread_create(rt1_data->pool, rt1_app_compute,
                          (void *)(intptr_t)(eid * num_comps * 2 + i),
                          ABT_THREAD_ATTR_NULL, NULL);
        ABT_task_create(rt1_data->pool, rt1_app_compute,
                        (void *)(intptr_t)(eid * num_comps * 2 + i + 1),
                        NULL);
    }

    do {
        ABT_thread_yield();

        /* If the size of cur_pool is zero, it means the stacked scheduler has
         * been terminated because of the shrinking event. */
        ABT_pool_get_total_size(cur_pool, &size);
        if (size == 0) break;

        ABT_pool_get_total_size(rt1_data->pool, &size);
    } while (size > 0);

    if (eid == 0) {
        ABT_event_prof_stop();

        int cnt = __atomic_exchange_n(&rt1_data->cnt, 0, __ATOMIC_SEQ_CST);
        double local_work = (double)(cnt * rt1_data->num_iters);
        ABT_event_prof_publish("ops", local_work, local_work);
    }
}

static void rt1_app_compute(void *arg)
{
    int pos = (int)(intptr_t)arg;
    int i;

    rt1_data->app_data[pos] = 0;
    for (i = 0; i < rt1_data->num_iters; i++) {
        rt1_data->app_data[pos] += sin((double)pos);
    }

    __atomic_fetch_add(&rt1_data->cnt, 1, __ATOMIC_SEQ_CST);
}


/******************************************************************************/
/* Event callback functions                                                   */
/******************************************************************************/
/* This function is the first callback function for the event,
 * ABT_EVENT_STOP_XSTREAM, and is invoked when the Argobots runtime receives a
 * shrinking request.  Its intention is to query whether the target ES, which
 * is passed as abt_arg, can be stopped.  If it is okay to stop the target ES,
 * this callback function should return ABT_TRUE.  Otherwise, it has to return
 * ABT_FALSE.  Since this callback function is for the agreement, it may be
 * called more than once.
 *
 * Parameters:
 *  [in] user_arg: user-provided argument
 *  [in] abt_arg : runtime-provided argument, the target ES to stop
 */
static ABT_bool rt1_ask_stop_xstream(void *user_arg, void *abt_arg)
{
    int rank;
    ABT_bool is_primary;
    rt1_data_t *my_data = (rt1_data_t *)user_arg;
    ABT_xstream tar_xstream = (ABT_xstream)abt_arg;

    if (my_data->num_xstreams == 1) {
        /* We cannot reduce the number of ESs anymore. */
        return ABT_FALSE;
    }

    /* The primary ES cannot be stopped */
    ABT_xstream_is_primary(tar_xstream, &is_primary);
    if (is_primary == ABT_TRUE) {
        return ABT_FALSE;
    }

    ABT_xstream_get_rank(tar_xstream, &rank);
    if (my_data->xstreams[rank] == ABT_XSTREAM_NULL) {
        /* The target ES is not used in this runtime. */
        return ABT_TRUE;
    }

    /* We may need other conditions to check? */

    return ABT_TRUE;
}

/* This function is the second callback function for the event,
 * ABT_EVENT_STOP_XSTREAM.  After all the first callback functions registered
 * for ABT_EVENT_STOP_XSTREAM return ABT_TRUE for the target ES, the Argobots
 * runtime will call this second callback function to notify that we have
 * reached an agreement and are going to stop the ES.
 *
 * Parameters:
 *  [in] user_arg: user-provided argument
 *  [in] abt_arg : runtime-provided argument, the target ES to stop
 */
static ABT_bool rt1_act_stop_xstream(void *user_arg, void *abt_arg)
{
    int rank;
    ABT_xstream xstream = (ABT_xstream)abt_arg;

    ABT_xstream_get_rank(xstream, &rank);
    g_xstreams[rank] = ABT_XSTREAM_NULL;

    return ABT_TRUE;
}

/* This function is the first callback function for the event,
 * ABT_EVENT_ADD_XSTREAM, and is invoked when the Argobots runtime receives a
 * expanding request.  Its intention is to query whether an ES with the target
 * ES, which is passed as abt_arg, can be created.  If it is possible to create
 * the target ES, this callback function should return ABT_TRUE.  Otherwise, it
 * has to return ABT_FALSE.  Since this callback function is for the agreement,
 * it may be called more than once.
 *
 * Parameters:
 *  [in] user_arg: user-provided argument
 *  [in] abt_arg : runtime-provided argument, the rank of ES to create
 */
static ABT_bool rt1_ask_add_xstream(void *user_arg, void *abt_arg)
{
    rt1_data_t *my_data = (rt1_data_t *)user_arg;
    int rank = (int)(intptr_t)abt_arg;

    if (my_data->num_xstreams >= my_data->max_xstreams) {
        return ABT_FALSE;
    }

    if (rank == ABT_XSTREAM_ANY_RANK) {
        return ABT_TRUE;
    }

    if (my_data->xstreams[rank] != ABT_XSTREAM_NULL) {
        return ABT_FALSE;
    }

    return ABT_TRUE;
}

/* This function is the second callback function for the event,
 * ABT_EVENT_ADD_XSTREAM.  After all the first callback functions registered
 * for ABT_EVENT_ADD_XSTREAM return ABT_TRUE for the target ES, the Argobots
 * runtime will call this second callback function to notify that we have
 * reached an agreement. For now, the creation of the target ES needs to be
 * happened in this callback function.
 *
 * Parameters:
 *  [in] user_arg: user-provided argument
 *  [in] abt_arg : runtime-provided argument, the rank of ES to create
 */
static ABT_bool rt1_act_add_xstream(void *user_arg, void *abt_arg)
{
    rt1_data_t *my_data = (rt1_data_t *)user_arg;
    int tar_rank = (int)(intptr_t)abt_arg;
    ABT_bool result = ABT_TRUE;
    ABT_pool pool;
    ABT_xstream tar_xstream;
    int rank, ret;

    /* Create a new ES */
    if (tar_rank == ABT_XSTREAM_ANY_RANK) {
        ret = ABT_xstream_create(ABT_SCHED_NULL, &tar_xstream);
        if (ret != ABT_SUCCESS) {
            result = ABT_FALSE;
            goto fn_exit;
        }
        ABT_xstream_get_rank(tar_xstream, &rank);
    } else {
        rank = tar_rank;
        ret = ABT_xstream_create_with_rank(ABT_SCHED_NULL, rank, &tar_xstream);
        if (ret != ABT_SUCCESS) {
            printf("ES%d: failed to create\n", rank);
            result = ABT_FALSE;
            goto fn_exit;
        }
    }
    ABT_mutex_spinlock(my_data->mutex);
    assert(rank < my_data->max_xstreams &&
           my_data->xstreams[rank] == ABT_XSTREAM_NULL);
    my_data->xstreams[rank] = tar_xstream;
    my_data->num_xstreams++;
    g_xstreams[rank] = tar_xstream;
    ABT_mutex_unlock(my_data->mutex);

    ABT_xstream_get_main_pools(tar_xstream, 1, &pool);
    ABT_thread_create(pool, rt1_launcher, (void *)(intptr_t)rank,
                      ABT_THREAD_ATTR_NULL, NULL);
    printf("ES%d: created\n", rank);

  fn_exit:
    return result;
}


/******************************************************************************/
/* Scheduler data structure and functions                                     */
/******************************************************************************/
typedef struct {
    uint32_t event_freq;
    int idx;
} sched_data_t;

static int sched_init(ABT_sched sched, ABT_sched_config config)
{
    int ret = ABT_SUCCESS;

    sched_data_t *p_data = (sched_data_t *)calloc(1, sizeof(sched_data_t));

    ABT_sched_config_read(config, 2, &p_data->event_freq, &p_data->idx);
    ret = ABT_sched_set_data(sched, (void *)p_data);

    return ret;
}

static void sched_run(ABT_sched sched)
{
    uint32_t work_count = 0;
    sched_data_t *p_data;
    ABT_pool my_pool;
    int num_pools;
    ABT_pool *pools;
    ABT_unit unit;
    int target;
    ABT_bool stop;
    unsigned seed = time(NULL);

    ABT_sched_get_data(sched, (void **)&p_data);
    ABT_sched_get_num_pools(sched, &num_pools);
    pools = (ABT_pool *)malloc(sizeof(ABT_pool) * num_pools);
    ABT_sched_get_pools(sched, num_pools, 0, pools);
    my_pool = pools[0];

    while (1) {
        /* Execute one work unit from the scheduler's pool */
        ABT_pool_pop(my_pool, &unit);
        if (unit != ABT_UNIT_NULL) {
            ABT_xstream_run_unit(unit, my_pool);
        } else if (num_pools > 1) {
            /* Steal a work unit from other pools */
            target = (num_pools == 2) ? 1 : (rand_r(&seed) % (num_pools-1) + 1);
            ABT_pool_pop(pools[target], &unit);
            if (unit != ABT_UNIT_NULL) {
                ABT_xstream_run_unit(unit, pools[target]);
            }
        }

        if (++work_count >= p_data->event_freq) {
            work_count = 0;
            ABT_xstream_check_events(sched);
            ABT_sched_has_to_stop(sched, &stop);
            if (stop == ABT_TRUE) {
                /* TODO: Migrate work units if we have private pools */
                break;
            }
        }
    }

    free(pools);
}

static int sched_free(ABT_sched sched)
{
    sched_data_t *p_data;

    ABT_sched_get_data(sched, (void **)&p_data);
    free(p_data);

    return ABT_SUCCESS;
}

