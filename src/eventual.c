/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"


/** @defgroup EVENTUAL Eventual
 * In Argobots, an \a eventual corresponds to the traditional behavior of
 * the future concept (refer to \ref FUTURE "Future"). A ULT creates an
 * eventual, which is a memory buffer that will eventually contain a value
 * of interest. Many ULTs can wait on the eventual (a blocking call),
 * until one ULT signals on that future.
 */

/**
 * @ingroup EVENTUAL
 * @brief   Create an eventual.
 *
 * \c ABT_eventual_create creates an eventual and returns a handle to the newly
 * created eventual into \c neweventual. This routine allocates a memory buffer
 * of \c nbytes size and creates a list of entries for all the ULTs that will
 * be blocked waiting for the eventual to be ready. The list is initially empty.
 *
 * @param[in]  nbytes       size in bytes of the memory buffer
 * @param[out] neweventual  handle to a new eventual
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_eventual_create(int nbytes, ABT_eventual *neweventual)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_eventual *p_eventual;

    p_eventual = (ABTI_eventual *)ABTU_malloc(sizeof(ABTI_eventual));
    ABTI_mutex_init(&p_eventual->mutex);
    p_eventual->ready = ABT_FALSE;
    p_eventual->nbytes = nbytes;
    p_eventual->value = ABTU_malloc(nbytes);
    p_eventual->waiters.head = p_eventual->waiters.tail = NULL;
    *neweventual = ABTI_eventual_get_handle(p_eventual);

    return abt_errno;
}

/**
 * @ingroup EVENTUAL
 * @brief   Free the eventual object.
 *
 * \c ABT_eventual_free releases memory associated with the eventual
 * \c eventual. It also deallocates the memory buffer of the eventual.
 * If it is successfully processed, \c eventual is set to \c ABT_EVENTUAL_NULL.
 *
 * @param[in,out] eventual  handle to the eventual
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_eventual_free(ABT_eventual *eventual)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_eventual *p_eventual = ABTI_eventual_get_ptr(*eventual);
    ABTI_CHECK_NULL_EVENTUAL_PTR(p_eventual);

    /* The lock needs to be acquired to safely free the eventual structure.
     * However, we do not have to unlock it because the entire structure is
     * freed here. */
    ABTI_mutex_spinlock(&p_eventual->mutex);

    ABTU_free(p_eventual->value);
    ABTU_free(p_eventual);

    *eventual = ABT_EVENTUAL_NULL;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

/**
 * @ingroup EVENTUAL
 * @brief   Wait on the eventual.
 *
 * \c ABT_eventual_wait blocks the caller ULT until the eventual \c eventual
 * is resolved. If the eventual is not ready, the ULT calling this routine
 * suspends and goes to the state BLOCKED. Internally, an entry is created
 * per each blocked ULT to be awaken when the eventual is signaled.
 * If the eventual is ready, the pointer pointed to by \c value will point to
 * the memory buffer associated with the eventual. The system keeps a list of
 * all the ULTs waiting on the eventual.
 *
 * @param[in]  eventual handle to the eventual
 * @param[out] value    pointer to the memory buffer of the eventual
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_eventual_wait(ABT_eventual eventual, void **value)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_eventual *p_eventual = ABTI_eventual_get_ptr(eventual);
    ABTI_CHECK_NULL_EVENTUAL_PTR(p_eventual);

    ABTI_mutex_lock(&p_eventual->mutex);
    if (p_eventual->ready == ABT_FALSE) {
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

        if (p_eventual->waiters.tail != NULL)
            p_eventual->waiters.tail->next = cur;
        p_eventual->waiters.tail = cur;
        if (p_eventual->waiters.head == NULL)
            p_eventual->waiters.head = cur;

        if (type == ABT_UNIT_TYPE_THREAD) {
            ABTI_thread_set_blocked(p_current);
        }
        ABTI_mutex_unlock(&p_eventual->mutex);

        if (type == ABT_UNIT_TYPE_THREAD) {
            ABTI_thread_suspend(p_current);
        } else {
            /* External thread is waiting here polling ext_signal. */
            /* FIXME: need a better implementation */
            while (!ext_signal);
        }
    } else {
        ABTI_mutex_unlock(&p_eventual->mutex);
    }
    *value = p_eventual->value;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

/**
 * @ingroup EVENTUAL
 * @brief   Signal the eventual.
 *
 * \c ABT_eventual_set sets a value in the eventual's buffer and releases all
 * waiting ULTs. It copies \c nbytes bytes from the buffer pointed to by
 * \c value into the internal buffer of eventual and awakes all ULTs waiting
 * on the eventual. Therefore, all ULTs waiting on this eventual will be ready
 * to be scheduled.
 *
 * @param[in] eventual  handle to the eventual
 * @param[in] value     pointer to the memory buffer containing the data that
 *                      will be copied to the memory buffer of the eventual
 * @param[in] nbytes    number of bytes to be copied
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_eventual_set(ABT_eventual eventual, void *value, int nbytes)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_eventual *p_eventual = ABTI_eventual_get_ptr(eventual);
    ABTI_CHECK_NULL_EVENTUAL_PTR(p_eventual);

    ABTI_mutex_lock(&p_eventual->mutex);
    p_eventual->ready = ABT_TRUE;
    memcpy(p_eventual->value, value, nbytes);
    ABTI_eventual_signal(p_eventual);
    ABTI_mutex_unlock(&p_eventual->mutex);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
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

