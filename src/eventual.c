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
 * created eventual into \c neweventual.  If \c nbytes is not zero, this routine
 * allocates a memory buffer of \c nbytes size and creates a list of entries
 * for all the ULTs that will be blocked waiting for the eventual to be ready.
 * The list is initially empty.  If \c nbytes is zero, the eventual is used
 * without passing the data.
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
    ABTI_spinlock_clear(&p_eventual->lock);
    p_eventual->ready = ABT_FALSE;
    p_eventual->nbytes = nbytes;
    p_eventual->value = (nbytes == 0) ? NULL : ABTU_malloc(nbytes);
    p_eventual->p_head = NULL;
    p_eventual->p_tail = NULL;

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
    ABTI_spinlock_acquire(&p_eventual->lock);

    if (p_eventual->value)
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
    ABTI_xstream *p_local_xstream = ABTI_local_get_xstream();
    ABTI_eventual *p_eventual = ABTI_eventual_get_ptr(eventual);
    ABTI_CHECK_NULL_EVENTUAL_PTR(p_eventual);

    ABTI_spinlock_acquire(&p_eventual->lock);
    if (p_eventual->ready == ABT_FALSE) {
        ABTI_ythread *p_current;
        ABTI_thread *p_unit;

        if (p_local_xstream != NULL) {
            p_unit = p_local_xstream->p_unit;
            ABTI_CHECK_TRUE(ABTI_thread_type_is_thread(p_unit->type),
                            ABT_ERR_EVENTUAL);
            p_current = ABTI_unit_get_thread(p_unit);
        } else {
            /* external thread */
            p_current = NULL;
            p_unit = (ABTI_thread *)ABTU_calloc(1, sizeof(ABTI_thread));
            p_unit->type = ABTI_THREAD_TYPE_EXT;
            /* use state for synchronization */
            ABTD_atomic_relaxed_store_int(&p_unit->state,
                                          ABTI_THREAD_STATE_BLOCKED);
        }

        p_unit->p_next = NULL;
        if (p_eventual->p_head == NULL) {
            p_eventual->p_head = p_unit;
            p_eventual->p_tail = p_unit;
        } else {
            p_eventual->p_tail->p_next = p_unit;
            p_eventual->p_tail = p_unit;
        }

        if (p_current) {
            ABTI_thread_set_blocked(p_current);

            ABTI_spinlock_release(&p_eventual->lock);

            /* Suspend the current ULT */
            ABTI_thread_suspend(&p_local_xstream, p_current,
                                ABT_SYNC_EVENT_TYPE_EVENTUAL,
                                (void *)p_eventual);

        } else {
            ABTI_spinlock_release(&p_eventual->lock);

            /* External thread is waiting here. */
            while (ABTD_atomic_acquire_load_int(&p_unit->state) !=
                   ABTI_THREAD_STATE_READY)
                ;
            ABTU_free(p_unit);
        }
    } else {
        ABTI_spinlock_release(&p_eventual->lock);
    }
    if (value)
        *value = p_eventual->value;

fn_exit:
    return abt_errno;

fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

/**
 * @ingroup EVENTUAL
 * @brief   Test the readiness of an eventual.
 *
 * \c ABT_eventual_test does a nonblocking test on the eventual \c eventual
 * if resolved. If the eventual is not ready, \c is_ready would equal FALSE.
 * If the eventual is ready, the pointer pointed to by \c value will point to
 * the memory buffer associated with the eventual.
 *
 * @param[in]  eventual handle to the eventual
 * @param[out] value    pointer to the memory buffer of the eventual
 * @param[out] is_ready pointer to the a user flag
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_eventual_test(ABT_eventual eventual, void **value, int *is_ready)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_eventual *p_eventual = ABTI_eventual_get_ptr(eventual);
    ABTI_CHECK_NULL_EVENTUAL_PTR(p_eventual);
    int flag = ABT_FALSE;

    ABTI_spinlock_acquire(&p_eventual->lock);
    if (p_eventual->ready != ABT_FALSE) {
        if (value)
            *value = p_eventual->value;
        flag = ABT_TRUE;
    }
    ABTI_spinlock_release(&p_eventual->lock);

    *is_ready = flag;

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
    ABTI_xstream *p_local_xstream = ABTI_local_get_xstream();
    ABTI_eventual *p_eventual = ABTI_eventual_get_ptr(eventual);
    ABTI_CHECK_NULL_EVENTUAL_PTR(p_eventual);
    ABTI_CHECK_TRUE(nbytes <= p_eventual->nbytes, ABT_ERR_INV_EVENTUAL);

    ABTI_spinlock_acquire(&p_eventual->lock);

    p_eventual->ready = ABT_TRUE;
    if (p_eventual->value)
        memcpy(p_eventual->value, value, nbytes);

    if (p_eventual->p_head == NULL) {
        ABTI_spinlock_release(&p_eventual->lock);
        goto fn_exit;
    }

    /* Wake up all waiting ULTs */
    ABTI_thread *p_head = p_eventual->p_head;
    ABTI_thread *p_unit = p_head;
    while (1) {
        ABTI_thread *p_next = p_unit->p_next;
        p_unit->p_next = NULL;

        if (ABTI_thread_type_is_thread(p_unit->type)) {
            ABTI_ythread *p_thread = ABTI_unit_get_thread(p_unit);
            ABTI_thread_set_ready(p_local_xstream, p_thread);
        } else {
            /* When the head is an external thread */
            ABTD_atomic_release_store_int(&p_unit->state,
                                          ABTI_THREAD_STATE_READY);
        }

        /* Next ULT */
        if (p_next != NULL) {
            p_unit = p_next;
        } else {
            break;
        }
    }

    p_eventual->p_head = NULL;
    p_eventual->p_tail = NULL;

    ABTI_spinlock_release(&p_eventual->lock);

fn_exit:
    return abt_errno;

fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

/**
 * @ingroup EVENTUAL
 * @brief   Reset the readiness of the target eventual.
 *
 * \c ABT_eventual_reset() resets the readiness of the target eventual
 * \c eventual so that it can be reused.  That is, it makes \c eventual
 * unready irrespective of its readiness.
 *
 * @param[in] eventual  handle to the target eventual
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_eventual_reset(ABT_eventual eventual)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_eventual *p_eventual = ABTI_eventual_get_ptr(eventual);
    ABTI_CHECK_NULL_EVENTUAL_PTR(p_eventual);

    ABTI_spinlock_acquire(&p_eventual->lock);
    p_eventual->ready = ABT_FALSE;
    ABTI_spinlock_release(&p_eventual->lock);

fn_exit:
    return abt_errno;

fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}
