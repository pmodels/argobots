/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#ifndef MUTEX_H_INCLUDED
#define MUTEX_H_INCLUDED

/* Inlined functions for Mutex */

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
}

static inline
void ABTI_mutex_spinlock(ABTI_mutex *p_mutex)
{
    while (ABTD_atomic_cas_uint32(&p_mutex->val, 0, 1) != 0) {
    }
}

static inline
void ABTI_mutex_lock(ABTI_mutex *p_mutex)
{
    ABT_unit_type type;

    /* Only ULTs can yield when the mutex has been locked. For others,
     * just call mutex_spinlock. */
    ABT_self_get_type(&type);
    if (type == ABT_UNIT_TYPE_THREAD) {
        while (ABTD_atomic_cas_uint32(&p_mutex->val, 0, 1) != 0) {
            ABT_thread_yield();
        }
    } else {
        ABTI_mutex_spinlock(p_mutex);
    }
}

static inline
int ABTI_mutex_trylock(ABTI_mutex *p_mutex)
{
    if (ABTD_atomic_cas_uint32(&p_mutex->val, 0, 1) != 0) {
        return ABT_ERR_MUTEX_LOCKED;
    }
    return ABT_SUCCESS;
}

static inline
void ABTI_mutex_unlock(ABTI_mutex *p_mutex)
{
    ABTD_atomic_mem_barrier();
    *(volatile uint32_t *)&p_mutex->val = 0;
}

static inline
ABT_bool ABTI_mutex_equal(ABTI_mutex *p_mutex1, ABTI_mutex *p_mutex2)
{
    return (p_mutex1 == p_mutex2) ? ABT_TRUE : ABT_FALSE;
}

#endif /* MUTEX_H_INCLUDED */

