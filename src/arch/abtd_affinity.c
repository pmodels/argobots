/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"
#include <unistd.h>

#ifdef HAVE_PTHREAD_SETAFFINITY_NP
#if defined(__FreeBSD__)
#include <sys/param.h>
#include <sys/cpuset.h>
#include <pthread_np.h>

typedef cpuset_t  cpu_set_t;

static inline
int ABTD_CPU_COUNT(cpu_set_t *p_cpuset)
{
    int i, num_cpus = 0;
    for (i = 0; i < CPU_SETSIZE; i++) {
        if (CPU_ISSET(i, p_cpuset)) {
            num_cpus++;
        }
    }
    return num_cpus;
}

#else
#define _GNU_SOURCE
#include <sched.h>
#define ABTD_CPU_COUNT  CPU_COUNT
#endif

enum {
    ABTI_ES_AFFINITY_CHAMELEON,
    ABTI_ES_AFFINITY_KNC,
    ABTI_ES_AFFINITY_DEFAULT
};
static int g_affinity_type = ABTI_ES_AFFINITY_DEFAULT;
static cpu_set_t g_cpusets[CPU_SETSIZE];

static inline cpu_set_t ABTD_affinity_get_cpuset_for_rank(int rank)
{
    int num_cores = gp_ABTI_global->num_cores;

    if (g_affinity_type == ABTI_ES_AFFINITY_CHAMELEON) {
        int num_threads_per_socket = num_cores / 2;
        int rem = rank % 2;
        int socket_id = rank / num_threads_per_socket;
        int target = (rank - num_threads_per_socket * socket_id - rem + socket_id)
                   + num_threads_per_socket * rem;
        return g_cpusets[target % num_cores];

    } else if (g_affinity_type == ABTI_ES_AFFINITY_KNC) {
        /* NOTE: This is an experimental affinity mapping for Intel Xeon Phi
         * (KNC).  It seems that the OS kernel on KNC numbers contiguous CPU
         * IDs at a single physical core and then moves to the next physical
         * core.  This numbering causes worse performance when we use a small
         * number of physical cores.  So, we set the ES affinity in a
         * round-robin manner from the view of physical cores, not logical
         * cores. */
        const int NUM_HTHREAD = 4;
        int NUM_PHYSICAL_CORES = num_cores / NUM_HTHREAD;
        int target;
        if (rank < NUM_PHYSICAL_CORES) {
            target = NUM_HTHREAD * rank;
        } else if (rank < NUM_PHYSICAL_CORES * 2) {
            target = NUM_HTHREAD * (rank - NUM_PHYSICAL_CORES) + 1;
        } else if (rank < NUM_PHYSICAL_CORES * 3) {
            target = NUM_HTHREAD * (rank - NUM_PHYSICAL_CORES * 2) + 2;
        } else {
            target = NUM_HTHREAD * (rank - NUM_PHYSICAL_CORES * 3) + 3;
        }
        return g_cpusets[target % num_cores];

    } else {
        return g_cpusets[rank % num_cores];
    }
}
#endif

void ABTD_affinity_init(void)
{
#ifdef HAVE_PTHREAD_SETAFFINITY_NP
    int i;
    int num_cores = 0;

#if defined(__FreeBSD__)
    for (i = 0; i < CPU_SETSIZE; i++) {
        CPU_ZERO(&g_cpusets[i]);
        CPU_SET(i, &g_cpusets[i]);
    }
    num_cores = CPU_SETSIZE;
#else
    cpu_set_t cpuset;
    i = sched_getaffinity(getpid(), sizeof(cpu_set_t), &cpuset);
    ABTI_ASSERT(i == 0);

    for (i = 0; i < CPU_SETSIZE; i++) {
        CPU_ZERO(&g_cpusets[i]);
        if (CPU_ISSET(i, &cpuset)) {
            CPU_SET(i, &g_cpusets[num_cores]);
            num_cores++;
        }
    }
#endif
    gp_ABTI_global->num_cores = num_cores;

    /* affinity type */
    char *env = getenv("ABT_AFFINITY_TYPE");
    if (env == NULL) env = getenv("ABT_ENV_AFFINITY_TYPE");
    if (env != NULL) {
        if (strcmp(env, "chameleon") == 0) {
            g_affinity_type = ABTI_ES_AFFINITY_CHAMELEON;
        } else if (strcmp(env, "knc") == 0) {
            g_affinity_type = ABTI_ES_AFFINITY_KNC;
        }
    }
#else
    /* In this case, we don't support the ES affinity. */
    gp_ABTI_global->set_affinity = ABT_FALSE;
#endif
}

int ABTD_affinity_set(ABTD_xstream_context ctx, int rank)
{
#ifdef HAVE_PTHREAD_SETAFFINITY_NP
    int abt_errno;

    cpu_set_t cpuset = ABTD_affinity_get_cpuset_for_rank(rank);
    if (!pthread_setaffinity_np(ctx, sizeof(cpu_set_t), &cpuset)) {
        abt_errno = ABT_SUCCESS;
    } else {
        abt_errno = ABT_ERR_OTHER;
        goto fn_fail;
    }

#if 0
    /* For debugging and verification */
    int ret = pthread_getaffinity_np(ctx, sizeof(cpu_set_t), &cpuset);
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
    int i;
    cpu_set_t cpuset;

    CPU_ZERO(&cpuset);
    for (i = 0; i < cpuset_size; i++) {
        ABTI_ASSERT(p_cpuset[i] < CPU_SETSIZE);
        CPU_SET(p_cpuset[i], &cpuset);
    }

    i = pthread_setaffinity_np(ctx, sizeof(cpu_set_t), &cpuset);
    ABTI_CHECK_TRUE(!i, ABT_ERR_OTHER);

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
    int i;
    cpu_set_t cpuset;
    int num_cpus = 0;

    i = pthread_getaffinity_np(ctx, sizeof(cpu_set_t), &cpuset);
    ABTI_CHECK_TRUE(!i, ABT_ERR_OTHER);

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
        *p_num_cpus = ABTD_CPU_COUNT(&cpuset);
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

