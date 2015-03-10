/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#ifndef ABTI_HANDLE_H_INCLUDED
#define ABTI_HANDLE_H_INCLUDED

/* Execution Stream (ES) */
static inline
ABTI_xstream *ABTI_xstream_get_ptr(ABT_xstream xstream)
{
#ifndef UNSAFE_MODE
    ABTI_xstream *p_xstream;
    if (xstream == ABT_XSTREAM_NULL) {
        p_xstream = NULL;
    } else {
        p_xstream = (ABTI_xstream *)xstream;
    }
    return p_xstream;
#else
    return (ABTI_xstream *)xstream;
#endif
}

static inline
ABT_xstream ABTI_xstream_get_handle(ABTI_xstream *p_xstream)
{
#ifndef UNSAFE_MODE
    ABT_xstream h_xstream;
    if (p_xstream == NULL) {
        h_xstream = ABT_XSTREAM_NULL;
    } else {
        h_xstream = (ABT_xstream)p_xstream;
    }
    return h_xstream;
#else
    return (ABT_xstream)p_xstream;
#endif
}

/* Scheduler */
static inline
ABTI_sched *ABTI_sched_get_ptr(ABT_sched sched)
{
#ifndef UNSAFE_MODE
    ABTI_sched *p_sched;
    if (sched == ABT_SCHED_NULL) {
        p_sched = NULL;
    } else {
        p_sched = (ABTI_sched *)sched;
    }
    return p_sched;
#else
    return (ABTI_sched *)sched;
#endif
}

static inline
ABT_sched ABTI_sched_get_handle(ABTI_sched *p_sched)
{
#ifndef UNSAFE_MODE
    ABT_sched h_sched;
    if (p_sched == NULL) {
        h_sched = ABT_SCHED_NULL;
    } else {
        h_sched = (ABT_sched)p_sched;
    }
    return h_sched;
#else
    return (ABT_sched)p_sched;
#endif
}

/* Pool */
static inline
ABTI_pool *ABTI_pool_get_ptr(ABT_pool pool)
{
#ifndef UNSAFE_MODE
    ABTI_pool *p_pool;
    if (pool == ABT_POOL_NULL) {
        p_pool = NULL;
    } else {
        p_pool = (ABTI_pool *)pool;
    }
    return p_pool;
#else
    return (ABTI_pool *)pool;
#endif
}

static inline
ABT_pool ABTI_pool_get_handle(ABTI_pool *p_pool)
{
#ifndef UNSAFE_MODE
    ABT_pool h_pool;
    if (p_pool == NULL) {
        h_pool = ABT_POOL_NULL;
    } else {
        h_pool = (ABT_pool)p_pool;
    }
    return h_pool;
#else
    return (ABT_pool)p_pool;
#endif
}

/* User-level Thread (ULT) */
static inline
ABTI_thread *ABTI_thread_get_ptr(ABT_thread thread)
{
#ifndef UNSAFE_MODE
    ABTI_thread *p_thread;
    if (thread == ABT_THREAD_NULL) {
        p_thread = NULL;
    } else {
        p_thread = (ABTI_thread *)thread;
    }
    return p_thread;
#else
    return (ABTI_thread *)thread;
#endif
}

static inline
ABT_thread ABTI_thread_get_handle(ABTI_thread *p_thread)
{
#ifndef UNSAFE_MODE
    ABT_thread h_thread;
    if (p_thread == NULL) {
        h_thread = ABT_THREAD_NULL;
    } else {
        h_thread = (ABT_thread)p_thread;
    }
    return h_thread;
#else
    return (ABT_thread)p_thread;
#endif
}

/* ULT Attributes */
/**
 * @ingroup ULT_ATTR_PRIVATE
 * @brief   Get \c ABTI_thread_attr pointer from \c ABT_thread_attr handle.
 *
 * \c ABTI_thread_attr_get_ptr() returns \c ABTI_thread_attr pointer
 * corresponding to \c ABT_thread_attr handle \c attr. If \c attr is
 * \c ABT_THREAD_NULL, \c NULL is returned.
 *
 * @param[in] attr  handle to the ULT attribute
 * @return ABTI_thread_attr pointer
 */
static inline
ABTI_thread_attr *ABTI_thread_attr_get_ptr(ABT_thread_attr attr)
{
#ifndef UNSAFE_MODE
    ABTI_thread_attr *p_attr;
    if (attr == ABT_THREAD_ATTR_NULL) {
        p_attr = NULL;
    } else {
        p_attr = (ABTI_thread_attr *)attr;
    }
    return p_attr;
#else
    return (ABTI_thread_attr *)attr;
#endif
}

/**
 * @ingroup ULT_ATTR_PRIVATE
 * @brief   Get \c ABT_thread_attr handle from \c ABTI_thread_attr pointer.
 *
 * \c ABTI_thread_attr_get_handle() returns \c ABT_thread_attr handle
 * corresponding to \c ABTI_thread_attr pointer \c attr. If \c attr is
 * \c NULL, \c ABT_THREAD_NULL is returned.
 *
 * @param[in] p_attr  pointer to ABTI_thread_attr
 * @return ABT_thread_attr handle
 */
