/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#ifndef ABTI_ERROR_H_INCLUDED
#define ABTI_ERROR_H_INCLUDED

#include <assert.h>
#include <abt_config.h>


#ifndef ABT_CONFIG_DISABLE_ERROR_CHECK
#define ABTI_IS_ERROR_CHECK_ENABLED 1
#else
#define ABTI_IS_ERROR_CHECK_ENABLED 0
#endif

#define ABTI_ASSERT(cond)                  \
    do {                                   \
        if (ABTI_IS_ERROR_CHECK_ENABLED) { \
            assert(cond);                  \
        }                                  \
    } while(0)

#define ABTI_CHECK_INITIALIZED()                                     \
    do {                                                             \
        if (ABTI_IS_ERROR_CHECK_ENABLED && gp_ABTI_global == NULL) { \
            abt_errno = ABT_ERR_UNINITIALIZED;                       \
            goto fn_fail;                                            \
        }                                                            \
    } while(0)

#define ABTI_CHECK_ERROR(abt_errno)                                    \
    do {                                                               \
        if (ABTI_IS_ERROR_CHECK_ENABLED && abt_errno != ABT_SUCCESS) { \
            goto fn_fail;                                              \
        }                                                              \
    } while(0)

#define ABTI_CHECK_ERROR_MSG(abt_errno, msg)                           \
    do {                                                               \
        if (ABTI_IS_ERROR_CHECK_ENABLED && abt_errno != ABT_SUCCESS) { \
            HANDLE_ERROR(msg);                                         \
            goto fn_fail;                                              \
        }                                                              \
    } while(0)

#define ABTI_CHECK_TRUE(cond, val)                                     \
    do {                                                               \
        if (ABTI_IS_ERROR_CHECK_ENABLED && !(cond)) {                  \
            abt_errno = (val);                                         \
            goto fn_fail;                                              \
        }                                                              \
    } while(0)

#define ABTI_CHECK_TRUE_RET(cond, val)                                 \
    do {                                                               \
        if (ABTI_IS_ERROR_CHECK_ENABLED && !(cond)) {                  \
            return (val);                                              \
        }                                                              \
    } while(0)

#define ABTI_CHECK_TRUE_MSG(cond, val, msg)           \
    do {                                              \
        if (ABTI_IS_ERROR_CHECK_ENABLED && !(cond)) { \
            abt_errno = (val);                        \
            HANDLE_ERROR(msg);                        \
            goto fn_fail;                             \
        }                                             \
    } while(0)

#define ABTI_CHECK_TRUE_MSG_RET(cond,val,msg)         \
    do {                                              \
        if (ABTI_IS_ERROR_CHECK_ENABLED && !(cond)) { \
            HANDLE_ERROR(msg);                        \
            return (val);                             \
        }                                             \
    } while(0)

#define ABTI_CHECK_NULL_XSTREAM_PTR(p)                  \
    do {                                                \
        if (ABTI_IS_ERROR_CHECK_ENABLED && p == NULL) { \
            abt_errno = ABT_ERR_INV_XSTREAM;            \
            goto fn_fail;                               \
        }                                               \
    } while(0)

#define ABTI_CHECK_NULL_POOL_PTR(p)                     \
    do {                                                \
        if (ABTI_IS_ERROR_CHECK_ENABLED && p == NULL) { \
            abt_errno = ABT_ERR_INV_POOL;               \
            goto fn_fail;                               \
        }                                               \
    } while(0)

#define ABTI_CHECK_NULL_SCHED_PTR(p)                    \
    do {                                                \
        if (ABTI_IS_ERROR_CHECK_ENABLED && p == NULL) { \
            abt_errno = ABT_ERR_INV_SCHED;              \
            goto fn_fail;                               \
        }                                               \
    } while(0)

#define ABTI_CHECK_NULL_THREAD_PTR(p)                   \
    do {                                                \
        if (ABTI_IS_ERROR_CHECK_ENABLED && p == NULL) { \
            abt_errno = ABT_ERR_INV_THREAD;             \
            goto fn_fail;                               \
        }                                               \
    } while(0)

#define ABTI_CHECK_NULL_THREAD_ATTR_PTR(p)              \
    do {                                                \
        if (ABTI_IS_ERROR_CHECK_ENABLED && p == NULL) { \
            abt_errno = ABT_ERR_INV_THREAD_ATTR;        \
            goto fn_fail;                               \
        }                                               \
    } while(0)

#define ABTI_CHECK_NULL_TASK_PTR(p)                     \
    do {                                                \
        if (ABTI_IS_ERROR_CHECK_ENABLED && p == NULL) { \
            abt_errno = ABT_ERR_INV_TASK;               \
            goto fn_fail;                               \
        }                                               \
    } while(0)

