/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"

#define SCHED_BASIC_EVENT_FREQ 8

static int      sched_init(ABT_sched sched, ABT_sched_config config);
static void     sched_run(ABT_sched sched);
static int      sched_free(ABT_sched);
ABT_sched_def ABTI_sched_basic = {
    .type = ABT_SCHED_TYPE_TASK,
    .init = sched_init,
    .run = sched_run,
    .free = sched_free,
    .get_migr_pool = NULL,
} ;

struct sched_config {
    int event_freq;
} ;
typedef struct sched_config sched_config;
static sched_config *sched_config_get_ptr(ABT_sched_config config);


static int sched_init(ABT_sched sched, ABT_sched_config config)
{
    int abt_errno = ABT_SUCCESS;

    if (config == ABT_SCHED_CONFIG_NULL) {
        sched_config *p_data = (sched_config *)malloc(sizeof(sched_config));
        p_data->event_freq = SCHED_BASIC_EVENT_FREQ;
        config = (ABT_sched_config)p_data;
    }

    abt_errno = ABT_sched_set_data(sched, (void *)config);
    return abt_errno;
}


static void sched_run(ABT_sched sched)
{
    int abt_errno = ABT_SUCCESS;
    ABT_pool pool;
    int work_count = 0;
    void *data;

    ABT_sched_get_data(sched, &data);
    int event_freq = sched_config_get_ptr(data)->event_freq;

    ABT_sched_get_pools(sched, 1, 0, &pool);

    while (1) {
        /* Execute one work unit from the scheduler's pool */
        size_t size;
        ABT_pool_get_size(pool, &size);
        if (size > 0) {
            /* Pop one work unit */
            ABT_unit unit;
            abt_errno = ABT_pool_pop(pool, &unit);
            ABTI_CHECK_ERROR(abt_errno);
            if (unit != ABT_UNIT_NULL) {
                ABT_xstream_run_unit(unit, pool);
            }
        }

        if (++work_count >= event_freq) {
            int stop;
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
    sched_config *config = sched_config_get_ptr(data);
    free(config);
    return abt_errno;
}

static sched_config *sched_config_get_ptr(ABT_sched_config config)
{
    return (sched_config *)config;
}

