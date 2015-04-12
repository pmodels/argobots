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
int ABTD_xstream_context_set_affinity(ABTD_xstream_context ctx, int rank);

/* ULT Context */
#include "abtd_thread.h"

/* Atomic Functions */
#include "abtd_atomic.h"

#if defined(HAVE_CLOCK_GETTIME)
#include <time.h>
typedef struct timespec ABTD_time;

#elif defined(HAVE_MACH_ABSOLUTE_TIME)
#include <mach/mach_time.h>
typedef uint64_t ABTD_time;

#elif defined(HAVE_GETTIMEOFDAY)
#include <sys/time.h>
typedef struct timeval ABTD_time;

#endif

void   ABTD_time_init(void);
int    ABTD_time_get(ABTD_time *p_time);
double ABTD_time_read_sec(ABTD_time *p_time);

#endif /* ABTD_H_INCLUDED */
