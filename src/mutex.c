/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"
#include "abti_thread_htable.h"


/** @defgroup MUTEX Mutex
 * Mutex is a synchronization method to support mutual exclusion between ULTs.
 * When more than one ULT competes for locking the same mutex, only one ULT is
 * guaranteed to lock the mutex.  Other ULTs are blocked and wait until the ULT
 * which locked the mutex unlocks it.  When the mutex is unlocked, another ULT
 * is able to lock the mutex again.
 *
 * The mutex is basically intended to be used by ULTs but it can also be used
 * by tasklets or external threads.  In that case, the mutex will behave like
 * a spinlock.
 */

/**
 * @ingroup MUTEX
 * @brief   Create a new mutex.
 *
 * \c ABT_mutex_create() creates a new mutex object with default attributes and
 * returns its handle through \c newmutex.  To set different attributes, please
 * use \c ABT_mutex_create_with_attr().  If an error occurs in this routine,
 * a non-zero error code will be returned and \c newmutex will be set to
 * \c ABT_MUTEX_NULL.
 *
 * @param[out] newmutex  handle to a new mutex
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_mutex_create(ABT_mutex *newmutex)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_mutex *p_newmutex;

    p_newmutex = (ABTI_mutex *)ABTU_calloc(1, sizeof(ABTI_mutex));
    ABTI_mutex_init(p_newmutex);

    /* Return value */
    *newmutex = ABTI_mutex_get_handle(p_newmutex);

    return abt_errno;
}

/**
 * @ingroup MUTEX
 * @brief   Create a new mutex with attributes.
 *
 * \c ABT_mutex_create_with_attr() creates a new mutex object having attributes
 * passed by \c attr and returns its handle through \c newmutex.  Note that
 * \c ABT_mutex_create() can be used to create a mutex with default attributes.
 *
 * If an error occurs in this routine, a non-zero error code will be returned
 * and \c newmutex will be set to \c ABT_MUTEX_NULL.
 *
 * @param[in]  attr      handle to the mutex attribute object
 * @param[out] newmutex  handle to a new mutex
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_mutex_create_with_attr(ABT_mutex_attr attr, ABT_mutex *newmutex)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_mutex_attr *p_attr = ABTI_mutex_attr_get_ptr(attr);
    ABTI_CHECK_NULL_MUTEX_ATTR_PTR(p_attr);
    ABTI_mutex *p_newmutex;

    p_newmutex = (ABTI_mutex *)ABTU_malloc(sizeof(ABTI_mutex));
    ABTI_mutex_init(p_newmutex);
    ABTI_mutex_attr_copy(&p_newmutex->attr, p_attr);

    /* Return value */
    *newmutex = ABTI_mutex_get_handle(p_newmutex);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

/**
 * @ingroup MUTEX
 * @brief   Free the mutex object.
 *
 * \c ABT_mutex_free() deallocates the memory used for the mutex object
 * associated with the handle \c mutex.  If it is successfully processed,
 * \c mutex is set to \c ABT_MUTEX_NULL.
 *
 * Using the mutex handle after calling \c ABT_mutex_free() may cause
 * undefined behavior.
 *
 * @param[in,out] mutex  handle to the mutex
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_mutex_free(ABT_mutex *mutex)
{
    int abt_errno = ABT_SUCCESS;
    ABT_mutex h_mutex = *mutex;
    ABTI_mutex *p_mutex = ABTI_mutex_get_ptr(h_mutex);
    ABTI_CHECK_NULL_MUTEX_PTR(p_mutex);

    ABTI_mutex_fini(p_mutex);
    ABTU_free(p_mutex);

    /* Return value */
    *mutex = ABT_MUTEX_NULL;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

