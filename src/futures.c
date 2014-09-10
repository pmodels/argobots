/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"


/** @defgroup FUTURE Future
 * Futures are used to wait for values asynchronously.
 */

/**
 * @ingroup FUTURE
 * @brief   Creates a future.
 *
 * @param[in]  n            Number of compartments in the array of pointers the future will hold
 * @param[in]  callback     Pointer to callback function that is executed when future is ready
 * @param[out] newfuture    Reference to the newly created future
 * @return Error code
 */
int ABT_future_create(int n, void (*callback)(void **arg), ABT_future *newfuture)
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
    p_future->counter = n;
    p_future->array = ABTU_malloc(n * sizeof(void *));
    p_future->p_callback = callback;
    p_future->waiters.head = p_future->waiters.tail = NULL;
    *newfuture = ABTI_future_get_handle(p_future);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_future_create", abt_errno);
    goto fn_exit;
}

/**
 * @ingroup FUTURE
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
	ABTU_free(p_future->array);
	ABTU_free(p_future);

    *future = ABT_FUTURE_NULL;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_future_free", abt_errno);
    goto fn_exit;
}

/**
 * @ingroup FUTURE
 * @brief   Blocks current thread until the feature has finished its computation.
 *
 * @param[in]  future       Reference to the future
 * @return Error code
 */
int ABT_future_wait(ABT_future future)
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
		ABTI_thread_set_blocked(ABTI_thread_current());
		ABT_mutex_unlock(p_future->mutex);
        ABTI_thread_relinquish();
    } else {
		ABT_mutex_unlock(p_future->mutex);
	}

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_future_wait", abt_errno);
    goto fn_exit;
}

/**
 * @ingroup FUTURE
 * @brief   Tests whether the future is ready
 *
 * @param[in]  		future       Reference to future
 * @param[out]		flag		 Holds 1 if future is ready; 0 otherwise
 * @return No value returned
 */
int ABT_future_test(ABT_future future, int *flag)
{
	int abt_errno = ABT_SUCCESS;
	ABTI_future *p_future = ABTI_future_get_ptr(future);
    ABTI_CHECK_NULL_FUTURE_PTR(p_future);

	if(p_future->ready)
		*flag = 1;
	else
		*flag = 0;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_future_test", abt_errno);
    goto fn_exit;
}

/**
 * @ingroup FUTURE
 * @brief   Sets another compartment in the future array of pointers. If the future is ready, it awakens all threads
 * blocked on the future, and executes the callback.
 *
 * @param[in]  future       Reference to the future
 * @param[in]  value        Pointer to a buffer containing a result
 * @return Error code
 */
int ABT_future_set(ABT_future future, void *value)
{
	int abt_errno = ABT_SUCCESS;
	ABTI_future *p_future = ABTI_future_get_ptr(future);
    ABTI_CHECK_NULL_FUTURE_PTR(p_future);

	ABT_mutex_lock(p_future->mutex);
	p_future->counter--;
	p_future->array[p_future->counter] = value;
	if(p_future->counter == 0) {
	    p_future->ready = 1;
		if(p_future->p_callback != NULL)
			(*p_future->p_callback)(p_future->array);
    	ABTI_future_signal(p_future);
	}
	ABT_mutex_unlock(p_future->mutex);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_future_set", abt_errno);
    goto fn_exit;
}


/*****************************************************************************/
/* Private APIs                                                              */
/*****************************************************************************/

/** @defgroup FUTURE_PRIVATE Future (Private)
 * This group combines private APIs for future.
 */

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

/**
 * @ingroup FUTURE_PRIVATE
 * @brief   Signals all threads blocking on a future once the result has been
 *          calculated.
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

