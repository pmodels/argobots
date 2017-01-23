/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"
#include <sys/time.h>


/** @defgroup COND Condition Variable
 * This group is for Condition Variable.
 */

/**
 * @ingroup COND
 * @brief   Create a new condition variable.
 *
 * \c ABT_cond_create() creates a new condition variable and returns its handle
 * through \c newcond.
 * If an error occurs in this routine, a non-zero error code will be returned
 * and newcond will be set to \c ABT_COND_NULL.
 *
 * @param[out] newcond  handle to a new condition variable
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_cond_create(ABT_cond *newcond)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_cond *p_newcond;

    p_newcond = (ABTI_cond *)ABTU_malloc(sizeof(ABTI_cond));
    ABTI_cond_init(p_newcond);

    /* Return value */
    *newcond = ABTI_cond_get_handle(p_newcond);

    return abt_errno;
}

/**
 * @ingroup COND
 * @brief   Free the condition variable.
 *
 * \c ABT_cond_free() deallocates the memory used for the condition variable
 * object associated with the handle \c cond. If it is successfully processed,
 * \c cond is set to \c ABT_COND_NULL.
 *
 * @param[in,out] cond  handle to the condition variable
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_cond_free(ABT_cond *cond)
{
    int abt_errno = ABT_SUCCESS;
    ABT_cond h_cond = *cond;
    ABTI_cond *p_cond = ABTI_cond_get_ptr(h_cond);
    ABTI_CHECK_NULL_COND_PTR(p_cond);

    ABTI_CHECK_TRUE(p_cond->num_waiters == 0, ABT_ERR_COND);

    ABTI_cond_fini(p_cond);
    ABTU_free(p_cond);

    /* Return value */
    *cond = ABT_COND_NULL;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

/**
 * @ingroup COND
 * @brief   Wait on the condition.
 *
 * The ULT calling \c ABT_cond_wait() waits on the condition variable until
 * it is signaled.
 * The user should call this routine while the mutex specified as \c mutex is
 * locked. The mutex will be automatically released while waiting. After signal
 * is received and the waiting ULT is awakened, the mutex will be
 * automatically locked for use by the ULT. The user is then responsible for
 * unlocking mutex when the ULT is finished with it.
 *
 * @param[in] cond   handle to the condition variable
 * @param[in] mutex  handle to the mutex
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_cond_wait(ABT_cond cond, ABT_mutex mutex)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_cond *p_cond = ABTI_cond_get_ptr(cond);
    ABTI_CHECK_NULL_COND_PTR(p_cond);
    ABTI_mutex *p_mutex = ABTI_mutex_get_ptr(mutex);
    ABTI_CHECK_NULL_MUTEX_PTR(p_mutex);

    abt_errno = ABTI_cond_wait(p_cond, p_mutex);
    if (abt_errno != ABT_SUCCESS)
        goto fn_fail;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}


static inline
double convert_timespec_to_sec(const struct timespec *p_ts)
{
    double secs;
    secs = ((double)p_ts->tv_sec) + 1.0e-9 * ((double)p_ts->tv_nsec);
    return secs;
}

static inline
double get_cur_time(void)
{
#if defined(HAVE_CLOCK_GETTIME)
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return convert_timespec_to_sec(&ts);
#elif defined(HAVE_GETTIMEOFDAY)
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return ((double)tv.tv_sec) + 1.0e-6 * ((double)tv.tv_usec);
#else
#error "No timer function available"
    return 0.0;
#endif
}

static inline
void remove_unit(ABTI_cond *p_cond, ABTI_unit *p_unit)
{
    if (p_unit->p_next == NULL) return;

    ABTI_spinlock_acquire(&p_cond->lock);

    if (p_unit->p_next == NULL) {
        ABTI_spinlock_release(&p_cond->lock);
        return ;
    }

    /* If p_unit is still in the queue, we have to remove it. */
    p_cond->num_waiters--;
    if (p_cond->num_waiters == 0) {
        p_cond->p_waiter_mutex = NULL;
        p_cond->p_head = NULL;
        p_cond->p_tail = NULL;
    } else {
        p_unit->p_prev->p_next = p_unit->p_next;
        p_unit->p_next->p_prev = p_unit->p_prev;
        if (p_unit == p_cond->p_head) {
            p_cond->p_head = p_unit->p_next;
        } else if (p_unit == p_cond->p_tail) {
            p_cond->p_tail = p_unit->p_prev;
        }
    }

    ABTI_spinlock_release(&p_cond->lock);

    p_unit->p_prev = NULL;
    p_unit->p_next = NULL;
}

/**
 * @ingroup COND
 * @brief   Wait on the condition.
 *
 * The ULT calling \c ABT_cond_timedwait() waits on the condition variable
 * until it is signaled or the absolute time specified by \c abstime passes.
 * If system time equals or exceeds \c abstime before \c cond is signaled,
 * the error code \c ABT_ERR_COND_TIMEDOUT is returned.
 *
 * The user should call this routine while the mutex specified as \c mutex is
 * locked. The mutex will be automatically released while waiting. After signal
 * is received and the waiting ULT is awakened, the mutex will be
 * automatically locked for use by the ULT. The user is then responsible for
 * unlocking mutex when the ULT is finished with it.
 *
 * @param[in] cond     handle to the condition variable
 * @param[in] mutex    handle to the mutex
 * @param[in] abstime  absolute time for timeout
 * @return Error code
 * @retval ABT_SUCCESS            on success
 * @retval ABT_ERR_COND_TIMEDOUT  timeout
 */