/**
 * @ingroup MUTEX
 * @brief   Lock the mutex.
 *
 * \c ABT_mutex_lock() locks the mutex \c mutex.  If this routine successfully
 * returns, the caller work unit acquires the mutex.  If the mutex has already
 * been locked, the caller will be blocked until the mutex becomes available.
 * When the caller is a ULT and is blocked, the context is switched to the
 * scheduler of the associated ES to make progress of other work units.
 *
 * The mutex can be used by any work units, but tasklets are discouraged to use
 * the mutex because any blocking calls like \c ABT_mutex_lock() may block the
 * associated ES and prevent other work units from being scheduled on the ES.
 *
 * @param[in] mutex  handle to the mutex
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_mutex_lock(ABT_mutex mutex)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_mutex *p_mutex = ABTI_mutex_get_ptr(mutex);
    ABTI_CHECK_NULL_MUTEX_PTR(p_mutex);

    if (p_mutex->attr.attrs == ABTI_MUTEX_ATTR_NONE) {
        /* default attributes */
        ABTI_mutex_lock(p_mutex);

    } else if (p_mutex->attr.attrs & ABTI_MUTEX_ATTR_RECURSIVE) {
        /* recursive mutex */
        ABTI_unit *p_self = ABTI_self_get_unit();
        if (p_self != p_mutex->attr.p_owner) {
            ABTI_mutex_lock(p_mutex);
            p_mutex->attr.p_owner = p_self;
            ABTI_ASSERT(p_mutex->attr.nesting_cnt == 0);
        } else {
            p_mutex->attr.nesting_cnt++;
        }

    } else {
        /* unknown attributes */
        ABTI_mutex_lock(p_mutex);
    }

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

static inline
void ABTI_mutex_lock_low(ABTI_mutex *p_mutex)
{
#ifdef ABT_CONFIG_USE_SIMPLE_MUTEX
    ABT_unit_type type;
    ABT_self_get_type(&type);
    if (type == ABT_UNIT_TYPE_THREAD) {
        LOG_EVENT("%p: lock_low - try\n", p_mutex);
        while (ABTD_atomic_cas_uint32(&p_mutex->val, 0, 1) != 0) {
            ABT_thread_yield();
        }
        LOG_EVENT("%p: lock_low - acquired\n", p_mutex);
    } else {
        ABTI_mutex_spinlock(p_mutex);
    }
#else
    int abt_errno;
    ABT_unit_type type;

    /* Only ULTs can yield when the mutex has been locked. For others,
     * just call mutex_spinlock. */
    ABT_self_get_type(&type);
    if (type == ABT_UNIT_TYPE_THREAD) {
        LOG_EVENT("%p: lock_low - try\n", p_mutex);
        int c;

        /* If other ULTs associated with the same ES are waiting on the
         * low-mutex queue, we give the header ULT a chance to try to get
         * the mutex by context switching to it. */
        ABTI_thread_htable *p_htable = p_mutex->p_htable;
        ABTI_thread *p_self = ABTI_local_get_thread();
        ABTI_xstream *p_xstream = p_self->p_last_xstream;
        int rank = (int)p_xstream->rank;
        ABTI_thread_queue *p_queue = &p_htable->queue[rank];
        if (p_queue->low_num_threads > 0) {
            ABT_bool ret = ABTI_thread_htable_switch_low(p_queue, p_self, p_htable);
            if (ret == ABT_TRUE) {
                /* This ULT became a waiter in the mutex queue */
                goto check_handover;
            }
        }

        if ((c = ABTD_atomic_cas_uint32(&p_mutex->val, 0, 1)) != 0) {
            if (c != 2) {
                c = ABTD_atomic_exchange_uint32(&p_mutex->val, 2);
            }
            while (c != 0) {
                ABTI_mutex_wait_low(p_mutex, 2);

  check_handover:
                /* If the mutex has been handed over to the current ULT from
                 * other ULT on the same ES, we don't need to change the mutex
                 * state. */
                if (p_mutex->p_handover) {
                    if (p_self == p_mutex->p_handover) {
                        p_mutex->p_handover = NULL;
                        p_mutex->val = 2;

                        /* Push the previous ULT to its pool */
                        ABTI_thread *p_giver = p_mutex->p_giver;
                        p_giver->state = ABT_THREAD_STATE_READY;
                        ABTI_POOL_PUSH(p_giver->p_pool, p_giver->unit,
                                       p_self->p_last_xstream);
                        break;
                    }
                }

                c = ABTD_atomic_exchange_uint32(&p_mutex->val, 2);
            }
        }
        LOG_EVENT("%p: lock_low - acquired\n", p_mutex);
    } else {
        ABTI_mutex_spinlock(p_mutex);
    }

  fn_exit:
    return ;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
#endif
}

