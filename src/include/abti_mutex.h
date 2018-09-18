/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#ifndef MUTEX_H_INCLUDED
#define MUTEX_H_INCLUDED

/* Inlined functions for Mutex */

#define ABTI_PTR_SPINLOCK(ptr)                          \
    while (ABTD_atomic_cas_uint32(ptr, 0, 1) != 0) {    \
        while (*(volatile uint32_t *)(ptr) != 0) {      \
        }                                               \
    }

#define ABTI_PTR_UNLOCK(ptr)                            \
    do {                                                \
        *(volatile uint32_t *)(ptr) = 0;                \
    } while (0)

#define ABTI_PTR_SPINLOCK_HIGH(ptr)                                 \
{                                                                   \
    uint64_t old_v = (uint64_t)1 << 32;                             \
    uint64_t new_v = ((uint64_t)1 << 32) | 1;                       \
    ptr[1] = 1;                                                     \
    uint64_t *v_ptr = (uint64_t *)ptr;                              \
    while (ABTD_atomic_cas_uint64(v_ptr, old_v, new_v) != old_v) {  \
        while (*(volatile uint32_t *)(&ptr[0]) != 0 ) {             \
        }                                                           \
        ptr[1] = 1;                                                 \
    }                                                               \
}

#define ABTI_PTR_UNLOCK_HIGH(ptr)                       \
    do {                                                \
        *(volatile uint64_t *)(ptr) = 0;                \
    } while (0)

#define ABTI_PTR_SPINLOCK_LOW(ptr)                      \
{                                                       \
    uint64_t *v_ptr = (uint64_t *)ptr;                  \
    while (ABTD_atomic_cas_uint64(v_ptr, 0, 1) != 0) {  \
        while (*(volatile uint32_t *)(&ptr[0]) != 0) {  \
        }                                               \
    }                                                   \
}

#define ABTI_PTR_UNLOCK_LOW(ptr)                        \
    do {                                                \
        ptr[0] = 0;                                     \
    } while (0)

/* Do not change the values of the constants. */

/* Mutex is unlocked. Anyone not taking a lock tries to change it with CAS. */
#define ABTI_MUTEX_UNLOCKED                   0
/* Mutex is locked. Since no threads are waiting, no need to wake them up.
 * When the lock has this value, the value might be rewritten by threads that
 * want to wait for this mutex with CAS. */
#define ABTI_MUTEX_LOCKED_NO_WAITING_THREADS  1
/* Mutex is locked. No threads wait for this mutex by adding themselves to
 * htable. This state is only updated by the owner of lock. */
#define ABTI_MUTEX_LOCKED_NEVER_WAIT          2
/* Mutex is locked and has waiting threads. This state is only updated by the
 * owner of lock. */
#define ABTI_MUTEX_LOCKED_WAITING_THREADS     3

static inline
ABTI_mutex *ABTI_mutex_get_ptr(ABT_mutex mutex)
{
#ifndef ABT_CONFIG_DISABLE_ERROR_CHECK
    ABTI_mutex *p_mutex;
    if (mutex == ABT_MUTEX_NULL) {
        p_mutex = NULL;
    } else {
        p_mutex = (ABTI_mutex *)mutex;
    }
    return p_mutex;
#else
    return (ABTI_mutex *)mutex;
#endif
}

static inline
ABT_mutex ABTI_mutex_get_handle(ABTI_mutex *p_mutex)
{
#ifndef ABT_CONFIG_DISABLE_ERROR_CHECK
    ABT_mutex h_mutex;
    if (p_mutex == NULL) {
        h_mutex = ABT_MUTEX_NULL;
    } else {
        h_mutex = (ABT_mutex)p_mutex;
    }
    return h_mutex;
#else
    return (ABT_mutex)p_mutex;
#endif
}

static inline
void ABTI_mutex_init(ABTI_mutex *p_mutex)
{
    p_mutex->val = 0;
    p_mutex->attr.attrs = ABTI_MUTEX_ATTR_NONE;
    p_mutex->attr.max_handovers = ABTI_global_get_mutex_max_handovers();
    p_mutex->attr.max_wakeups = ABTI_global_get_mutex_max_wakeups();
#ifndef ABT_CONFIG_USE_SIMPLE_MUTEX
    p_mutex->p_htable = ABTI_thread_htable_create(gp_ABTI_global->max_xstreams);
    p_mutex->p_handover = NULL;
    p_mutex->p_giver = NULL;
#endif
}

#ifdef ABT_CONFIG_USE_SIMPLE_MUTEX
#define ABTI_mutex_fini(p_mutex)
#else
static inline
void ABTI_mutex_fini(ABTI_mutex *p_mutex)
{
    ABTI_thread_htable_free(p_mutex->p_htable);
}
#endif

static inline
void ABTI_mutex_spinlock(ABTI_mutex *p_mutex)
{
    ABTI_PTR_SPINLOCK(&p_mutex->val);
    LOG_EVENT("%p: spinlock\n", p_mutex);
}

