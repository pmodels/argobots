/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#ifndef ABTD_H_INCLUDED
#define ABTD_H_INCLUDED

#define __USE_GNU
#include <pthread.h>
#include "abtd_ucontext.h"

/* Data Types */
typedef pthread_t           ABTD_xstream_context;
typedef pthread_mutex_t     ABTD_xstream_mutex;
typedef abt_ucontext_t      ABTD_thread_context;

/* ES Storage Qualifier */
#define ABTD_XSTREAM_LOCAL  __thread

/* Environment */
void ABTD_env_init(ABTI_global *p_global);

/* ES Context */
int ABTD_xstream_context_create(void *(*f_xstream)(void *), void *p_arg,
                                ABTD_xstream_context *p_ctx);
int ABTD_xstream_context_free(ABTD_xstream_context *p_ctx);
int ABTD_xstream_context_join(ABTD_xstream_context ctx);
int ABTD_xstream_context_exit(void);
int ABTD_xstream_context_self(ABTD_xstream_context *p_ctx);

/* ULT Context */
int ABTD_thread_context_create(ABTD_thread_context *p_link,
                               void (*f_thread)(void *), void *p_arg,
                               size_t stacksize, void *p_stack,
                               ABTD_thread_context *p_newctx);
int ABTD_thread_context_free(ABTD_thread_context *p_ctx);
int ABTD_thread_context_switch(ABTD_thread_context *p_old,
                               ABTD_thread_context *p_new);
int ABTD_thread_context_change_link(ABTD_thread_context *p_ctx,
                                    ABTD_thread_context *p_link);

/* Atomic Functions */
#include "abtd_atomic.h"

#endif /* ABTD_H_INCLUDED */