/**
 * @ingroup MUTEX
 * @brief   Lock the mutex with low priority.
 *
 * \c ABT_mutex_lock_low() locks the mutex with low priority, while
 * \c ABT_mutex_lock() does with high priority.  Apart from the priority,
 * other semantics of \c ABT_mutex_lock_low() are the same as those of
 * \c ABT_mutex_lock().
 *
 * @param[in] mutex  handle to the mutex
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_mutex_lock_low(ABT_mutex mutex)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_mutex *p_mutex = ABTI_mutex_get_ptr(mutex);
    ABTI_CHECK_NULL_MUTEX_PTR(p_mutex);

    if (p_mutex->attr.attrs == ABTI_MUTEX_ATTR_NONE) {
        /* default attributes */
        ABTI_mutex_lock_low(p_mutex);

    } else if (p_mutex->attr.attrs & ABTI_MUTEX_ATTR_RECURSIVE) {
        /* recursive mutex */
        ABTI_unit *p_self = ABTI_self_get_unit();
        if (p_self != p_mutex->attr.p_owner) {
            ABTI_mutex_lock_low(p_mutex);
            p_mutex->attr.p_owner = p_self;
            ABTI_ASSERT(p_mutex->attr.nesting_cnt == 0);
        } else {
            p_mutex->attr.nesting_cnt++;
        }

    } else {
        /* unknown attributes */
        ABTI_mutex_lock_low(p_mutex);
    }

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

int ABT_mutex_lock_high(ABT_mutex mutex)
{
    return ABT_mutex_lock(mutex);
}

/**
 * @ingroup MUTEX
 * @brief   Attempt to lock a mutex without blocking.
 *
 * \c ABT_mutex_trylock() attempts to lock the mutex \c mutex without blocking
 * the caller work unit.  If this routine successfully returns, the caller
 * acquires the mutex.
 *
 * If the mutex has already been locked and there happens no error,
 * \c ABT_ERR_MUTEX_LOCKED will be returned immediately without blocking
 * the caller.
 *
 * @param[in] mutex  handle to the mutex
 * @return Error code
 * @retval ABT_SUCCESS          on success
 * @retval ABT_ERR_MUTEX_LOCKED when mutex has already been locked
 */