static inline
void ABTI_mutex_lock(ABTI_mutex *p_mutex)
{
#ifdef ABT_CONFIG_USE_SIMPLE_MUTEX
    ABT_unit_type type;
    ABT_self_get_type(&type);
    if (type == ABT_UNIT_TYPE_THREAD) {
        LOG_EVENT("%p: lock - try\n", p_mutex);
        while (ABTD_atomic_cas_uint32(&p_mutex->val, ABTI_MUTEX_UNLOCKED,
               ABTI_MUTEX_LOCKED_NO_WAITING_THREADS) != ABTI_MUTEX_UNLOCKED) {
            ABT_thread_yield();
        }
        LOG_EVENT("%p: lock - acquired\n", p_mutex);
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
        LOG_EVENT("%p: lock - try\n", p_mutex);
        int c = ABTD_atomic_cas_uint32(&p_mutex->val, ABTI_MUTEX_UNLOCKED,
                                       ABTI_MUTEX_LOCKED_NO_WAITING_THREADS);
        while (c != ABTI_MUTEX_UNLOCKED) {
            if (c == ABTI_MUTEX_LOCKED_NO_WAITING_THREADS) {
                /* If p_mutex->val is ABTI_MUTEX_LOCKED_NO_WAITING_THREADS,
                 * update it to ABTI_MUTEX_LOCKED_WAITING_THREADS since
                 * this thread becomes waiting. */
                c = ABTD_atomic_cas_uint32(&p_mutex->val,
                        ABTI_MUTEX_LOCKED_NO_WAITING_THREADS,
                        ABTI_MUTEX_LOCKED_WAITING_THREADS);
            }
            /* It waits for p_mutex using p_htable only if p_mutex->val
             * is ABTI_MUTEX_LOCKED_WAITING_THREADS. Otherwise, no one might
             * wake me up. */
            ABTI_mutex_wait(p_mutex, ABTI_MUTEX_LOCKED_WAITING_THREADS);
            /* If the mutex has been handed over to the current ULT from
             * other ULT on the same ES, we don't need to change the mutex
             * state. */
            ABTI_thread *p_self = ABTI_local_get_thread();
            if (p_self == p_mutex->p_handover) {
                /* If it is handed over, p_mutex is still locked,
                 * but p_mutex->p_htable->mutex is unlocked. */
                p_mutex->p_handover = NULL;
                ABTI_ASSERT(p_mutex->val == ABTI_MUTEX_LOCKED_NEVER_WAIT);
                /* Update p_mutex->val to ABTI_MUTEX_LOCKED_WAITING_THREADS
                 * because, though this thread is not waiting now, other threads
                 * might be still waiting. We do not need atomic operations
                 * since ABTI_MUTEX_LOCKED_NEVER_WAIT state is only modified
                 * by the owner of the lock. */
                p_mutex->val = ABTI_MUTEX_LOCKED_WAITING_THREADS;
                /* Push the previous ULT to its pool */
                ABTI_thread *p_giver = p_mutex->p_giver;
                p_giver->state = ABT_THREAD_STATE_READY;
                ABTI_POOL_PUSH(p_giver->p_pool, p_giver->unit,
                               p_self->p_last_xstream);
                /* When handover succeeds, p_mutex->val is still locked,
                 * and owned by this thread. */
                break;
            }
            /* If p_mutex->val is unlocked, take the lock. This time, it is
             * updated to ABTI_MUTEX_LOCKED_WAITING_THREADS because the thread
             * that woke up this thread might not have woken up all the sleeping
             * threads (because of max_wakeups, for instance.)
             */
            c = ABTD_atomic_cas_uint32(&p_mutex->val, ABTI_MUTEX_UNLOCKED,
                                       ABTI_MUTEX_LOCKED_WAITING_THREADS);
        }
        /* This thread successfully gets a lock. */
        LOG_EVENT("%p: lock - acquired\n", p_mutex);
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

static inline
int ABTI_mutex_trylock(ABTI_mutex *p_mutex)
{
    if (ABTD_atomic_cas_uint32(&p_mutex->val, ABTI_MUTEX_UNLOCKED,
        ABTI_MUTEX_LOCKED_NO_WAITING_THREADS) != ABTI_MUTEX_UNLOCKED) {
        return ABT_ERR_MUTEX_LOCKED;
    }
    return ABT_SUCCESS;
}

static inline
void ABTI_mutex_unlock(ABTI_mutex *p_mutex)
{
#ifdef ABT_CONFIG_USE_SIMPLE_MUTEX
    ABTD_atomic_mem_barrier();
    *(volatile uint32_t *)&p_mutex->val = ABTI_MUTEX_UNLOCKED;
    LOG_EVENT("%p: unlock w/o wake\n", p_mutex);
#else
    /* Here's a trick to unlock p_mutex->val if p_mutex is
     * ABTI_MUTEX_LOCKED_NO_WAITING_THREADS (=1). If it's not, p_mutex->lock
     * is not released. Note ABTI_MUTEX_LOCKED_NO_WAITING_THREADS is 1. */
    if (ABTD_atomic_fetch_sub_uint32(&p_mutex->val, 1)
        != ABTI_MUTEX_LOCKED_NO_WAITING_THREADS) {
        /* p_mutex->val was ABTI_MUTEX_LOCKED_WAITING_THREADS (=3),
         * so p_mutex->val becomes ABTI_MUTEX_LOCKED_NEVER_WAIT. */
        LOG_EVENT("%p: unlock with wake\n", p_mutex);
        ABTI_mutex_wake_de(p_mutex);
        /* p_mutex is unlocked after waking up threads. */
        ABTI_PTR_UNLOCK(&p_mutex->val);
    } else {
        LOG_EVENT("%p: unlock w/o wake\n", p_mutex);
    }
#endif
}

static inline
ABT_bool ABTI_mutex_equal(ABTI_mutex *p_mutex1, ABTI_mutex *p_mutex2)
{
    return (p_mutex1 == p_mutex2) ? ABT_TRUE : ABT_FALSE;
}

#endif /* MUTEX_H_INCLUDED */

