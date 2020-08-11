/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"
#include <unistd.h>

#ifdef HAVE_PTHREAD_SETAFFINITY_NP
#ifdef __FreeBSD__

#include <sys/param.h>
#include <sys/cpuset.h>
#include <pthread_np.h>
typedef cpuset_t cpu_set_t;

#else /* !__FreeBSD__ */

#define _GNU_SOURCE
#include <sched.h>

#endif
#endif /* HAVE_PTHREAD_SETAFFINITY_NP */

typedef struct {
    ABTD_affinity_cpuset initial_cpuset;
    size_t num_cpusets;
    ABTD_affinity_cpuset *cpusets;
} global_affinity;

static global_affinity g_affinity;

static inline int int_rem(int a, unsigned int b)
{
    /* Return x where a = n * b + x and 0 <= x < b */
    /* Because of ambiguity in the C specification, it uses a branch to check if
     * the result is positive. */
    int ret = (a % b) + b;
    return ret >= b ? (ret - b) : ret;
}

static int get_num_cores(pthread_t native_thread, int *p_num_cores)
{
#ifdef HAVE_PTHREAD_SETAFFINITY_NP
    int i, num_cores = 0;
    /* Check the number of available cores by counting set bits. */
    cpu_set_t cpuset;
    int ret = pthread_getaffinity_np(native_thread, sizeof(cpu_set_t), &cpuset);
    if (ret)
        return ABT_ERR_OTHER;
    for (i = 0; i < CPU_SETSIZE; i++) {
        if (CPU_ISSET(i, &cpuset)) {
            num_cores++;
        }
    }
    *p_num_cores = num_cores;
    return ABT_SUCCESS;
#else
    return ABT_ERR_FEATURE_NA;
#endif
}

static int read_cpuset(pthread_t native_thread, ABTD_affinity_cpuset *p_cpuset)
{
#ifdef HAVE_PTHREAD_SETAFFINITY_NP
    cpu_set_t cpuset;
    int ret = pthread_getaffinity_np(native_thread, sizeof(cpu_set_t), &cpuset);
    if (ret)
        return ABT_ERR_OTHER;
    int i, j, num_cpuids = 0;
    for (i = 0; i < CPU_SETSIZE; i++) {
        if (CPU_ISSET(i, &cpuset))
            num_cpuids++;
    }
    p_cpuset->num_cpuids = num_cpuids;
    p_cpuset->cpuids = (int *)malloc(sizeof(int) * num_cpuids);
    for (i = 0, j = 0; i < CPU_SETSIZE; i++) {
        if (CPU_ISSET(i, &cpuset))
            p_cpuset->cpuids[j++] = i;
    }
    return ABT_SUCCESS;
#else
    return ABT_ERR_FEATURE_NA;
#endif
}

static int apply_cpuset(pthread_t native_thread,
                        const ABTD_affinity_cpuset *p_cpuset)
{
#ifdef HAVE_PTHREAD_SETAFFINITY_NP
    size_t i;
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    for (i = 0; i < p_cpuset->num_cpuids; i++) {
        CPU_SET(int_rem(p_cpuset->cpuids[i], CPU_SETSIZE), &cpuset);
    }
    int ret = pthread_setaffinity_np(native_thread, sizeof(cpu_set_t), &cpuset);
    return ret == 0 ? ABT_SUCCESS : ABT_ERR_OTHER;
#else
    return ABT_ERR_FEATURE_NA;
#endif
}