int ABT_mutex_trylock(ABT_mutex mutex)
{
    int abt_errno;
    ABTI_mutex *p_mutex = ABTI_mutex_get_ptr(mutex);
    ABTI_CHECK_NULL_MUTEX_PTR(p_mutex);

    if (p_mutex->attr.attrs == ABTI_MUTEX_ATTR_NONE) {
        /* default attributes */
        abt_errno = ABTI_mutex_trylock(p_mutex);

    } else if (p_mutex->attr.attrs & ABTI_MUTEX_ATTR_RECURSIVE) {
        /* recursive mutex */
        ABTI_unit *p_self = ABTI_self_get_unit();
        if (p_self != p_mutex->attr.p_owner) {
            abt_errno = ABTI_mutex_trylock(p_mutex);
            if (abt_errno == ABT_SUCCESS) {
                p_mutex->attr.p_owner = p_self;
                ABTI_ASSERT(p_mutex->attr.nesting_cnt == 0);
            }
        } else {
            p_mutex->attr.nesting_cnt++;
            abt_errno = ABT_SUCCESS;
        }

    } else {
        /* unknown attributes */
        abt_errno = ABTI_mutex_trylock(p_mutex);
    }

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

/**
 * @ingroup MUTEX
 * @brief   Lock the mutex without context switch.
 *
 * \c ABT_mutex_spinlock() locks the mutex without context switch.  If this
 * routine successfully returns, the caller work unit acquires the mutex.
 * If the mutex has already been locked, the caller will be blocked until
 * the mutex becomes available.  Unlike \c ABT_mutex_lock(), the ULT calling
 * this routine continuously tries to lock the mutex without context switch.
 *
 * @param[in] mutex  handle to the mutex
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_mutex_spinlock(ABT_mutex mutex)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_mutex *p_mutex = ABTI_mutex_get_ptr(mutex);
    ABTI_CHECK_NULL_MUTEX_PTR(p_mutex);

    if (p_mutex->attr.attrs == ABTI_MUTEX_ATTR_NONE) {
        /* default attributes */
        ABTI_mutex_spinlock(p_mutex);

    } else if (p_mutex->attr.attrs & ABTI_MUTEX_ATTR_RECURSIVE) {
        /* recursive mutex */
        ABTI_unit *p_self = ABTI_self_get_unit();
        if (p_self != p_mutex->attr.p_owner) {
            ABTI_mutex_spinlock(p_mutex);
            p_mutex->attr.p_owner = p_self;
            ABTI_ASSERT(p_mutex->attr.nesting_cnt == 0);
        } else {
            p_mutex->attr.nesting_cnt++;
        }

    } else {
        /* unknown attributes */
        ABTI_mutex_spinlock(p_mutex);
    }

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

/**
 * @ingroup MUTEX
 * @brief   Unlock the mutex.
 *
 * \c ABT_mutex_unlock() unlocks the mutex \c mutex.  If the caller locked the
 * mutex, this routine unlocks the mutex.  However, if the caller did not lock
 * the mutex, this routine may result in undefined behavior.
 *
 * @param[in] mutex  handle to the mutex
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_mutex_unlock(ABT_mutex mutex)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_mutex *p_mutex = ABTI_mutex_get_ptr(mutex);
    ABTI_CHECK_NULL_MUTEX_PTR(p_mutex);

    if (p_mutex->attr.attrs == ABTI_MUTEX_ATTR_NONE) {
        /* default attributes */
        ABTI_mutex_unlock(p_mutex);

    } else if (p_mutex->attr.attrs & ABTI_MUTEX_ATTR_RECURSIVE) {
        /* recursive mutex */
        ABTI_unit *p_self = ABTI_self_get_unit();
        ABTI_CHECK_TRUE(p_self == p_mutex->attr.p_owner, ABT_ERR_INV_THREAD);
        if (p_mutex->attr.nesting_cnt == 0) {
            p_mutex->attr.p_owner = NULL;
            ABTI_mutex_unlock(p_mutex);
        } else {
            p_mutex->attr.nesting_cnt--;
        }

    } else {
        /* unknown attributes */
        ABTI_mutex_unlock(p_mutex);
    }

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

