/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"


/** @defgroup FUTURE Future
 * A future, an eventual, or a \a promise, is a mechanism for passing a value
 * between threads, allowing a thread to wait for a value that is set
 * asynchronously. It is used to increase concurrency in a parallel program.
 * This construction is really popular in functional programming languages,
 * in particular MultiLisp. If the programmer defines a future containing
 * an expression, the runtime system \a promises to evaluate that expression
 * concurrently. The resulting value of the expression might not be available
 * immediately, but it will be eventually computed. Therefore, futures also
 * require a synchronization interface between the program and the multiple
 * concurrent threads that may be computing portions of the code.
 *
 * In Argobots, futures are used with the purpose of synchronizing execution
 * between cooperating concurrent ULTs. There are two basic mechanisms
 * implemented, \ref EVENTUAL "eventuals" and futures.
 *
 * A \a future in Argobots has a slightly different behavior. A future is
 * created with a number of \a compartments. Each of those \a k compartments
 * will be set by contributing ULTs. Any other ULT will block on a future
 * until all the compartments have been set. In some sense, a future is
 * a multiple-buffer extension of an eventual. Eventuals and futures have
 * a different philosophy of memory management. An eventual will create and
 * destroy the memory buffer that will hold a result. In contrast, a future
 * does not create any buffer. Therefore, a future assumes each contributing
 * ULT allocates and destroys all memory buffers. When a contributing ULT
 * sets a value, it just passes a pointer to the particular memory location.
 */

/**
 * @ingroup FUTURE
 * @brief   Create a future.
 *
 * \c ABT_future_create creates a future and returns a handle to the newly
 * created future into \c newfuture. This routine allocates an array with
 * as many \c compartments as defined. Each compartment consists in a void*
 * pointer. The future has a counter to determine whether all contributions
 * have been made. This routine also creates a list of entries for all the
 * ULTs that will be blocked waiting for the future to be ready. The list
 * is initially empty. The entries in the list are set with the same order as
 * the \c ABT_future_set are terminated.
 *
 * @param[in]  compartments number of compartments in the future
 * @param[in]  cb_func      callback function to be called once the future
 *                          is ready
 * @param[out] newfuture    handle to a new future
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_future_create(uint32_t compartments, void (*cb_func)(void **arg),
                      ABT_future *newfuture)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_future *p_future = (ABTI_future*)ABTU_malloc(sizeof(ABTI_future));
    ABTI_mutex_init(&p_future->mutex);
    p_future->ready = ABT_FALSE;
    p_future->counter = 0;
    p_future->compartments = compartments;
    p_future->array = ABTU_malloc(compartments * sizeof(void *));
    p_future->p_callback = cb_func;
    p_future->waiters.head = p_future->waiters.tail = NULL;
    *newfuture = ABTI_future_get_handle(p_future);

    return abt_errno;
}

/**
 * @ingroup FUTURE
 * @brief   Free the future object.
 *
 * \c ABT_future_free releases memory associated with the future \c future.
 * It also deallocates the array of compartments of the future. If it is
 * successfully processed, \c future is set to \c ABT_FUTURE_NULL.
 *
 * @param[in,out] future  handle to the future
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_future_free(ABT_future *future)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_future *p_future = ABTI_future_get_ptr(*future);
    ABTI_CHECK_NULL_FUTURE_PTR(p_future);

    /* The lock needs to be acquired to safely free the future structure.
     * However, we do not have to unlock it because the entire structure is
     * freed here. */
    ABTI_mutex_spinlock(&p_future->mutex);

    ABTU_free(p_future->array);
    ABTU_free(p_future);

    *future = ABT_FUTURE_NULL;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

