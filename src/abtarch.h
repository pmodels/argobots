/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#ifndef ABTARCH_H_INCLUDED
#define ABTARCH_H_INCLUDED

/* TODO: Generalize for diverse architectures
 * Currently, ARGOBOTS is implemented using pthread and ucontext.
 * This file defines types, macros and functions that hide architecture-
 * dependent code, e.g., pthread and ucontext.
 */

#define __USE_GNU
#include <pthread.h>

#define _XOPEN_SOURCE
#include <ucontext.h>

typedef pthread_t       ABTA_ES_t;
typedef pthread_mutex_t ABTA_ES_lock_t;
typedef ucontext_t      ABTA_ULT_t;

#define ABTA_ES_SUCCESS             0
#define ABTA_ES_ERR_OTHER           1
#define ABTA_ES_create(a,b,c,d)     pthread_create(a,b,c,d)
#define ABTA_ES_join(a,b)           pthread_join(a,b)
#define ABTA_ES_exit(a)             pthread_exit(a)
#define ABTA_ES_self()              pthread_self()
#define ABTA_ES_yield()             pthread_yield()
#define ABTA_ES_lock_create(a,b)    pthread_mutex_init(a,b)
#define ABTA_ES_lock_free(a)        pthread_mutex_destroy(a)
#define ABTA_ES_lock(a)             pthread_mutex_lock(a)
#define ABTA_ES_unlock(a)           pthread_mutex_unlock(a)

#define ABTA_ULT_SUCCESS            0
#define ABTA_ULT_ERR_OTHER          1
#define ABTA_ULT_swap(a,b)          swapcontext(a,b)
int ABTA_ULT_make(ABTD_Stream *stream_ptr, ABTD_Thread *thread_ptr,
                  void (*thread_func)(void *), void *arg);

#endif /* ABTARCH_H_INCLUDED */
