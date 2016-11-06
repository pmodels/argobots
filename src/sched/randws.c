/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"

/* Random Work-stealing Scheduler Implementation */

static int  sched_init(ABT_sched sched, ABT_sched_config config);
static void sched_run(ABT_sched sched);
static int  sched_free(ABT_sched);

static ABT_sched_def sched_randws_def = {
    .type = ABT_SCHED_TYPE_TASK,
    .init = sched_init,
    .run = sched_run,
    .free = sched_free,
    .get_migr_pool = NULL,
};

typedef struct {
    uint32_t event_freq;
#ifdef ABT_CONFIG_USE_SCHED_SLEEP
    struct timespec sleep_time;
#endif
} sched_data;

ABT_sched_def *ABTI_sched_get_randws_def(void)
{
    return &sched_randws_def;
}

static int sched_init(ABT_sched sched, ABT_sched_config config)
{
    int abt_errno = ABT_SUCCESS;

    /* Default settings */
    sched_data *p_data = (sched_data *)ABTU_malloc(sizeof(sched_data));
    p_data->event_freq = ABTI_global_get_sched_event_freq();
#ifdef ABT_CONFIG_USE_SCHED_SLEEP
    p_data->sleep_time.tv_sec = 0;
    p_data->sleep_time.tv_nsec = ABTI_global_get_sched_sleep_nsec();
#endif

    /* Set the variables from the config */
    ABT_sched_config_read(config, 1, &p_data->event_freq);

    abt_errno = ABT_sched_set_data(sched, (void *)p_data);
    ABTI_CHECK_ERROR(abt_errno);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("randws: sched_init", abt_errno);
    goto fn_exit;
}

static void sched_run(ABT_sched sched)
{
    uint32_t work_count = 0;
    sched_data *p_data;
    int num_pools;
    ABT_pool *p_pools;
    ABT_unit unit;
    int target;
    unsigned seed = time(NULL);
    CNT_DECL(run_cnt);

    ABTI_xstream *p_xstream = ABTI_local_get_xstream();
    ABTI_sched *p_sched = ABTI_sched_get_ptr(sched);

    ABT_sched_get_data(sched, (void **)&p_data);
    ABT_sched_get_num_pools(sched, &num_pools);
    p_pools = (ABT_pool *)ABTU_malloc(num_pools * sizeof(ABT_pool));
    ABT_sched_get_pools(sched, num_pools, 0, p_pools);

    while (1) {
        CNT_INIT(run_cnt, 0);

        /* Execute one work unit from the scheduler's pool */
        ABT_pool pool = p_pools[0];
        ABTI_pool *p_pool = ABTI_pool_get_ptr(pool);
        size_t size = p_pool->p_get_size(pool);
        if (size > 0) {
            unit = p_pool->p_pop(pool);
            LOG_EVENT_POOL_POP(p_pool, unit);
            if (unit != ABT_UNIT_NULL) {
                ABTI_xstream_run_unit(p_xstream, unit, p_pool);
                CNT_INC(run_cnt);
            }
        } else if (num_pools > 1) {
            /* Steal a work unit from other pools */
            target = (num_pools == 2) ? 1 : (rand_r(&seed) % (num_pools-1) + 1);
            pool = p_pools[target];
            p_pool = ABTI_pool_get_ptr(pool);
            size = p_pool->p_get_size(pool);
            if (size > 0) {
                unit = p_pool->p_pop(pool);
                LOG_EVENT_POOL_POP(p_pool, unit);
                if (unit != ABT_UNIT_NULL) {
                    ABT_unit_set_associated_pool(unit, pool);
                    ABTI_xstream_run_unit(p_xstream, unit, p_pool);
                    CNT_INC(run_cnt);
                }
            }
        }

        if (++work_count >= p_data->event_freq) {
            ABT_bool stop = ABTI_sched_has_to_stop(p_sched, p_xstream);
            if (stop == ABT_TRUE) break;
            work_count = 0;
            ABTI_xstream_check_events(p_xstream, sched);
            SCHED_SLEEP(run_cnt, p_data->sleep_time);
        }
    }

    ABTU_free(p_pools);
}

static int sched_free(ABT_sched sched)
{
    sched_data *p_data;

    ABT_sched_get_data(sched, (void **)&p_data);
    ABTU_free(p_data);

    return ABT_SUCCESS;
}

