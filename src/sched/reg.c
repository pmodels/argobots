/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include <stdlib.h>
#include <string.h>
#include "abti.h"

typedef struct {
  uint64_t event_num;
  uint32_t num_pools;
  uint32_t prev_pool;
  uint32_t successes;
  ABT_sched_policy *policies;
} sched_data_t;

static int sched_init(ABT_sched sched, ABT_sched_config config) {
  ABT_sched_policy *policies;

  sched_data_t *p_data = (sched_data_t *)calloc(1, sizeof(sched_data_t));

  p_data->event_num = 0;
  p_data->prev_pool = -1;
  p_data->successes = 0;
  ABT_sched_get_num_pools(sched, &p_data->num_pools);
  p_data->policies = (ABT_sched_policy *)calloc(p_data->num_pools, sizeof(ABT_sched_policy));
  
  ABT_sched_config_read(config, 1, &policies);
  memcpy(p_data->policies, policies, p_data->num_pools * sizeof(ABT_sched_policy));

  ABT_sched_set_data(sched, (void *)p_data);

  return ABT_SUCCESS;
}

static void sched_run(ABT_sched sched) {
  int cur_rank;
  sched_data_t* p_data;
  ABT_bool stop;

  int num_pools;
  ABT_pool *pools;

  ABT_xstream_self_rank(&cur_rank);
  ABT_sched_get_data(sched, (void **)&p_data);

  ABT_sched_get_num_pools(sched, &num_pools);
  pools = (ABT_pool *)malloc(num_pools * sizeof(ABT_pool));
  ABT_sched_get_pools(sched, num_pools, 0, pools);

  while (1) {
    int pool = -1;
    int prio = -1;
    double current_time = ABT_get_wtime();
    // TODO: Can also check "isEmpty" so we don't take from pools we don't want to
    for (int i = 0; i < num_pools; i++) {
      if ((p_data->policies[i].ready_at <= p_data->event_num &&
         p_data->policies[i].ready_at_wt <= current_time)  &&
         (p_data->policies[i].priority < prio || prio == -1)) {
        pool = i;
        prio = p_data->policies[i].priority;
      }
    }
    if (pool != p_data->prev_pool) {
      p_data->prev_pool = pool;
      p_data->successes = 0;
    }
    if (pool == -1) {
      p_data->event_num++;
    } else {
      ABT_sched_policy *policy = &p_data->policies[pool];
      for (int i = 0; i < policy->min_successes; i++) {
        /* Execute one work unit from the scheduler's pool */
        ABT_thread thread;
        current_time = ABT_get_wtime();
        ABT_pool_pop_thread(pools[pool], &thread);
        if (thread != ABT_THREAD_NULL) {
          /* "thread" is associated with its original pool (pools[0]). */
          ABT_self_schedule(thread, ABT_POOL_NULL);
          p_data->event_num++;
          p_data->successes++;
          if (p_data->successes >= policy->max_successes) {
            p_data->successes = 0;
            policy->ready_at = p_data->event_num + policy->success_timeout;
            policy->ready_at_wt = current_time + policy->success_timeout_wt;
            break;
          }
        } else {
          p_data->successes = 0;
          policy->ready_at = p_data->event_num + policy->fail_timeout;
          policy->ready_at_wt = current_time + policy->fail_timeout_wt;
          break;
        }
      }
    }
    ABT_sched_has_to_stop(sched, &stop);
    if (stop == ABT_TRUE)
        break;
    ABT_xstream_check_events(sched);
  }

  free(pools);
}

static int sched_free(ABT_sched sched) {
  sched_data_t* p_data;

  ABT_sched_get_data(sched, (void **)&p_data);
  free(p_data->policies);
  free(p_data);

  return ABT_SUCCESS;
}

void ABT_sched_create_reg(int num_pools, ABT_pool *pools,
                          int num_policies, ABT_sched_policy *policies,
                          ABT_sched *sched) {
  ABT_sched_config config;

  ABT_sched_config_var cv_policies = { .idx = 0,
                                       .type = ABT_SCHED_CONFIG_PTR };

  ABT_sched_def sched_def = { .type = ABT_SCHED_TYPE_ULT,
                              .init = sched_init,
                              .run = sched_run,
                              .free = sched_free,
                              .get_migr_pool = NULL };

  /* Create a scheduler config */
  ABT_sched_config_create(&config, cv_policies, policies,
                          ABT_sched_config_var_end);

  ABT_sched_create(&sched_def, num_pools, pools, config, sched);
  ABT_sched_config_free(&config);
}
