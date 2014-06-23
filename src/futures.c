/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include <stdlib.h>
#include <string.h>
#include "abti.h"

/** @defgroup Future Future
 * Futures are used to wait for values asynchronously.
 */

/**
 * @ingroup Future
 * @brief   Blocks current thread until the feature has finished its computation.
 *
 * @param[in]  future       Reference to the future
 * @param[out] value        Reference to value of future
 * @return Error code
 */
int ABT_future_wait(ABT_future future, void **value)
{
	int abt_errno = ABT_SUCCESS;
	ABTI_future *p_future = ABTI_future_get_ptr(future);
    ABTI_CHECK_NULL_FUTURE_PTR(p_future);

	ABT_mutex_lock(p_future->mutex);
    if (!p_future->ready) {
        ABTI_thread_entry *cur = (ABTI_thread_entry*) ABTU_malloc(sizeof(ABTI_thread_entry));
        cur->current = ABTI_thread_current();
        cur->next = NULL;
        if(p_future->waiters.tail != NULL)
            p_future->waiters.tail->next = cur;
        p_future->waiters.tail = cur;
        if(p_future->waiters.head == NULL)
            p_future->waiters.head = cur;
		ABT_mutex_unlock(p_future->mutex);
        ABTI_thread_suspend();
    } else {
		ABT_mutex_unlock(p_future->mutex);
	}
    *value = p_future->value;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_future_wait", abt_errno);
    goto fn_exit;
}

/**
 * @ingroup Future
 * @brief   Signals all threads blocking on a future once the result has been calculated.
 *
 * @param[in]  data       Pointer to future's data
 * @return No value returned
 */
void ABTI_future_signal(ABTI_future *p_future)
{
    ABTI_thread_entry *cur = p_future->waiters.head;
    while (cur != NULL)
    {
        ABT_thread mythread = cur->current;
        ABTI_thread_set_ready(mythread);
        ABTI_thread_entry *tmp = cur;
        cur=cur->next;
        free(tmp);
    }
}

/**
 * @ingroup Future
 * @brief   Sets a nbytes-value into a future for all threads blocking on the future to resume.
 *
 * @param[in]  future       Reference to the future
 * @param[in]  value        Pointer to the buffer containing the result
 * @param[in]  nbytes       Number of bytes in the buffer
 * @return Error code
 */
int ABT_future_set(ABT_future future, void *value, int nbytes)
{
	int abt_errno = ABT_SUCCESS;
	ABTI_future *p_future = ABTI_future_get_ptr(future);
    ABTI_CHECK_NULL_FUTURE_PTR(p_future);

	ABT_mutex_lock(p_future->mutex);
    p_future->ready = 1;
    memcpy(p_future->value, value, nbytes);
	ABT_mutex_unlock(p_future->mutex);
    ABTI_future_signal(p_future);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_future_set", abt_errno);
    goto fn_exit;
}

/**
 * @ingroup Future
 * @brief   Creates a future.
 *
 * @param[in]  n            Number of bytes in the buffer containing the result of the future
 * @param[out] newfuture       Reference to the newly created future
 * @return Error code
 */
int ABT_future_create(int n, ABT_future *newfuture)
{
	int abt_errno = ABT_SUCCESS;
    ABTI_future *p_future = (ABTI_future*)ABTU_malloc(sizeof(ABTI_future));
    if (!p_future) {
        HANDLE_ERROR("ABTU_malloc");
        *newfuture = ABT_FUTURE_NULL;
        abt_errno = ABT_ERR_MEM;
        goto fn_fail;
    }

    ABT_mutex_create(&p_future->mutex);
    p_future->ready = 0;
    p_future->nbytes = n;
    p_future->value = ABTU_malloc(n);
    p_future->waiters.head = p_future->waiters.tail = NULL;
    *newfuture = ABTI_future_get_handle(p_future);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_future_create", abt_errno);
    goto fn_exit;
}

/**
 * @ingroup Future
 * @brief   Releases the memory of a future.
 *
 * @param[out] future       Reference to the future
 * @return Error code
 */
int ABT_future_free(ABT_future *future)
{
	int abt_errno = ABT_SUCCESS;
	ABTI_future *p_future = ABTI_future_get_ptr(*future);
    ABTI_CHECK_NULL_FUTURE_PTR(p_future);

	ABT_mutex_free(&p_future->mutex);
	ABTU_free(p_future->value);
	ABTU_free(p_future);

    *future = ABT_FUTURE_NULL;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_future_free", abt_errno);
    goto fn_exit;
}

/* Private API */
ABTI_future *ABTI_future_get_ptr(ABT_future future)
{
    ABTI_future *p_future;
    if (future == ABT_FUTURE_NULL) {
        p_future = NULL;
    } else {
        p_future = (ABTI_future *)future;
    }
    return p_future;
}

ABT_future ABTI_future_get_handle(ABTI_future *p_future)
{
    ABT_future h_future;
    if (p_future == NULL) {
        h_future = ABT_FUTURE_NULL;
    } else {
        h_future = (ABT_future)p_future;
    }
    return h_future;
}
