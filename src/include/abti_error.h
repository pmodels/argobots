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

#define ABTI_ASSERT(cond)                                                      \
    do {                                                                       \
        if (ABTI_IS_ERROR_CHECK_ENABLED) {                                     \
            assert(cond);                                                      \
        }                                                                      \
    } while (0)

#define ABTI_STATIC_ASSERT(cond)                                               \
    do {                                                                       \
        ((void)sizeof(char[2 * !!(cond)-1]));                                  \
    } while (0)

#define ABTI_CHECK_INITIALIZED()                                               \
    do {                                                                       \
        if (ABTI_IS_ERROR_CHECK_ENABLED && gp_ABTI_global == NULL) {           \
            abt_errno = ABT_ERR_UNINITIALIZED;                                 \
            goto fn_fail;                                                      \
        }                                                                      \
    } while (0)

#define ABTI_CHECK_ERROR(abt_errno)                                            \
    do {                                                                       \
        if (ABTI_IS_ERROR_CHECK_ENABLED && abt_errno != ABT_SUCCESS) {         \
            goto fn_fail;                                                      \
        }                                                                      \
    } while (0)

#define ABTI_CHECK_ERROR_MSG(abt_errno, msg)                                   \
    do {                                                                       \
        if (ABTI_IS_ERROR_CHECK_ENABLED && abt_errno != ABT_SUCCESS) {         \
            HANDLE_ERROR(msg);                                                 \
            goto fn_fail;                                                      \
        }                                                                      \
    } while (0)

#define ABTI_CHECK_TRUE(cond, val)                                             \
    do {                                                                       \
        if (ABTI_IS_ERROR_CHECK_ENABLED && !(cond)) {                          \
            abt_errno = (val);                                                 \
            goto fn_fail;                                                      \
        }                                                                      \
    } while (0)

#define ABTI_CHECK_TRUE_RET(cond, val)                                         \
    do {                                                                       \
        if (ABTI_IS_ERROR_CHECK_ENABLED && !(cond)) {                          \
            return (val);                                                      \
        }                                                                      \
    } while (0)

#define ABTI_CHECK_TRUE_MSG(cond, val, msg)                                    \
    do {                                                                       \
        if (ABTI_IS_ERROR_CHECK_ENABLED && !(cond)) {                          \
            abt_errno = (val);                                                 \
            HANDLE_ERROR(msg);                                                 \
            goto fn_fail;                                                      \
        }                                                                      \
    } while (0)

#define ABTI_CHECK_TRUE_MSG_RET(cond, val, msg)                                \
    do {                                                                       \
        if (ABTI_IS_ERROR_CHECK_ENABLED && !(cond)) {                          \
            HANDLE_ERROR(msg);                                                 \
            return (val);                                                      \
        }                                                                      \
    } while (0)

#define ABTI_CHECK_NULL_XSTREAM_PTR(p)                                         \
    do {                                                                       \
        if (ABTI_IS_ERROR_CHECK_ENABLED && p == (ABTI_xstream *)NULL) {        \
            abt_errno = ABT_ERR_INV_XSTREAM;                                   \
            goto fn_fail;                                                      \
        }                                                                      \
    } while (0)

#define ABTI_CHECK_NULL_POOL_PTR(p)                                            \
    do {                                                                       \
        if (ABTI_IS_ERROR_CHECK_ENABLED && p == (ABTI_pool *)NULL) {           \
            abt_errno = ABT_ERR_INV_POOL;                                      \
            goto fn_fail;                                                      \
        }                                                                      \
    } while (0)

#define ABTI_CHECK_NULL_SCHED_PTR(p)                                           \
    do {                                                                       \
        if (ABTI_IS_ERROR_CHECK_ENABLED && p == (ABTI_sched *)NULL) {          \
            abt_errno = ABT_ERR_INV_SCHED;                                     \
            goto fn_fail;                                                      \
        }                                                                      \
    } while (0)

#define ABTI_CHECK_NULL_THREAD_PTR(p)                                          \
    do {                                                                       \
        if (ABTI_IS_ERROR_CHECK_ENABLED && p == (ABTI_thread *)NULL) {         \
            abt_errno = ABT_ERR_INV_THREAD;                                    \
            goto fn_fail;                                                      \
        }                                                                      \
    } while (0)

#define ABTI_CHECK_NULL_THREAD_ATTR_PTR(p)                                     \
    do {                                                                       \
        if (ABTI_IS_ERROR_CHECK_ENABLED && p == (ABTI_thread_attr *)NULL) {    \
            abt_errno = ABT_ERR_INV_THREAD_ATTR;                               \
            goto fn_fail;                                                      \
        }                                                                      \
    } while (0)

#define ABTI_CHECK_NULL_TASK_PTR(p)                                            \
    do {                                                                       \
        if (ABTI_IS_ERROR_CHECK_ENABLED && p == (ABTI_thread *)NULL) {         \
            abt_errno = ABT_ERR_INV_TASK;                                      \
            goto fn_fail;                                                      \
        }                                                                      \
    } while (0)

#define ABTI_CHECK_NULL_KEY_PTR(p)                                             \
    do {                                                                       \
        if (ABTI_IS_ERROR_CHECK_ENABLED && p == (ABTI_key *)NULL) {            \
            abt_errno = ABT_ERR_INV_KEY;                                       \
            goto fn_fail;                                                      \
        }                                                                      \
    } while (0)