void ABTD_affinity_init(const char *affinity_str)
{
    g_affinity.num_cpusets = 0;
    g_affinity.cpusets = NULL;
    g_affinity.initial_cpuset.cpuids = NULL;
    pthread_t self_native_thread = pthread_self();
    int i, ret;
    ret = get_num_cores(self_native_thread, &gp_ABTI_global->num_cores);
    if (ret != ABT_SUCCESS || gp_ABTI_global->num_cores == 0) {
        gp_ABTI_global->set_affinity = ABT_FALSE;
        return;
    }
    ret = read_cpuset(self_native_thread, &g_affinity.initial_cpuset);
    if (ret != ABT_SUCCESS) {
        gp_ABTI_global->set_affinity = ABT_FALSE;
        return;
    } else if (g_affinity.initial_cpuset.num_cpuids == 0) {
        ABTD_affinity_cpuset_destroy(&g_affinity.initial_cpuset);
        gp_ABTI_global->set_affinity = ABT_FALSE;
        return;
    }
    gp_ABTI_global->set_affinity = ABT_TRUE;
    ABTD_affinity_list *p_list = ABTD_affinity_list_create(affinity_str);
    if (p_list) {
        if (p_list->num == 0) {
            ABTD_affinity_list_free(p_list);
            p_list = NULL;
        }
    }
    if (p_list) {
        /* Create cpusets based on the affinity list.*/
        g_affinity.num_cpusets = p_list->num;
        g_affinity.cpusets =
            (ABTD_affinity_cpuset *)ABTU_calloc(g_affinity.num_cpusets,
                                                sizeof(ABTD_affinity_cpuset));
        for (i = 0; i < p_list->num; i++) {
            const ABTD_affinity_id_list *p_id_list = p_list->p_id_lists[i];
            int j, num_cpuids = 0, len_cpuids = 8;
            g_affinity.cpusets[i].cpuids =
                (int *)ABTU_malloc(sizeof(int) * len_cpuids);
            for (j = 0; j < p_id_list->num; j++) {
                int cpuid_i = int_rem(p_id_list->ids[j],
                                      g_affinity.initial_cpuset.num_cpuids);
                int cpuid = g_affinity.initial_cpuset.cpuids[cpuid_i];
                /* If it is unique, add it.*/
                int k, is_unique = 1;
                for (k = 0; k < num_cpuids; k++) {
                    if (g_affinity.cpusets[i].cpuids[k] == cpuid) {
                        is_unique = 0;
                        break;
                    }
                }
                if (is_unique) {
                    if (num_cpuids == len_cpuids) {
                        g_affinity.cpusets[i].cpuids =
                            (int *)ABTU_realloc(g_affinity.cpusets[i].cpuids,
                                                sizeof(int) * len_cpuids,
                                                sizeof(int) * len_cpuids * 2);
                        len_cpuids *= 2;
                    }
                    g_affinity.cpusets[i].cpuids[num_cpuids] = cpuid;
                    num_cpuids++;
                }
            }
            /* Adjust the size of cpuids. */
            if (num_cpuids != len_cpuids)
                g_affinity.cpusets[i].cpuids =
                    (int *)ABTU_realloc(g_affinity.cpusets[i].cpuids,
                                        sizeof(int) * len_cpuids,
                                        sizeof(int) * num_cpuids);
            g_affinity.cpusets[i].num_cpuids = num_cpuids;
        }
        ABTD_affinity_list_free(p_list);
    } else {
        /* Create default cpusets. */
        g_affinity.num_cpusets = g_affinity.initial_cpuset.num_cpuids;
        g_affinity.cpusets =
            (ABTD_affinity_cpuset *)ABTU_calloc(g_affinity.num_cpusets,
                                                sizeof(ABTD_affinity_cpuset));
        for (i = 0; i < g_affinity.num_cpusets; i++) {
            g_affinity.cpusets[i].num_cpuids = 1;
            g_affinity.cpusets[i].cpuids = (int *)ABTU_malloc(
                sizeof(int) * g_affinity.cpusets[i].num_cpuids);
            g_affinity.cpusets[i].cpuids[0] =
                g_affinity.initial_cpuset.cpuids[i];
        }
    }
}

void ABTD_affinity_finalize(void)
{
    pthread_t self_native_thread = pthread_self();
    if (gp_ABTI_global->set_affinity) {
        /* Set the affinity of the main native thread to the original one. */
        apply_cpuset(self_native_thread, &g_affinity.initial_cpuset);
    }
    /* Free g_afinity. */
    ABTD_affinity_cpuset_destroy(&g_affinity.initial_cpuset);
    int i;
    for (i = 0; i < g_affinity.num_cpusets; i++) {
        ABTD_affinity_cpuset_destroy(&g_affinity.cpusets[i]);
    }
    ABTU_free(g_affinity.cpusets);
    g_affinity.cpusets = NULL;
    g_affinity.num_cpusets = 0;
}

int ABTD_affinity_cpuset_read(ABTD_xstream_context *p_ctx,
                              ABTD_affinity_cpuset *p_cpuset)
{
    return read_cpuset(p_ctx->native_thread, p_cpuset);
}

int ABTD_affinity_cpuset_apply(ABTD_xstream_context *p_ctx,
                               const ABTD_affinity_cpuset *p_cpuset)
{
    return apply_cpuset(p_ctx->native_thread, p_cpuset);
}

int ABTD_affinity_cpuset_apply_default(ABTD_xstream_context *p_ctx, int rank)
{
    if (gp_ABTI_global->set_affinity) {
        ABTD_affinity_cpuset *p_cpuset =
            &g_affinity.cpusets[rank % g_affinity.num_cpusets];
        return apply_cpuset(p_ctx->native_thread, p_cpuset);
    }
    return ABT_SUCCESS;
}

void ABTD_affinity_cpuset_destroy(ABTD_affinity_cpuset *p_cpuset)
{
    if (p_cpuset) {
        ABTU_free(p_cpuset->cpuids);
        p_cpuset->cpuids = NULL;
    }
}