/* Hand over the mutex to other ULT on the same ES */
static inline
int ABTI_mutex_unlock_se(ABTI_mutex *p_mutex)
{
    int abt_errno = ABT_SUCCESS;

#ifdef ABT_CONFIG_USE_SIMPLE_MUTEX
    ABTD_atomic_mem_barrier();
    *(volatile uint32_t *)&p_mutex->val = 0;
    LOG_EVENT("%p: unlock_se\n", p_mutex);
    ABTI_thread_yield(ABTI_local_get_thread());
#else
    int i;
    ABTI_xstream *p_xstream;
    ABTI_thread *p_next = NULL;
    ABTI_thread *p_thread;
    ABTI_thread_queue *p_queue;

    /* Unlock the mutex */
    /* If p_mutex->val is 1 before decreasing it, it means there is no any
     * waiter in the mutex queue.  We can just return. */
    if (ABTD_atomic_fetch_sub_uint32(&p_mutex->val, 1) == 1) {
        LOG_EVENT("%p: unlock_se\n", p_mutex);
        ABTI_thread_yield(ABTI_local_get_thread());
        return abt_errno;
    }

    /* There are ULTs waiting in the mutex queue */
    ABTI_thread_htable *p_htable = p_mutex->p_htable;

    p_thread = ABTI_local_get_thread();
    p_xstream = p_thread->p_last_xstream;
    ABTI_ASSERT(p_xstream == ABTI_local_get_xstream());
    i = (int)p_xstream->rank;
    p_queue = &p_htable->queue[i];

 check_cond:
    /* Check whether the mutex handover is possible */
    if (p_queue->num_handovers >= p_mutex->attr.max_handovers) {
        ABTI_PTR_UNLOCK(&p_mutex->val);
        LOG_EVENT("%p: unlock_se\n", p_mutex);
        ABTI_mutex_wake_de(p_mutex);
        p_queue->num_handovers = 0;
        ABTI_thread_yield(p_thread);
        return abt_errno;
    }

    /* Hand over the mutex to high-priority ULTs */
    if (p_queue->num_threads <= 1) {
        if(p_htable->h_list != NULL) {
            ABTI_PTR_UNLOCK(&p_mutex->val);
            LOG_EVENT("%p: unlock_se\n", p_mutex);
            ABTI_mutex_wake_de(p_mutex);
            ABTI_thread_yield(p_thread);
            return abt_errno;
        }
    } else {
        p_next = ABTI_thread_htable_pop(p_htable, p_queue);
        if (p_next == NULL) goto check_cond;
        else goto handover;
    }

    /* When we don't have high-priority ULTs and other ESs don't either,
     * we hand over the mutex to low-priority ULTs. */
    if (p_queue->low_num_threads <= 1) {
        ABTI_PTR_UNLOCK(&p_mutex->val);
        LOG_EVENT("%p: unlock_se\n", p_mutex);
        ABTI_mutex_wake_de(p_mutex);
        ABTI_thread_yield(p_thread);
        return abt_errno;
    } else {
        p_next = ABTI_thread_htable_pop_low(p_htable, p_queue);
        if (p_next == NULL) goto check_cond;
    }

  handover:
    /* We don't push p_thread to the pool. Instead, we will yield_to p_thread
     * directly at the end of this function. */
    p_queue->num_handovers++;

    /* We are handing over the mutex */
    p_mutex->p_handover = p_next;
    p_mutex->p_giver = p_thread;

    LOG_EVENT("%p: handover -> U%" PRIu64 "\n",
              p_mutex, ABTI_thread_get_id(p_next));

    /* yield_to the next ULT */
    while (*(volatile uint32_t *)(&p_next->request) & ABTI_THREAD_REQ_BLOCK) {}
    ABTI_pool_dec_num_blocked(p_next->p_pool);
    ABTI_local_set_thread(p_next);
    p_next->state = ABT_THREAD_STATE_RUNNING;
    ABTD_thread_context_switch(&p_thread->ctx, &p_next->ctx);
#endif

    return abt_errno;
}

/**
 * @ingroup MUTEX
 * @brief   Hand over the mutex within the ES.
 *
 * \c ABT_mutex_unlock_se() fisrt tries to hand over the mutex to a ULT, which
 * is waiting for this mutex and is running on the same ES as the caller.  If
 * no ULT on the same ES is waiting, it unlocks the mutex like
 * \c ABT_mutex_unlock().
 *
 * If the caller ULT locked the mutex, this routine unlocks the mutex.
 * However, if the caller ULT did not lock the mutex, this routine may result
 * in undefined behavior.
 *
 * @param[in] mutex  handle to the mutex
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_mutex_unlock_se(ABT_mutex mutex)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_mutex *p_mutex = ABTI_mutex_get_ptr(mutex);
    ABTI_CHECK_NULL_MUTEX_PTR(p_mutex);

    if (p_mutex->attr.attrs == ABTI_MUTEX_ATTR_NONE) {
        /* default attributes */
        ABTI_mutex_unlock_se(p_mutex);

    } else if (p_mutex->attr.attrs & ABTI_MUTEX_ATTR_RECURSIVE) {
        /* recursive mutex */
        ABTI_unit *p_self = ABTI_self_get_unit();
        ABTI_CHECK_TRUE(p_self == p_mutex->attr.p_owner, ABT_ERR_INV_THREAD);
        if (p_mutex->attr.nesting_cnt == 0) {
            p_mutex->attr.p_owner = NULL;
            ABTI_mutex_unlock_se(p_mutex);
        } else {
            p_mutex->attr.nesting_cnt--;
        }

    } else {
        /* unknown attributes */
        ABTI_mutex_unlock_se(p_mutex);
    }

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