#define ABTI_CHECK_NULL_MUTEX_PTR(p)                                           \
    do {                                                                       \
        if (ABTI_IS_ERROR_CHECK_ENABLED && p == (ABTI_mutex *)NULL) {          \
            abt_errno = ABT_ERR_INV_MUTEX;                                     \
            goto fn_fail;                                                      \
        }                                                                      \
    } while (0)

#define ABTI_CHECK_NULL_MUTEX_ATTR_PTR(p)                                      \
    do {                                                                       \
        if (ABTI_IS_ERROR_CHECK_ENABLED && p == (ABTI_mutex_attr *)NULL) {     \
            abt_errno = ABT_ERR_INV_MUTEX_ATTR;                                \
            goto fn_fail;                                                      \
        }                                                                      \
    } while (0)

#define ABTI_CHECK_NULL_COND_PTR(p)                                            \
    do {                                                                       \
        if (ABTI_IS_ERROR_CHECK_ENABLED && p == (ABTI_cond *)NULL) {           \
            abt_errno = ABT_ERR_INV_COND;                                      \
            goto fn_fail;                                                      \
        }                                                                      \
    } while (0)

#define ABTI_CHECK_NULL_RWLOCK_PTR(p)                                          \
    do {                                                                       \
        if (ABTI_IS_ERROR_CHECK_ENABLED && p == (ABTI_rwlock *)NULL) {         \
            abt_errno = ABT_ERR_INV_RWLOCK;                                    \
            goto fn_fail;                                                      \
        }                                                                      \
    } while (0)

#define ABTI_CHECK_NULL_FUTURE_PTR(p)                                          \
    do {                                                                       \
        if (ABTI_IS_ERROR_CHECK_ENABLED && p == (ABTI_future *)NULL) {         \
            abt_errno = ABT_ERR_INV_FUTURE;                                    \
            goto fn_fail;                                                      \
        }                                                                      \
    } while (0)

#define ABTI_CHECK_NULL_EVENTUAL_PTR(p)                                        \
    do {                                                                       \
        if (ABTI_IS_ERROR_CHECK_ENABLED && p == (ABTI_eventual *)NULL) {       \
            abt_errno = ABT_ERR_INV_EVENTUAL;                                  \
            goto fn_fail;                                                      \
        }                                                                      \
    } while (0)

#define ABTI_CHECK_NULL_BARRIER_PTR(p)                                         \
    do {                                                                       \
        if (ABTI_IS_ERROR_CHECK_ENABLED && p == (ABTI_barrier *)NULL) {        \
            abt_errno = ABT_ERR_INV_BARRIER;                                   \
            goto fn_fail;                                                      \
        }                                                                      \
    } while (0)

#define ABTI_CHECK_NULL_XSTREAM_BARRIER_PTR(p)                                 \
    do {                                                                       \
        if (ABTI_IS_ERROR_CHECK_ENABLED &&                                     \
            p == (ABTI_xstream_barrier *)NULL) {                               \
            abt_errno = ABT_ERR_INV_BARRIER;                                   \
            goto fn_fail;                                                      \
        }                                                                      \
    } while (0)

#define ABTI_CHECK_NULL_TIMER_PTR(p)                                           \
    do {                                                                       \
        if (ABTI_IS_ERROR_CHECK_ENABLED && p == (ABTI_timer *)NULL) {          \
            abt_errno = ABT_ERR_INV_TIMER;                                     \
            goto fn_fail;                                                      \
        }                                                                      \
    } while (0)

#define ABTI_CHECK_NULL_TOOL_CONTEXT_PTR(p)                                    \
    do {                                                                       \
        if (ABTI_IS_ERROR_CHECK_ENABLED && p == (ABTI_tool_context *)NULL) {   \
            abt_errno = ABT_ERR_INV_TOOL_CONTEXT;                              \
            goto fn_fail;                                                      \
        }                                                                      \
    } while (0)

#ifdef ABT_CONFIG_PRINT_ABT_ERRNO
#define ABTI_IS_PRINT_ABT_ERRNO_ENABLED 1
#else
#define ABTI_IS_PRINT_ABT_ERRNO_ENABLED 0
#endif

#define HANDLE_WARNING(msg)                                                    \
    do {                                                                       \
        if (ABTI_IS_PRINT_ABT_ERRNO_ENABLED) {                                 \
            fprintf(stderr, "[%s:%d] %s\n", __FILE__, __LINE__, msg);          \
        }                                                                      \
    } while (0)

#define HANDLE_ERROR(msg)                                                      \
    do {                                                                       \
        if (ABTI_IS_PRINT_ABT_ERRNO_ENABLED) {                                 \
            fprintf(stderr, "[%s:%d] %s\n", __FILE__, __LINE__, msg);          \
        }                                                                      \
    } while (0)

#define HANDLE_ERROR_WITH_CODE(msg, n)                                         \
    do {                                                                       \
        if (ABTI_IS_PRINT_ABT_ERRNO_ENABLED) {                                 \
            fprintf(stderr, "[%s:%d] %s: %d\n", __FILE__, __LINE__, msg, n);   \
        }                                                                      \
    } while (0)

#define HANDLE_ERROR_FUNC_WITH_CODE(n)                                         \
    do {                                                                       \
        if (ABTI_IS_PRINT_ABT_ERRNO_ENABLED) {                                 \
            fprintf(stderr, "[%s:%d] %s: %d\n", __FILE__, __LINE__, __func__,  \
                    n);                                                        \
        }                                                                      \
    } while (0)

#endif /* ABTI_ERROR_H_INCLUDED */
