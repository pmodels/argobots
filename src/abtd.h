/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#ifndef ABTD_H_INCLUDED
#define ABTD_H_INCLUDED

/* TODO: Generalize for diverse architectures
 * Currently, ARGOBOTS is implemented using pthread and ucontext.
 * This file defines types, macros and functions that hide architecture-
 * dependent code, e.g., pthread and ucontext.
 */

#define __USE_GNU
#include <pthread.h>

#define _XOPEN_SOURCE
#include <ucontext.h>

/* Data Types */
typedef pthread_t           ABTD_Stream_context;
typedef pthread_mutex_t     ABTD_Stream_mutex;
typedef ucontext_t          ABTD_Thread_context;

/* ES Storage Qualifier */
#define ABTD_STREAM_LOCAL   __thread

/* ES Context */
int ABTD_Stream_context_create(void *(*f_stream)(void *), void *p_arg,
                               ABTD_Stream_context *p_ctx);
int ABTD_Stream_context_free(ABTD_Stream_context *p_ctx);
int ABTD_Stream_context_join(ABTD_Stream_context ctx);
int ABTD_Stream_context_exit();
int ABTD_Stream_context_self(ABTD_Stream_context *p_ctx);

/* ULT Context */
int ABTD_Thread_context_create(ABTD_Thread_context *p_link,
                               void (*f_thread)(void *), void *p_arg,
                               size_t stacksize, void *p_stack,
                               ABTD_Thread_context *p_newctx);
int ABTD_Thread_context_free(ABTD_Thread_context *p_ctx);
int ABTD_Thread_context_switch(ABTD_Thread_context *p_old,
                               ABTD_Thread_context *p_new);
int ABTD_Thread_context_change_link(ABTD_Thread_context *p_ctx,
                                    ABTD_Thread_context *p_link);

/* Atomic Functions */
#include "abtd_atomic.h"

#endif /* ABTD_H_INCLUDED */