#define ABTI_CHECK_NULL_KEY_PTR(p)                      \
    do {                                                \
        if (ABTI_IS_ERROR_CHECK_ENABLED && p == NULL) { \
            abt_errno = ABT_ERR_INV_KEY;                \
            goto fn_fail;                               \
        }                                               \
    } while(0)

#define ABTI_CHECK_NULL_MUTEX_PTR(p)                    \
    do {                                                \
        if (ABTI_IS_ERROR_CHECK_ENABLED && p == NULL) { \
            abt_errno = ABT_ERR_INV_MUTEX;              \
            goto fn_fail;                               \
        }                                               \
    } while(0)

#define ABTI_CHECK_NULL_MUTEX_ATTR_PTR(p)               \
    do {                                                \
        if (ABTI_IS_ERROR_CHECK_ENABLED && p == NULL) { \
            abt_errno = ABT_ERR_INV_MUTEX_ATTR;         \
            goto fn_fail;                               \
        }                                               \
    } while(0)

#define ABTI_CHECK_NULL_COND_PTR(p)                     \
    do {                                                \
        if (ABTI_IS_ERROR_CHECK_ENABLED && p == NULL) { \
            abt_errno = ABT_ERR_INV_COND;               \
            goto fn_fail;                               \
        }                                               \
    } while(0)

#define ABTI_CHECK_NULL_RWLOCK_PTR(p)                   \
    do {                                                \
        if (ABTI_IS_ERROR_CHECK_ENABLED && p == NULL) { \
            abt_errno = ABT_ERR_INV_RWLOCK;             \
            goto fn_fail;                               \
        }                                               \
    } while(0)

#define ABTI_CHECK_NULL_FUTURE_PTR(p)                   \
    do {                                                \
        if (ABTI_IS_ERROR_CHECK_ENABLED && p == NULL) { \
            abt_errno = ABT_ERR_INV_FUTURE;             \
            goto fn_fail;                               \
        }                                               \
    } while(0)

#define ABTI_CHECK_NULL_EVENTUAL_PTR(p)                 \
    do {                                                \
        if (ABTI_IS_ERROR_CHECK_ENABLED && p == NULL) { \
            abt_errno = ABT_ERR_INV_EVENTUAL;           \
            goto fn_fail;                               \
        }                                               \
    } while(0)

#define ABTI_CHECK_NULL_BARRIER_PTR(p)                  \
    do {                                                \
        if (ABTI_IS_ERROR_CHECK_ENABLED && p == NULL) { \
            abt_errno = ABT_ERR_INV_BARRIER;            \
            goto fn_fail;                               \
        }                                               \
    } while (0)

#define ABTI_CHECK_NULL_TIMER_PTR(p)                    \
    do {                                                \
        if (ABTI_IS_ERROR_CHECK_ENABLED && p == NULL) { \
            abt_errno = ABT_ERR_INV_TIMER;              \
            goto fn_fail;                               \
        }                                               \
    } while(0)

#ifdef ABT_CONFIG_PRINT_ABT_ERRNO
#define HANDLE_WARNING(msg) \
    fprintf(stderr, "[%s:%d] %s\n", __FILE__, __LINE__, msg)

#define HANDLE_ERROR(msg) \
    fprintf(stderr, "[%s:%d] %s\n", __FILE__, __LINE__, msg)
    //fprintf(stderr, "[%s:%d] %s\n", __FILE__, __LINE__, msg); exit(-1)

#define HANDLE_ERROR_WITH_CODE(msg,n) \
    fprintf(stderr, "[%s:%d] %s: %d\n", __FILE__, __LINE__, msg, n)
    //fprintf(stderr, "[%s:%d] %s: %d\n", __FILE__, __LINE__, msg, n); exit(-1)

#define HANDLE_ERROR_FUNC_WITH_CODE(n) \
    fprintf(stderr, "[%s:%d] %s: %d\n", __FILE__, __LINE__, __func__, n)
    //fprintf(stderr, "[%s:%d] %s: %d\n", __FILE__, __LINE__, __func__, n); exit(-1)

#else

#define HANDLE_WARNING(msg)            do { } while (0)
#define HANDLE_ERROR(msg)              do { } while (0)
#define HANDLE_ERROR_WITH_CODE(msg,n)  do { } while (0)
#define HANDLE_ERROR_FUNC_WITH_CODE(n) do { } while (0)

#endif

#endif /* ABTI_ERROR_H_INCLUDED */
