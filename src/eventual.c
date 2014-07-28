/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include <stdlib.h>
#include <string.h>
#include "abti.h"

/** @defgroup Eventual Eventual
 * Eventuals are used to wait for values asynchronously.
 */

/**
 * @ingroup Eventual
 * @brief   Blocks current thread until the feature has finished its computation.
 *
 * @param[in]  eventual       Reference to the eventual
 * @param[out] value        Reference to value of eventual
 * @return Error code
 */
int ABT_eventual_wait(ABT_eventual eventual, void **value)
{
	int abt_errno = ABT_SUCCESS;
	ABTI_eventual *p_eventual = ABTI_eventual_get_ptr(eventual);
    ABTI_CHECK_NULL_EVENTUAL_PTR(p_eventual);

	ABT_mutex_lock(p_eventual->mutex);
    if (!p_eventual->ready) {
        ABTI_thread_entry *cur = (ABTI_thread_entry*) ABTU_malloc(sizeof(ABTI_thread_entry));
        cur->current = ABTI_thread_current();
        cur->next = NULL;
        if(p_eventual->waiters.tail != NULL)
            p_eventual->waiters.tail->next = cur;
        p_eventual->waiters.tail = cur;
        if(p_eventual->waiters.head == NULL)
            p_eventual->waiters.head = cur;
		ABT_mutex_unlock(p_eventual->mutex);
        ABTI_thread_suspend();
    } else {
		ABT_mutex_unlock(p_eventual->mutex);
	}
    *value = p_eventual->value;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_eventual_wait", abt_errno);
    goto fn_exit;
}

/**
 * @ingroup Eventual
 * @brief   Signals all threads blocking on a eventual once the result has been calculated.
 *
 * @param[in]  data       Pointer to eventual's data
 * @return No value returned
 */
void ABTI_eventual_signal(ABTI_eventual *p_eventual)
{
    ABTI_thread_entry *cur = p_eventual->waiters.head;
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
 * @ingroup Eventual
 * @brief   Sets a nbytes-value into a eventual for all threads blocking on the eventual to resume.
 *
 * @param[in]  eventual       Reference to the eventual
 * @param[in]  value        Pointer to the buffer containing the result
 * @param[in]  nbytes       Number of bytes in the buffer
 * @return Error code
 */
int ABT_eventual_set(ABT_eventual eventual, void *value, int nbytes)
{
	int abt_errno = ABT_SUCCESS;
	ABTI_eventual *p_eventual = ABTI_eventual_get_ptr(eventual);
    ABTI_CHECK_NULL_EVENTUAL_PTR(p_eventual);

	ABT_mutex_lock(p_eventual->mutex);
    p_eventual->ready = 1;
    memcpy(p_eventual->value, value, nbytes);
	ABT_mutex_unlock(p_eventual->mutex);
    ABTI_eventual_signal(p_eventual);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_eventual_set", abt_errno);
    goto fn_exit;
}

/**
 * @ingroup Eventual
 * @brief   Creates a eventual.
 *
 * @param[in]  n            Number of bytes in the buffer containing the result of the eventual
 * @param[out] neweventual       Reference to the newly created eventual
 * @return Error code
 */
int ABT_eventual_create(int n, ABT_eventual *neweventual)
{
	int abt_errno = ABT_SUCCESS;
    ABTI_eventual *p_eventual = (ABTI_eventual*)ABTU_malloc(sizeof(ABTI_eventual));
    if (!p_eventual) {
        HANDLE_ERROR("ABTU_malloc");
        *neweventual = ABT_EVENTUAL_NULL;
        abt_errno = ABT_ERR_MEM;
        goto fn_fail;
    }

    ABT_mutex_create(&p_eventual->mutex);
    p_eventual->ready = 0;
    p_eventual->nbytes = n;
    p_eventual->value = ABTU_malloc(n);
    p_eventual->waiters.head = p_eventual->waiters.tail = NULL;
    *neweventual = ABTI_eventual_get_handle(p_eventual);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_eventual_create", abt_errno);
    goto fn_exit;
}

/**
 * @ingroup Eventual
 * @brief   Releases the memory of a eventual.
 *
 * @param[out] eventual       Reference to the eventual
 * @return Error code
 */
int ABT_eventual_free(ABT_eventual *eventual)
{
	int abt_errno = ABT_SUCCESS;
	ABTI_eventual *p_eventual = ABTI_eventual_get_ptr(*eventual);
    ABTI_CHECK_NULL_EVENTUAL_PTR(p_eventual);

	ABT_mutex_free(&p_eventual->mutex);
	ABTU_free(p_eventual->value);
	ABTU_free(p_eventual);

    *eventual = ABT_EVENTUAL_NULL;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_eventual_free", abt_errno);
    goto fn_exit;
}

/* Private API */
ABTI_eventual *ABTI_eventual_get_ptr(ABT_eventual eventual)
{
    ABTI_eventual *p_eventual;
    if (eventual == ABT_EVENTUAL_NULL) {
        p_eventual = NULL;
    } else {
        p_eventual = (ABTI_eventual *)eventual;
    }
    return p_eventual;
}

ABT_eventual ABTI_eventual_get_handle(ABTI_eventual *p_eventual)
{
    ABT_eventual h_eventual;
    if (p_eventual == NULL) {
        h_eventual = ABT_EVENTUAL_NULL;
    } else {
        h_eventual = (ABT_eventual)p_eventual;
    }
    return h_eventual;
}
