/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"
#include <unistd.h>

#ifdef HAVE_PTHREAD_SETAFFINITY_NP
static cpu_set_t g_cpusets[CPU_SETSIZE];

static inline cpu_set_t ABTD_affinity_get_cpuset_for_rank(int rank)
{
    return g_cpusets[rank % gp_ABTI_global->num_cores];
}
#endif

void ABTD_affinity_init(void)
{
#ifdef HAVE_PTHREAD_SETAFFINITY_NP
    cpu_set_t cpuset;
    int i, ret;
    int num_cores = 0;

    ret = sched_getaffinity(getpid(), sizeof(cpu_set_t), &cpuset);
    ABTI_ASSERT(ret == 0);

    for (i = 0; i < CPU_SETSIZE; i++) {
        CPU_ZERO(&g_cpusets[i]);
        if (CPU_ISSET(i, &cpuset)) {
            CPU_SET(i, &g_cpusets[num_cores]);
            num_cores++;
        }
    }
    gp_ABTI_global->num_cores = num_cores;
#else
    /* Nothing to do */
#endif
}

int ABTD_affinity_set(ABTD_xstream_context ctx, int rank)
{
#ifdef HAVE_PTHREAD_SETAFFINITY_NP
    int abt_errno = ABT_SUCCESS;

    cpu_set_t cpuset = ABTD_affinity_get_cpuset_for_rank(rank);
    int ret = pthread_setaffinity_np(ctx, sizeof(cpu_set_t), &cpuset);
    ABTI_CHECK_TRUE(!ret, ABT_ERR_OTHER);

#if 0
    /* For debugging and verification */
    ret = pthread_getaffinity_np(ctx, sizeof(cpu_set_t), &cpuset);
    ABTI_CHECK_TRUE(!ret, ABT_ERR_OTHER);
    int i;
    for (i = 0; i < CPU_SETSIZE; i++) {
        if (CPU_ISSET(i, &cpuset)) {
            printf("ES%d mapped to core %d\n", rank, i);
        }
    }
#endif

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
#else
    return ABT_SUCCESS;
#endif
}

int ABTD_affinity_set_cpuset(ABTD_xstream_context ctx, int cpuset_size,
                             int *p_cpuset)
{
#ifdef HAVE_PTHREAD_SETAFFINITY_NP
    int abt_errno = ABT_SUCCESS;
    int i, ret;
    cpu_set_t cpuset;

    CPU_ZERO(&cpuset);
    for (i = 0; i < cpuset_size; i++) {
        ABTI_ASSERT(p_cpuset[i] < CPU_SETSIZE);
        CPU_SET(p_cpuset[i], &cpuset);
    }

    ret = pthread_setaffinity_np(ctx, sizeof(cpu_set_t), &cpuset);
    ABTI_CHECK_TRUE(!ret, ABT_ERR_OTHER);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
#else
    return ABT_ERR_FEATURE_NA;
#endif
}

int ABTD_affinity_get_cpuset(ABTD_xstream_context ctx, int cpuset_size,
                             int *p_cpuset, int *p_num_cpus)
{
#ifdef HAVE_PTHREAD_SETAFFINITY_NP
    int abt_errno = ABT_SUCCESS;
    int i, ret;
    cpu_set_t cpuset;
    int num_cpus = 0;

    ret = pthread_getaffinity_np(ctx, sizeof(cpu_set_t), &cpuset);
    ABTI_CHECK_TRUE(!ret, ABT_ERR_OTHER);

    if (p_cpuset != NULL) {
        for (i = 0; i < CPU_SETSIZE; i++) {
            if (CPU_ISSET(i, &cpuset)) {
                if (num_cpus < cpuset_size) {
                    p_cpuset[num_cpus] = i;
                } else {
                    break;
                }
                num_cpus++;
            }
        }
    }

    if (p_num_cpus != NULL) {
        *p_num_cpus = CPU_COUNT(&cpuset);
    }

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
#else
    return ABT_ERR_FEATURE_NA;
#endif
}