int ABT_mutex_unlock_de(ABT_mutex mutex)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_mutex *p_mutex = ABTI_mutex_get_ptr(mutex);
    ABTI_CHECK_NULL_MUTEX_PTR(p_mutex);

    ABTI_mutex_unlock(p_mutex);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

/**
 * @ingroup MUTEX
 * @brief   Compare two mutex handles for equality.
 *
 * \c ABT_mutex_equal() compares two mutex handles for equality.  If two
 * handles are associated with the same mutex object, \c result will be set to
 * \c ABT_TRUE.  Otherwise, \c result will be set to \c ABT_FALSE.
 *
 * @param[in]  mutex1  handle to the mutex 1
 * @param[in]  mutex2  handle to the mutex 2
 * @param[out] result  comparison result (<tt>ABT_TRUE</tt>: same,
 *                     <tt>ABT_FALSE</tt>: not same)
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_mutex_equal(ABT_mutex mutex1, ABT_mutex mutex2, ABT_bool *result)
{
    ABTI_mutex *p_mutex1 = ABTI_mutex_get_ptr(mutex1);
    ABTI_mutex *p_mutex2 = ABTI_mutex_get_ptr(mutex2);
    *result = ABTI_mutex_equal(p_mutex1, p_mutex2);
    return ABT_SUCCESS;
}


void ABTI_mutex_wait(ABTI_mutex *p_mutex, int val)
{
    ABTI_thread_htable *p_htable = p_mutex->p_htable;
    ABTI_thread *p_self = ABTI_local_get_thread();
    ABTI_xstream *p_xstream = p_self->p_last_xstream;

    int rank = (int)p_xstream->rank;
    ABTI_ASSERT(rank < p_htable->num_rows);
    ABTI_thread_queue *p_queue = &p_htable->queue[rank];

#if 0
    if (p_queue->num_threads > 0) {
        /* Push the current ULT to the queue */
        if (ABTI_thread_htable_add(p_htable, rank, p_self) == ABT_TRUE) {
            /* If p_queue is not linked in the list, we should correct it. */
            if (p_queue->p_h_next == NULL) {
                ABTI_THREAD_HTABLE_LOCK(p_htable->mutex);
                ABTI_thread_htable_add_h_node(p_htable, p_queue);
                ABTI_THREAD_HTABLE_UNLOCK(p_htable->mutex);return;
            }

            /* Suspend the current ULT */
            ABTI_thread_suspend(p_self);

            return;
        }
    }
#endif

    ABTI_THREAD_HTABLE_LOCK(p_htable->mutex);

    if (p_mutex->val != val) {
        ABTI_THREAD_HTABLE_UNLOCK(p_htable->mutex);
        return;
    }

    if (p_queue->p_h_next == NULL) {
        ABTI_thread_htable_add_h_node(p_htable, p_queue);
    }

    /* Change the ULT's state to BLOCKED */
    ABTI_thread_set_blocked(p_self);

    /* Push the current ULT to the queue */
    ABTI_thread_htable_push(p_htable, rank, p_self);

    /* Unlock */
    ABTI_THREAD_HTABLE_UNLOCK(p_htable->mutex);

    /* Suspend the current ULT */
    ABTI_thread_suspend(p_self);
}