static inline
ABT_thread_attr ABTI_thread_attr_get_handle(ABTI_thread_attr *p_attr)
{
#ifndef UNSAFE_MODE
    ABT_thread_attr h_attr;
    if (p_attr == NULL) {
        h_attr = ABT_THREAD_ATTR_NULL;
    } else {
        h_attr = (ABT_thread_attr)p_attr;
    }
    return h_attr;
#else
    return (ABT_thread_attr)p_attr;
#endif
}

/* Tasklet */
static inline
ABTI_task *ABTI_task_get_ptr(ABT_task task)
{
#ifndef UNSAFE_MODE
    ABTI_task *p_task;
    if (task == ABT_TASK_NULL) {
        p_task = NULL;
    } else {
        p_task = (ABTI_task *)task;
    }
    return p_task;
#else
    return (ABTI_task *)task;
#endif
}

static inline
ABT_task ABTI_task_get_handle(ABTI_task *p_task)
{
#ifndef UNSAFE_MODE
    ABT_task h_task;
    if (p_task == NULL) {
        h_task = ABT_TASK_NULL;
    } else {
        h_task = (ABT_task)p_task;
    }
    return h_task;
#else
    return (ABT_task)p_task;
#endif
}

/* Mutex */
static inline
ABTI_mutex *ABTI_mutex_get_ptr(ABT_mutex mutex)
{
#ifndef UNSAFE_MODE
    ABTI_mutex *p_mutex;
    if (mutex == ABT_MUTEX_NULL) {
        p_mutex = NULL;
    } else {
        p_mutex = (ABTI_mutex *)mutex;
    }
    return p_mutex;
#else
    return (ABTI_mutex *)mutex;
#endif
}

static inline
ABT_mutex ABTI_mutex_get_handle(ABTI_mutex *p_mutex)
{
#ifndef UNSAFE_MODE
    ABT_mutex h_mutex;
    if (p_mutex == NULL) {
        h_mutex = ABT_MUTEX_NULL;
    } else {
        h_mutex = (ABT_mutex)p_mutex;
    }
    return h_mutex;
#else
    return (ABT_mutex)p_mutex;
#endif
}

/* Condition Variable */
static inline
ABTI_cond *ABTI_cond_get_ptr(ABT_cond cond)
{
#ifndef UNSAFE_MODE
    ABTI_cond *p_cond;
    if (cond == ABT_COND_NULL) {
        p_cond = NULL;
    } else {
        p_cond = (ABTI_cond *)cond;
    }
    return p_cond;
#else
    return (ABTI_cond *)cond;
#endif
}

static inline
ABT_cond ABTI_cond_get_handle(ABTI_cond *p_cond)
{
#ifndef UNSAFE_MODE
    ABT_cond h_cond;
    if (p_cond == NULL) {
        h_cond = ABT_COND_NULL;
    } else {
        h_cond = (ABT_cond)p_cond;
    }
    return h_cond;
#else
    return (ABT_cond)p_cond;
#endif
}

/* Eventual */
static inline
ABTI_eventual *ABTI_eventual_get_ptr(ABT_eventual eventual)
{
#ifndef UNSAFE_MODE
    ABTI_eventual *p_eventual;
    if (eventual == ABT_EVENTUAL_NULL) {
        p_eventual = NULL;
    } else {
        p_eventual = (ABTI_eventual *)eventual;
    }
    return p_eventual;
#else
    return (ABTI_eventual *)eventual;
#endif
}

static inline
ABT_eventual ABTI_eventual_get_handle(ABTI_eventual *p_eventual)
{
#ifndef UNSAFE_MODE
    ABT_eventual h_eventual;
    if (p_eventual == NULL) {
        h_eventual = ABT_EVENTUAL_NULL;
    } else {
        h_eventual = (ABT_eventual)p_eventual;
    }
    return h_eventual;
#else
    return (ABT_eventual)p_eventual;
#endif
}

/* Future */
static inline
ABTI_future *ABTI_future_get_ptr(ABT_future future)
{
#ifndef UNSAFE_MODE
    ABTI_future *p_future;
    if (future == ABT_FUTURE_NULL) {
        p_future = NULL;
    } else {
        p_future = (ABTI_future *)future;
    }
    return p_future;
#else
    return (ABTI_future *)future;
#endif
}

static inline
ABT_future ABTI_future_get_handle(ABTI_future *p_future)
{
#ifndef UNSAFE_MODE
    ABT_future h_future;
    if (p_future == NULL) {
        h_future = ABT_FUTURE_NULL;
    } else {
        h_future = (ABT_future)p_future;
    }
    return h_future;
#else
    return (ABT_future)p_future;
#endif
}

/* Timer */
static inline
ABTI_timer *ABTI_timer_get_ptr(ABT_timer timer)
{
#ifndef UNSAFE_MODE
    ABTI_timer *p_timer;
    if (timer == ABT_TIMER_NULL) {
        p_timer = NULL;
    } else {
        p_timer = (ABTI_timer *)timer;
    }
    return p_timer;
#else
    return (ABTI_timer *)timer;
#endif
}

static inline
ABT_timer ABTI_timer_get_handle(ABTI_timer *p_timer)
{
#ifndef UNSAFE_MODE
    ABT_timer h_timer;
    if (p_timer == NULL) {
        h_timer = ABT_TIMER_NULL;
    } else {
        h_timer = (ABT_timer)p_timer;
    }
    return h_timer;
#else
    return (ABT_timer)p_timer;
#endif
}

#endif /* ABTI_HANDLE_H_INCLUDED */
