/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#ifndef ABTI_ERROR_H_INCLUDED
#define ABTI_ERROR_H_INCLUDED

#include <assert.h>

#ifndef ABT_CONFIG_DISABLE_ERROR_CHECK
#define ABTI_ASSERT(cond) assert(cond)
#else
#define ABTI_ASSERT(cond)
#endif

#ifndef ABT_CONFIG_DISABLE_ERROR_CHECK
#define ABTI_CHECK_INITIALIZED()                \
    do {                                        \
        if (gp_ABTI_global == NULL) {           \
            abt_errno = ABT_ERR_UNINITIALIZED;  \
            goto fn_fail;                       \
        }                                       \
    } while(0)
#else
#define ABTI_CHECK_INITIALIZED()
#endif

#ifndef ABT_CONFIG_DISABLE_ERROR_CHECK
#define ABTI_CHECK_ERROR(abt_errno)             \
    if (abt_errno != ABT_SUCCESS) goto fn_fail
#else
#define ABTI_CHECK_ERROR(abt_errno)
#endif

#ifndef ABT_CONFIG_DISABLE_ERROR_CHECK
#define ABTI_CHECK_ERROR_MSG(abt_errno,msg)     \
    do {                                        \
        if (abt_errno != ABT_SUCCESS) {         \
            HANDLE_ERROR(msg);                  \
            goto fn_fail;                       \
        }                                       \
    } while(0)
#else
#define ABTI_CHECK_ERROR_MSG(abt_errno,msg)
#endif

#ifndef ABT_CONFIG_DISABLE_ERROR_CHECK
#define ABTI_CHECK_TRUE(cond,val)               \
    do {                                        \
        if (!(cond)) {                          \
            abt_errno = (val);                  \
            goto fn_fail;                       \
        }                                       \
    } while(0)
#else
#define ABTI_CHECK_TRUE(cond,val)
#endif

#ifndef ABT_CONFIG_DISABLE_ERROR_CHECK
#define ABTI_CHECK_TRUE_MSG(cond,val,msg)       \
    do {                                        \
        if (!(cond)) {                          \
            abt_errno = (val);                  \
            HANDLE_ERROR(msg);                  \
            goto fn_fail;                       \
        }                                       \
    } while(0)
#else
#define ABTI_CHECK_TRUE_MSG(cond,val,msg)
#endif

#ifndef ABT_CONFIG_DISABLE_ERROR_CHECK
#define ABTI_CHECK_NULL_XSTREAM_PTR(p)          \
    do {                                        \
        if (p == NULL) {                        \
            abt_errno = ABT_ERR_INV_XSTREAM;    \
            goto fn_fail;                       \
        }                                       \
    } while(0)
#else
#define ABTI_CHECK_NULL_XSTREAM_PTR(p)
#endif

#ifndef ABT_CONFIG_DISABLE_ERROR_CHECK
#define ABTI_CHECK_NULL_POOL_PTR(p)             \
    do {                                        \
        if (p == NULL) {                        \
            abt_errno = ABT_ERR_INV_POOL;       \
            goto fn_fail;                       \
        }                                       \
    } while(0)
#else
#define ABTI_CHECK_NULL_POOL_PTR(p)
#endif

#ifndef ABT_CONFIG_DISABLE_ERROR_CHECK
#define ABTI_CHECK_NULL_SCHED_PTR(p)            \
    do {                                        \
        if (p == NULL) {                        \
            abt_errno = ABT_ERR_INV_SCHED;      \
            goto fn_fail;                       \
        }                                       \
    } while(0)
#else
#define ABTI_CHECK_NULL_SCHED_PTR(p)
#endif

#ifndef ABT_CONFIG_DISABLE_ERROR_CHECK
#define ABTI_CHECK_NULL_THREAD_PTR(p)           \
    do {                                        \
        if (p == NULL) {                        \
            abt_errno = ABT_ERR_INV_THREAD;     \
            goto fn_fail;                       \
        }                                       \
    } while(0)
#else
#define ABTI_CHECK_NULL_THREAD_PTR(p)
#endif

#ifndef ABT_CONFIG_DISABLE_ERROR_CHECK
#define ABTI_CHECK_NULL_THREAD_ATTR_PTR(p)          \
    do {                                            \
        if (p == NULL) {                            \
            abt_errno = ABT_ERR_INV_THREAD_ATTR;    \
            goto fn_fail;                           \
        }                                           \
    } while(0)
#else
#define ABTI_CHECK_NULL_THREAD_ATTR_PTR(p)
#endif

#ifndef ABT_CONFIG_DISABLE_ERROR_CHECK
#define ABTI_CHECK_NULL_TASK_PTR(p)             \
    do {                                        \
        if (p == NULL) {                        \
            abt_errno = ABT_ERR_INV_TASK;       \
            goto fn_fail;                       \
        }                                       \
    } while(0)
#else
#define ABTI_CHECK_NULL_TASK_PTR(p)
#endif