void ABTI_mutex_wait_low(ABTI_mutex *p_mutex, int val)
{
    ABTI_thread_htable *p_htable = p_mutex->p_htable;
    ABTI_thread *p_self = ABTI_local_get_thread();
    ABTI_xstream *p_xstream = p_self->p_last_xstream;

    int rank = (int)p_xstream->rank;
    ABTI_ASSERT(rank < p_htable->num_rows);
    ABTI_thread_queue *p_queue = &p_htable->queue[rank];

#if 0
    if (p_queue->low_num_threads > 0) {
        /* Push the current ULT to the queue */
        if (ABTI_thread_htable_add_low(p_htable, rank, p_self) == ABT_TRUE) {
            /* If p_queue is not linked in the list, we should correct it. */
            if (p_queue->p_l_next == NULL) {
                ABTI_THREAD_HTABLE_LOCK(p_htable->mutex);
                ABTI_thread_htable_add_l_node(p_htable, p_queue);
                ABTI_THREAD_HTABLE_UNLOCK(p_htable->mutex);
            }

            /* Suspend the current ULT */
            ABTI_thread_suspend(p_self);

            return;
        }
    }
#endif

    ABTI_THREAD_HTABLE_LOCK(p_htable->mutex);

    if (p_mutex->val != val) {
        ABTI_THREAD_HTABLE_UNLOCK(p_htable->mutex);
        return;
    }

    if (p_queue->p_l_next == NULL) {
        ABTI_thread_htable_add_l_node(p_htable, p_queue);
    }

    /* Change the ULT's state to BLOCKED */
    ABTI_thread_set_blocked(p_self);

    /* Push the current ULT to the queue */
    ABTI_thread_htable_push_low(p_htable, rank, p_self);

    /* Unlock */
    ABTI_THREAD_HTABLE_UNLOCK(p_htable->mutex);

    /* Suspend the current ULT */
    ABTI_thread_suspend(p_self);
}

void ABTI_mutex_wake_de(ABTI_mutex *p_mutex)
{
    int n;
    ABTI_thread *p_thread;
    ABTI_thread_htable *p_htable = p_mutex->p_htable;
    int num = p_mutex->attr.max_wakeups;
    ABTI_thread_queue *p_start, *p_curr;

    /* Wake up num ULTs in a round-robin manner */
    for (n = 0; n < num; n++) {
        p_thread = NULL;

        ABTI_THREAD_HTABLE_LOCK(p_htable->mutex);

        if (p_htable->num_elems == 0) {
            ABTI_THREAD_HTABLE_UNLOCK(p_htable->mutex);
            break;
        }

        /* Wake up the high-priority ULTs */
        p_start = p_htable->h_list;
        for (p_curr = p_start; p_curr; ) {
            p_thread = ABTI_thread_htable_pop(p_htable, p_curr);
            if (p_curr->num_threads == 0) {
                ABTI_thread_htable_del_h_head(p_htable);
            } else {
                p_htable->h_list = p_curr->p_h_next;
            }
            if (p_thread != NULL) goto done;
            p_curr = p_htable->h_list;
            if (p_curr == p_start) break;
        }

        /* Wake up the low-priority ULTs */
        p_start = p_htable->l_list;
        for (p_curr = p_start; p_curr; ) {
            p_thread = ABTI_thread_htable_pop_low(p_htable, p_curr);
            if (p_curr->low_num_threads == 0) {
                ABTI_thread_htable_del_l_head(p_htable);
            } else {
                p_htable->l_list = p_curr->p_l_next;
            }
            if (p_thread != NULL) goto done;
            p_curr = p_htable->l_list;
            if (p_curr == p_start) break;
        }

        /* Nothing to wake up */
        ABTI_THREAD_HTABLE_UNLOCK(p_htable->mutex);
        LOG_EVENT("%p: nothing to wake up\n", p_mutex);
        break;

  done:
        ABTI_THREAD_HTABLE_UNLOCK(p_htable->mutex);

        /* Push p_thread to the scheduler's pool */
        LOG_EVENT("%p: wake up U%" PRIu64 ":E%" PRIu64 "\n", p_mutex,
                  ABTI_thread_get_id(p_thread),
                  ABTI_thread_get_xstream_rank(p_thread));
        ABTI_thread_set_ready(p_thread);
    }
}

