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
    ABTI_spinlock_create(&p_eventual->lock);
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

    ABTI_spinlock_free(&p_eventual->lock);
    if (p_eventual->value) ABTU_free(p_eventual->value);
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

    ABTI_spinlock_acquire(&p_eventual->lock);
    if (p_eventual->ready == ABT_FALSE) {
        ABTI_thread *p_current;
        ABTI_unit *p_unit;
        ABT_unit_type type;
        volatile int ext_signal = 0;

        if (lp_ABTI_local != NULL) {
            p_current = ABTI_local_get_thread();
            ABTI_CHECK_TRUE(p_current != NULL, ABT_ERR_EVENTUAL);

            type = ABT_UNIT_TYPE_THREAD;
            p_unit = &p_current->unit_def;
            p_unit->thread = ABTI_thread_get_handle(p_current);
            p_unit->type = type;
        } else {
            /* external thread */
            type = ABT_UNIT_TYPE_EXT;
            p_unit = (ABTI_unit *)ABTU_calloc(1, sizeof(ABTI_unit));
            p_unit->pool = (ABT_pool)&ext_signal;
            p_unit->type = type;
        }

        p_unit->p_next = NULL;
        if (p_eventual->p_head == NULL) {
            p_eventual->p_head = p_unit;
            p_eventual->p_tail = p_unit;
        } else {
            p_eventual->p_tail->p_next = p_unit;
            p_eventual->p_tail = p_unit;
        }

        if (type == ABT_UNIT_TYPE_THREAD) {
            ABTI_thread_set_blocked(p_current);

            ABTI_spinlock_release(&p_eventual->lock);

            /* Suspend the current ULT */
            ABTI_thread_suspend(p_current);

        } else {
            ABTI_spinlock_release(&p_eventual->lock);

            /* External thread is waiting here polling ext_signal. */
            /* FIXME: need a better implementation */
            while (!ext_signal) {
            }
            ABTU_free(p_unit);
        }
    } else {
        ABTI_spinlock_release(&p_eventual->lock);
    }
    if (value) *value = p_eventual->value;

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
    ABTI_CHECK_TRUE(nbytes <= p_eventual->nbytes, ABT_ERR_INV_EVENTUAL);

    ABTI_spinlock_acquire(&p_eventual->lock);

    p_eventual->ready = ABT_TRUE;
    if (p_eventual->value) memcpy(p_eventual->value, value, nbytes);

    if (p_eventual->p_head == NULL) {
        ABTI_spinlock_release(&p_eventual->lock);
        goto fn_exit;
    }

    /* Wake up all waiting ULTs */
    ABTI_unit *p_head = p_eventual->p_head;
    ABTI_unit *p_unit = p_head;
    while (1) {
        ABTI_unit *p_next = p_unit->p_next;
        ABT_unit_type type = p_unit->type;

        p_unit->p_next = NULL;

        if (type == ABT_UNIT_TYPE_THREAD) {
            ABTI_thread *p_thread = ABTI_thread_get_ptr(p_unit->thread);
            ABTI_thread_set_ready(p_thread);
        } else {
            /* When the head is an external thread */
            volatile int *p_ext_signal = (volatile int *)p_unit->pool;
            *p_ext_signal = 1;
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