/**
 * @ingroup FUTURE
 * @brief   Wait on the future.
 *
 * \c ABT_future_wait blocks the caller ULT until the future \c future is
 * resolved. If the future is not ready, the ULT calling this routine
 * suspends and goes to state BLOCKED. Internally, an entry is created per
 * each blocked ULT to be awaken when the future is signaled. If the future
 * is ready, this routine returns immediately. The system keeps a list of
 * all the ULTs waiting on the future.
 *
 * @param[in] future  handle to the future
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_future_wait(ABT_future future)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_future *p_future = ABTI_future_get_ptr(future);
    ABTI_CHECK_NULL_FUTURE_PTR(p_future);

    ABTI_mutex_lock(&p_future->mutex);
    if (p_future->ready == ABT_FALSE) {
        ABTI_thread_entry *cur;
        ABTI_thread *p_current;
        ABT_unit_type type;
        volatile int ext_signal = 0;

        cur = (ABTI_thread_entry *)ABTU_malloc(sizeof(ABTI_thread_entry));
        if (lp_ABTI_local != NULL) {
            p_current = ABTI_local_get_thread();
            ABTI_CHECK_TRUE(p_current != NULL, ABT_ERR_FUTURE);
            type = ABT_UNIT_TYPE_THREAD;
        } else {
            /* external thread */
            p_current = (ABTI_thread *)&ext_signal;
            type = ABT_UNIT_TYPE_EXT;
        }
        cur->current = p_current;
        cur->next = NULL;
        cur->type = type;

        if (p_future->waiters.tail != NULL)
            p_future->waiters.tail->next = cur;
        p_future->waiters.tail = cur;
        if (p_future->waiters.head == NULL)
            p_future->waiters.head = cur;

        if (type == ABT_UNIT_TYPE_THREAD) {
            ABTI_thread_set_blocked(p_current);
        }
        ABTI_mutex_unlock(&p_future->mutex);

        if (type == ABT_UNIT_TYPE_THREAD) {
            ABTI_thread_suspend(p_current);
        } else {
            /* External thread is waiting here polling ext_signal. */
            /* FIXME: need a better implementation */
            while (!ext_signal);
        }
    } else {
        ABTI_mutex_unlock(&p_future->mutex);
    }

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

/**
 * @ingroup FUTURE
 * @brief   Test whether the future is ready.
 *
 * \c ABT_future_test is a non-blocking function that tests whether the future
 * \c future is ready or not. It returns the result through \c flag.
 *
 * @param[in]  future  handle to the future
 * @param[out] flag    \c ABT_TRUE if future is ready; otherwise, \c ABT_FALSE
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_future_test(ABT_future future, ABT_bool *flag)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_future *p_future = ABTI_future_get_ptr(future);
    ABTI_CHECK_NULL_FUTURE_PTR(p_future);

    *flag = p_future->ready;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

/**
 * @ingroup FUTURE
 * @brief   Signal the future.
 *
 * \c ABT_future_set sets a value in the future's array. If all the
 * contributions have been received, this routine awakes all ULTs waiting on
 * the future \c future. In that case, all ULTs waiting on this future will
 * be ready to be scheduled. If there are contributions still missing, this
 * routine will store the pointer passed by parameter \c value and increase
 * the internal counter.
 *
 * @param[in] future  handle to the future
 * @param[in] value   pointer to the memory buffer containing the data that
 *                    will be pointed by one compartment of the future
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_future_set(ABT_future future, void *value)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_future *p_future = ABTI_future_get_ptr(future);
    ABTI_CHECK_NULL_FUTURE_PTR(p_future);

    ABTI_mutex_spinlock(&p_future->mutex);
    p_future->array[p_future->counter] = value;
    p_future->counter++;
    ABTI_CHECK_TRUE(p_future->counter <= p_future->compartments,
                    ABT_ERR_FUTURE);

    if (p_future->counter == p_future->compartments) {
        p_future->ready = ABT_TRUE;
        if (p_future->p_callback != NULL)
            (*p_future->p_callback)(p_future->array);
        ABTI_future_signal(p_future);
    }
    ABTI_mutex_unlock(&p_future->mutex);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}


/*****************************************************************************/
/* Private APIs                                                              */
/*****************************************************************************/

/** @defgroup FUTURE_PRIVATE Future (Private)
 * This group combines private APIs for future.
 */

/**
 * @ingroup FUTURE_PRIVATE
 * @brief   Signal all ULTs blocking on a future once the result has been
 *          calculated.
 *
 * @param[in] p_future  pointer to the internal future struct
 * @return No value returned
 */
void ABTI_future_signal(ABTI_future *p_future)
{
    ABTI_thread_entry *cur = p_future->waiters.head;
    while (cur != NULL)
    {
        if (cur->type == ABT_UNIT_TYPE_THREAD) {
            ABTI_thread *p_thread = cur->current;
            ABTI_thread_set_ready(p_thread);
        } else {
            /* When cur is an external thread */
            volatile int *p_ext_signal = (volatile int *)cur->current;
            *p_ext_signal = 1;
        }
        ABTI_thread_entry *tmp = cur;
        cur=cur->next;
        ABTU_free(tmp);
    }
}