int ABT_cond_timedwait(ABT_cond cond, ABT_mutex mutex,
                       const struct timespec *abstime)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_cond *p_cond = ABTI_cond_get_ptr(cond);
    ABTI_CHECK_NULL_COND_PTR(p_cond);
    ABTI_mutex *p_mutex = ABTI_mutex_get_ptr(mutex);
    ABTI_CHECK_NULL_MUTEX_PTR(p_mutex);

    double tar_time = convert_timespec_to_sec(abstime);

    ABTI_unit *p_unit;
    volatile int ext_signal = 0;

    p_unit = (ABTI_unit *)ABTU_calloc(1, sizeof(ABTI_unit));
    p_unit->pool = (ABT_pool)&ext_signal;
    p_unit->type = ABT_UNIT_TYPE_EXT;

    ABTI_spinlock_acquire(&p_cond->lock);

    if (p_cond->p_waiter_mutex == NULL) {
        p_cond->p_waiter_mutex = p_mutex;
    } else {
        ABT_bool result = ABTI_mutex_equal(p_cond->p_waiter_mutex, p_mutex);
        if (result == ABT_FALSE) {
            ABTI_spinlock_release(&p_cond->lock);
            abt_errno = ABT_ERR_INV_MUTEX;
            goto fn_fail;
        }
    }

    if (p_cond->num_waiters == 0) {
        p_unit->p_prev = p_unit;
        p_unit->p_next = p_unit;
        p_cond->p_head = p_unit;
        p_cond->p_tail = p_unit;
    } else {
        p_cond->p_tail->p_next = p_unit;
        p_cond->p_head->p_prev = p_unit;
        p_unit->p_prev = p_cond->p_tail;
        p_unit->p_next = p_cond->p_head;
        p_cond->p_tail = p_unit;
    }

    p_cond->num_waiters++;

    ABTI_spinlock_release(&p_cond->lock);

    /* Unlock the mutex that the calling ULT is holding */
    ABTI_mutex_unlock(p_mutex);

    while (!ext_signal) {
        double cur_time = get_cur_time();
        if (cur_time >= tar_time) {
            remove_unit(p_cond, p_unit);
            abt_errno = ABT_ERR_COND_TIMEDOUT;
            break;
        }
        ABT_thread_yield();
    }
    ABTU_free(p_unit);

    /* Lock the mutex again */
    ABTI_mutex_spinlock(p_mutex);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}


/**
 * @ingroup COND
 * @brief   Signal a condition.
 *
 * \c ABT_cond_signal() signals another ULT that is waiting on the condition
 * variable. Only one ULT is waken up by the signal and the scheduler
 * determines the ULT.
 * This routine shall have no effect if no ULTs are currently blocked on the
 * condition variable.
 *
 * @param[in] cond   handle to the condition variable
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_cond_signal(ABT_cond cond)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_cond *p_cond = ABTI_cond_get_ptr(cond);
    ABTI_CHECK_NULL_COND_PTR(p_cond);

    ABTI_spinlock_acquire(&p_cond->lock);

    if (p_cond->num_waiters == 0) {
        ABTI_spinlock_release(&p_cond->lock);
        goto fn_exit;
    }

    /* Wake up the first waiting ULT */
    ABTI_unit *p_unit = p_cond->p_head;

    p_cond->num_waiters--;
    if (p_cond->num_waiters == 0) {
        p_cond->p_waiter_mutex = NULL;
        p_cond->p_head = NULL;
        p_cond->p_tail = NULL;
    } else {
        p_unit->p_prev->p_next = p_unit->p_next;
        p_unit->p_next->p_prev = p_unit->p_prev;
        p_cond->p_head = p_unit->p_next;
    }
    p_unit->p_prev = NULL;
    p_unit->p_next = NULL;

    if (p_unit->type == ABT_UNIT_TYPE_THREAD) {
        ABTI_thread *p_thread = ABTI_thread_get_ptr(p_unit->thread);
        ABTI_thread_set_ready(p_thread);
    } else {
        /* When the head is an external thread */
        volatile int *p_ext_signal = (volatile int *)p_unit->pool;
        *p_ext_signal = 1;
    }

    ABTI_spinlock_release(&p_cond->lock);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

/**
 * @ingroup COND
 * @brief   Broadcast a condition.
 *
 * \c ABT_cond_broadcast() signals all ULTs that are waiting on the
 * condition variable.
 * This routine shall have no effect if no ULTs are currently blocked on the
 * condition variable.
 *
 * @param[in] cond   handle to the condition variable
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_cond_broadcast(ABT_cond cond)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_cond *p_cond = ABTI_cond_get_ptr(cond);
    ABTI_CHECK_NULL_COND_PTR(p_cond);

    ABTI_cond_broadcast(p_cond);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