#ifndef ABT_CONFIG_DISABLE_ERROR_CHECK
#define ABTI_CHECK_NULL_KEY_PTR(p)              \
    do {                                        \
        if (p == NULL) {                        \
            abt_errno = ABT_ERR_INV_KEY;        \
            goto fn_fail;                       \
        }                                       \
    } while(0)
#else
#define ABTI_CHECK_NULL_KEY_PTR(p)
#endif

#ifndef ABT_CONFIG_DISABLE_ERROR_CHECK
#define ABTI_CHECK_NULL_MUTEX_PTR(p)            \
    do {                                        \
        if (p == NULL) {                        \
            abt_errno = ABT_ERR_INV_MUTEX;      \
            goto fn_fail;                       \
        }                                       \
    } while(0)
#else
#define ABTI_CHECK_NULL_MUTEX_PTR(p)
#endif

#ifndef ABT_CONFIG_DISABLE_ERROR_CHECK
#define ABTI_CHECK_NULL_MUTEX_ATTR_PTR(p)       \
    do {                                        \
        if (p == NULL) {                        \
            abt_errno = ABT_ERR_INV_MUTEX_ATTR; \
            goto fn_fail;                       \
        }                                       \
    } while(0)
#else
#define ABTI_CHECK_NULL_MUTEX_ATTR_PTR(p)
#endif

#ifndef ABT_CONFIG_DISABLE_ERROR_CHECK
#define ABTI_CHECK_NULL_COND_PTR(p)             \
    do {                                        \
        if (p == NULL) {                        \
            abt_errno = ABT_ERR_INV_COND;       \
            goto fn_fail;                       \
        }                                       \
    } while(0)
#else
#define ABTI_CHECK_NULL_COND_PTR(p)
#endif

#ifndef ABT_CONFIG_DISABLE_ERROR_CHECK
#define ABTI_CHECK_NULL_RWLOCK_PTR(p)             \
    do {                                        \
        if (p == NULL) {                        \
            abt_errno = ABT_ERR_INV_RWLOCK;       \
            goto fn_fail;                       \
        }                                       \
    } while(0)
#else
#define ABTI_CHECK_NULL_RWLOCK_PTR(p) \
    do {                              \
        if (0) {                      \
            goto fn_fail;             \
        }                             \
    } while(0)
#endif

#ifndef ABT_CONFIG_DISABLE_ERROR_CHECK
#define ABTI_CHECK_NULL_FUTURE_PTR(p)           \
    do {                                        \
        if (p == NULL) {                        \
            abt_errno = ABT_ERR_INV_FUTURE;     \
            goto fn_fail;                       \
        }                                       \
    } while(0)
#else
#define ABTI_CHECK_NULL_FUTURE_PTR(p)
#endif

#ifndef ABT_CONFIG_DISABLE_ERROR_CHECK
#define ABTI_CHECK_NULL_EVENTUAL_PTR(p)         \
    do {                                        \
        if (p == NULL) {                        \
            abt_errno = ABT_ERR_INV_EVENTUAL;   \
            goto fn_fail;                       \
        }                                       \
    } while(0)
#else
#define ABTI_CHECK_NULL_EVENTUAL_PTR(p)
#endif

#ifndef ABT_CONFIG_DISABLE_ERROR_CHECK
#define ABTI_CHECK_NULL_BARRIER_PTR(p)          \
    do {                                        \
        if (p == NULL) {                        \
            abt_errno = ABT_ERR_INV_BARRIER;    \
            goto fn_fail;                       \
        }                                       \
    } while (0)
#else
#define ABTI_CHECK_NULL_BARRIER_PTR(p)
#endif

#ifndef ABT_CONFIG_DISABLE_ERROR_CHECK
#define ABTI_CHECK_NULL_TIMER_PTR(p)            \
    do {                                        \
        if (p == NULL) {                        \
            abt_errno = ABT_ERR_INV_TIMER;      \
            goto fn_fail;                       \
        }                                       \
    } while(0)
#else
#define ABTI_CHECK_NULL_TIMER_PTR(p)
#endif

#define HANDLE_ERROR(msg) \
    fprintf(stderr, "[%s:%d] %s\n", __FILE__, __LINE__, msg)
    //fprintf(stderr, "[%s:%d] %s\n", __FILE__, __LINE__, msg); exit(-1)

#define HANDLE_ERROR_WITH_CODE(msg,n) \
    fprintf(stderr, "[%s:%d] %s: %d\n", __FILE__, __LINE__, msg, n)
    //fprintf(stderr, "[%s:%d] %s: %d\n", __FILE__, __LINE__, msg, n); exit(-1)

#define HANDLE_ERROR_FUNC_WITH_CODE(n) \
    fprintf(stderr, "[%s:%d] %s: %d\n", __FILE__, __LINE__, __func__, n)
    //fprintf(stderr, "[%s:%d] %s: %d\n", __FILE__, __LINE__, __func__, n); exit(-1)

#endif /* ABTI_ERROR_H_INCLUDED */
