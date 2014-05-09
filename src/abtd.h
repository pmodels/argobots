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

typedef pthread_t       ABTD_ES;
typedef pthread_mutex_t ABTD_ES_lock;
typedef ucontext_t      ABTD_ULT;

#define ABTD_ES_SUCCESS             0
#define ABTD_ES_ERR_OTHER           1
#define ABTD_ES_create(a,b,c,d)     pthread_create(a,b,c,d)
#define ABTD_ES_join(a,b)           pthread_join(a,b)
#define ABTD_ES_exit(a)             pthread_exit(a)
#define ABTD_ES_self()              pthread_self()
#define ABTD_ES_yield()             pthread_yield()
#define ABTD_ES_lock_create(a,b)    pthread_mutex_init(a,b)
#define ABTD_ES_lock_free(a)        pthread_mutex_destroy(a)
#define ABTD_ES_lock(a)             pthread_mutex_lock(a)
#define ABTD_ES_unlock(a)           pthread_mutex_unlock(a)

#define ABTD_ULT_SUCCESS            0
#define ABTD_ULT_ERR_OTHER          1
#define ABTD_ULT_swap(a,b)          swapcontext(a,b)
int ABTD_ULT_make(ABTI_Stream *p_stream, ABTI_Thread *p_thread,
                  void (*thread_func)(void *), void *p_arg);

#endif /* ABTD_H_INCLUDED */
