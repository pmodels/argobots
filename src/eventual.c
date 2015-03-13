/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"


/** @defgroup EVENTUAL Eventual
 * Eventuals are used to wait for values asynchronously.
 */

/**
 * @ingroup EVENTUAL
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
    p_eventual->ready = ABT_FALSE;
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
 * @ingroup EVENTUAL
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

/**
 * @ingroup EVENTUAL
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
    if (p_eventual->ready == ABT_FALSE) {
        ABTI_thread_entry *cur = (ABTI_thread_entry*) ABTU_malloc(sizeof(ABTI_thread_entry));
        ABTI_thread *p_current = ABTI_thread_current();
        cur->current = p_current;
        cur->next = NULL;
        if(p_eventual->waiters.tail != NULL)
            p_eventual->waiters.tail->next = cur;
        p_eventual->waiters.tail = cur;
        if(p_eventual->waiters.head == NULL)
            p_eventual->waiters.head = cur;
		ABTI_thread_set_blocked(p_current);
		ABT_mutex_unlock(p_eventual->mutex);
        ABTI_thread_suspend(p_current);
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
 * @ingroup EVENTUAL
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
    p_eventual->ready = ABT_TRUE;
    memcpy(p_eventual->value, value, nbytes);
	ABT_mutex_unlock(p_eventual->mutex);
    ABTI_eventual_signal(p_eventual);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_WITH_CODE("ABT_eventual_set", abt_errno);
    goto fn_exit;
}


/*****************************************************************************/
/* Private APIs                                                              */
/*****************************************************************************/

/** @defgroup EVENTUAL_PRIVATE Eventual (Private)
 * This group combines private APIs for eventual.
 */

/**
 * @ingroup EVENTUAL_PRIVATE
 * @brief   Signal all ULTs blocking on an eventual once the result has been
 *          calculated.
 *
 * @param[in] p_eventual  pointer to internal eventual struct
 * @return No value returned
 */
void ABTI_eventual_signal(ABTI_eventual *p_eventual)
{
    ABTI_thread_entry *cur = p_eventual->waiters.head;
    while (cur != NULL)
    {
        ABTI_thread *p_thread = cur->current;
        ABTI_thread_set_ready(p_thread);
        ABTI_thread_entry *tmp = cur;
        cur=cur->next;
        ABTU_free(tmp);
    }
}

