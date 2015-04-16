/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"

int ABTD_xstream_context_create(void *(*f_xstream)(void *), void *p_arg,
                                ABTD_xstream_context *p_ctx)
{
    int abt_errno = ABT_SUCCESS;
    int ret = pthread_create(p_ctx, NULL, f_xstream, p_arg);
    if (ret != 0) {
        HANDLE_ERROR("pthread_create");
        abt_errno = ABT_ERR_XSTREAM;
    }
    return abt_errno;
}

int ABTD_xstream_context_free(ABTD_xstream_context *p_ctx)
{
    ABTI_UNUSED(p_ctx);
    int abt_errno = ABT_SUCCESS;
    /* Currently, nothing to do */
    return abt_errno;
}

int ABTD_xstream_context_join(ABTD_xstream_context ctx)
{
    int abt_errno = ABT_SUCCESS;
    int ret = pthread_join(ctx, NULL);
    if (ret != 0) {
        HANDLE_ERROR("pthread_join");
        abt_errno = ABT_ERR_XSTREAM;
    }
    return abt_errno;
}

int ABTD_xstream_context_exit(void)
{
    pthread_exit(NULL);
    return ABT_SUCCESS;
}

int ABTD_xstream_context_self(ABTD_xstream_context *p_ctx)
{
    int abt_errno = ABT_SUCCESS;
    *p_ctx = pthread_self();
    return abt_errno;
}

#ifdef HAVE_PTHREAD_SETAFFINITY_NP
cpu_set_t ABTD_env_get_cpuset(int rank);

int ABTD_xstream_context_set_affinity(ABTD_xstream_context ctx, int rank)
{
    int abt_errno = ABT_SUCCESS;

    cpu_set_t cpuset = ABTD_env_get_cpuset(rank);
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
}
#else
int ABTD_xstream_context_set_affinity(ABTD_xstream_context ctx, int rank)
{
    return ABT_SUCCESS;
}
#endif
