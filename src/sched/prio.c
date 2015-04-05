/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"


/* Priority Scheduler Implementation */

#define SCHED_PRIO_EVENT_FREQ 8

static int  sched_init(ABT_sched sched, ABT_sched_config config);
static void sched_run(ABT_sched sched);
static int  sched_free(ABT_sched);

static ABT_sched_def sched_prio_def = {
    .type = ABT_SCHED_TYPE_TASK,
    .init = sched_init,
    .run = sched_run,
    .free = sched_free,
    .get_migr_pool = NULL
};

typedef struct sched_config {
    int event_freq;
} sched_config;

static inline sched_config *sched_config_get_ptr(ABT_sched_config config)
{
    return (sched_config *)config;
}


/* Create a predefined priority scheduler */
int ABTI_sched_create_prio(int num_pools, ABT_pool *p_pools,
                           ABT_sched *newsched)
{
    return ABT_sched_create(&sched_prio_def, num_pools, p_pools,
                            ABT_SCHED_CONFIG_NULL, newsched);
}


static int sched_init(ABT_sched sched, ABT_sched_config config)
{
    int abt_errno = ABT_SUCCESS;
    sched_config *p_config;

    if (config == ABT_SCHED_CONFIG_NULL) {
        p_config = (sched_config *)ABTU_malloc(sizeof(sched_config));
        p_config->event_freq = SCHED_PRIO_EVENT_FREQ;
    } else {
        p_config = sched_config_get_ptr(config);
    }

    abt_errno = ABT_sched_set_data(sched, (void *)p_config);
    ABTI_CHECK_ERROR(abt_errno);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("prio: sched_init", abt_errno);
    goto fn_exit;
}

static void sched_run(ABT_sched sched)
{
    int abt_errno = ABT_SUCCESS;
    int i, work_count = 0;
    sched_config *p_config;
    int event_freq;
    int num_pools;
    ABT_pool *p_pools;
    void *p_data;
    ABT_unit unit;
    size_t size;

    ABTI_xstream *p_xstream = ABTI_local_get_xstream();
    ABTI_sched *p_sched = ABTI_sched_get_ptr(sched);

    abt_errno = ABT_sched_get_data(sched, &p_data);
    ABTI_CHECK_ERROR(abt_errno);

    p_config   = sched_config_get_ptr(p_data);
    event_freq = p_config->event_freq;

    /* Get the list of pools */
    ABT_sched_get_num_pools(sched, &num_pools);
    p_pools = (ABT_pool *)ABTU_malloc(num_pools * sizeof(ABT_pool));
    ABT_sched_get_pools(sched, num_pools, 0, p_pools);

    while (1) {
        /* Execute one work unit from the scheduler's pool */
        /* The pool with lower index has higher priority. */
        for (i = 0; i < num_pools; i++) {
            ABT_pool pool = p_pools[i];
            ABTI_pool *p_pool = ABTI_pool_get_ptr(pool);
            size = p_pool->p_get_size(pool);
            if (size > 0) {
                unit = p_pool->p_pop(pool);
                if (unit != ABT_UNIT_NULL) {
                    abt_errno = ABT_xstream_run_unit(unit, pool);
                    ABTI_CHECK_ERROR(abt_errno);
                }
                break;
            }
        }

        if (++work_count >= event_freq) {
            ABT_bool stop = ABTI_sched_has_to_stop(p_sched, p_xstream);
            if (stop == ABT_TRUE) break;
            work_count = 0;
            ABTI_xstream_check_events(p_xstream, sched);
        }
    }

    ABTU_free(p_pools);

  fn_exit:
    return;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("prio: sched_run", abt_errno);
    goto fn_exit;
}

static int sched_free(ABT_sched sched)
{
    int abt_errno = ABT_SUCCESS;
    void *p_data;
    sched_config *p_config;

    abt_errno = ABT_sched_get_data(sched, &p_data);
    ABTI_CHECK_ERROR(abt_errno);

    p_config = sched_config_get_ptr(p_data);
    ABTU_free(p_config);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("prio: sched_free", abt_errno);
    goto fn_exit;
}

