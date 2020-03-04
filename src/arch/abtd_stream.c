/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"

static void *ABTDI_xstream_context_thread_func(void *arg)
{
    ABTD_xstream_context *p_ctx = (ABTD_xstream_context *)arg;
    void *(*thread_f)(void *) = p_ctx->thread_f;
    void *p_arg = p_ctx->p_arg;
    thread_f(p_arg);
    return NULL;
}

int ABTD_xstream_context_create(void *(*f_xstream)(void *), void *p_arg,
                                ABTD_xstream_context *p_ctx)
{
    int abt_errno = ABT_SUCCESS;
    p_ctx->thread_f = f_xstream;
    p_ctx->p_arg = p_arg;
    int ret = pthread_create(&p_ctx->native_thread, NULL,
                             ABTDI_xstream_context_thread_func, p_ctx);
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

int ABTD_xstream_context_join(ABTD_xstream_context *p_ctx)
{
    int abt_errno = ABT_SUCCESS;
    int ret = pthread_join(p_ctx->native_thread, NULL);
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
    p_ctx->native_thread = pthread_self();
    return abt_errno;
}

