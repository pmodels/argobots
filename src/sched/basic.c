/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"

/** @defgroup SCHED_BASIC Basic scheduler
 * This group is for the basic scheudler.
 */

#define SCHED_BASIC_EVENT_FREQ 8

static int  sched_init(ABT_sched sched, ABT_sched_config config);
static void sched_run(ABT_sched sched);
static int  sched_free(ABT_sched);
static void sched_sort_pools(int num_pools, ABT_pool *pools);

ABT_sched_def ABTI_sched_basic = {
    .type = ABT_SCHED_TYPE_TASK,
    .init = sched_init,
    .run = sched_run,
    .free = sched_free,
    .get_migr_pool = NULL,
};

struct sched_data {
    int event_freq;
    int num_pools;
    ABT_pool *pools;
};
typedef struct sched_data sched_data;

static inline sched_data *sched_data_get_ptr(ABT_sched_config config)
{
    return (sched_data *)config;
}

ABT_sched_config_var ABT_sched_basic_freq = {
  .idx = 0,
  .type = ABT_SCHED_CONFIG_INT
};

static int sched_init(ABT_sched sched, ABT_sched_config config)
{
    int abt_errno = ABT_SUCCESS;
    int num_pools;

    /* Default settings */
    sched_data *p_data;
    p_data = (sched_data *)ABTU_malloc(sizeof(sched_data));
    p_data->event_freq = SCHED_BASIC_EVENT_FREQ;

    /* Set the variables from the config */
    ABT_sched_config_read(config, 1, &p_data->event_freq);

    /* Save the list of pools */
    ABT_sched_get_num_pools(sched, &num_pools);
    p_data->num_pools = num_pools;
    p_data->pools = (ABT_pool *)ABTU_malloc(num_pools * sizeof(ABT_pool));
    abt_errno = ABT_sched_get_pools(sched, num_pools, 0, p_data->pools);
    ABTI_CHECK_ERROR(abt_errno);

    /* Sort pools according to their access mode so the scheduler can execute
       work units from the private pools. */
    if (num_pools > 1) {
        sched_sort_pools(num_pools, p_data->pools);
    }

    abt_errno = ABT_sched_set_data(sched, (void *)p_data);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("basic: sched_init", abt_errno);
    goto fn_exit;
}

static void sched_run(ABT_sched sched)
{
    int abt_errno = ABT_SUCCESS;
    int work_count = 0;
    void *data;
    sched_data *p_data;
    int event_freq;
    int num_pools;
    ABT_pool *pools;
    int i;

    ABT_sched_get_data(sched, &data);
    p_data = sched_data_get_ptr(data);
    event_freq = p_data->event_freq;
    num_pools  = p_data->num_pools;
    pools      = p_data->pools;

    while (1) {
        /* Execute one work unit from the scheduler's pool */
        for (i = 0; i < num_pools; i++) {
            ABT_pool pool = pools[i];
            ABTI_pool *p_pool = ABTI_pool_get_ptr(pool);
            size_t size = p_pool->p_get_size(pool);
            if (size > 0) {
                /* Pop one work unit */
                ABT_unit unit = p_pool->p_pop(pool);
                if (unit != ABT_UNIT_NULL) {
                    ABT_xstream_run_unit(unit, pool);
                }
                break;
            }
        }

        if (++work_count >= event_freq) {
            ABT_bool stop;
            ABT_sched_has_to_stop(sched, &stop);
            if (stop == ABT_TRUE)
                break;
            work_count = 0;
            ABT_xstream_check_events(sched);
        }
    }

  fn_exit:
    return ;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("basic: sched_run", abt_errno);
    goto fn_exit;
}

static int sched_free(ABT_sched sched)
{
    int abt_errno = ABT_SUCCESS;

    void *data;

    ABT_sched_get_data(sched, &data);
    sched_data *p_data = sched_data_get_ptr(data);
    ABTU_free(p_data->pools);
    ABTU_free(p_data);
    return abt_errno;
}

static int pool_get_access_num(ABT_pool *p_pool)
{
    ABT_pool_access access;
    int num = 0;

    ABT_pool_get_access(*p_pool, &access);
    switch (access) {
        case ABT_POOL_ACCESS_PRIV: num = 0; break;
        case ABT_POOL_ACCESS_SPSC:
        case ABT_POOL_ACCESS_MPSC: num = 1; break;
        case ABT_POOL_ACCESS_SPMC:
        case ABT_POOL_ACCESS_MPMC: num = 2; break;
        default: assert(0); break;
    }

    return num;
}

static int sched_cmp_pools(const void *p1, const void *p2)
{
    int p1_access, p2_access;

    p1_access = pool_get_access_num((ABT_pool *)p1);
    p2_access = pool_get_access_num((ABT_pool *)p2);

    if (p1_access > p2_access) {
        return 1;
    } else if (p1_access < p2_access) {
        return -1;
    } else {
        return 0;
    }
}

static void sched_sort_pools(int num_pools, ABT_pool *pools)
{
    qsort(pools, num_pools, sizeof(ABT_pool), sched_cmp_pools);
}

