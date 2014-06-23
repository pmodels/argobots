/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#ifndef ABTI_ERROR_H_INCLUDED
#define ABTI_ERROR_H_INCLUDED

#define ABTI_CHECK_ERROR(abt_errno)  \
    if (abt_errno != ABT_SUCCESS) goto fn_fail

#define ABTI_CHECK_NULL_XSTREAM_PTR(p)          \
    if (p == NULL) {                            \
        abt_errno = ABT_ERR_INV_XSTREAM;        \
        goto fn_fail;                           \
    }

#define ABTI_CHECK_NULL_SCHED_PTR(p)            \
    if (p == NULL) {                            \
        abt_errno = ABT_ERR_INV_SCHED;          \
        goto fn_fail;                           \
    }

#define ABTI_CHECK_NULL_THREAD_PTR(p)           \
    if (p == NULL) {                            \
        abt_errno = ABT_ERR_INV_THREAD;         \
        goto fn_fail;                           \
    }

#define ABTI_CHECK_NULL_THREAD_ATTR_PTR(p)      \
    if (p == NULL) {                       \
        abt_errno = ABT_ERR_INV_THREAD_ATTR;    \
        goto fn_fail;                           \
    }

#define ABTI_CHECK_NULL_TASK_PTR(p)             \
    if (p == NULL) {                            \
        abt_errno = ABT_ERR_INV_TASK;           \
        goto fn_fail;                           \
    }

#define ABTI_CHECK_NULL_MUTEX_PTR(p)            \
    if (p == NULL) {                            \
        abt_errno = ABT_ERR_INV_MUTEX;          \
        goto fn_fail;                           \
    }

#define ABTI_CHECK_NULL_FUTURE_PTR(p)           \
    if (p == NULL) {                            \
        abt_errno = ABT_ERR_INV_FUTURE;         \
        goto fn_fail;                           \
    }

#define ABTI_CHECK_SCHED_PRIO(prio)             \
    if (prio > ABT_SCHED_PRIO_HIGH) {           \
        abt_errno = ABT_ERR_INV_SCHED_PRIO;     \
        goto fn_fail;                           \
    }

#define HANDLE_ERROR(msg) \
    fprintf(stderr, "[%s:%d] %s\n", __FILE__, __LINE__, msg); exit(-1)

#define HANDLE_ERROR_WITH_CODE(msg,n) \
    fprintf(stderr, "[%s:%d] %s: %d\n", __FILE__, __LINE__, msg, n); exit(-1)


#endif /* ABTI_ERROR_H_INCLUDED */
