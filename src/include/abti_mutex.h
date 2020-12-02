/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#ifndef ABTI_MUTEX_H_INCLUDED
#define ABTI_MUTEX_H_INCLUDED

static inline ABTI_mutex *ABTI_mutex_get_ptr(ABT_mutex mutex)
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

static inline ABT_mutex ABTI_mutex_get_handle(ABTI_mutex *p_mutex)
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

ABTU_ret_err static inline int ABTI_mutex_init(ABTI_mutex *p_mutex)
{
    ABTD_atomic_relaxed_store_uint32(&p_mutex->val, 0);
    p_mutex->attr.attrs = ABTI_MUTEX_ATTR_NONE;
    return ABT_SUCCESS;
}

static inline void ABTI_mutex_spinlock(ABTI_mutex *p_mutex)
{
    /* ABTI_spinlock_ functions cannot be used since p_mutex->val can take
     * other values (i.e., not UNLOCKED nor LOCKED.) */
    while (!ABTD_atomic_bool_cas_weak_uint32(&p_mutex->val, 0, 1)) {
        while (ABTD_atomic_acquire_load_uint32(&p_mutex->val) != 0)
            ;
    }
    LOG_DEBUG("%p: spinlock\n", p_mutex);
}

static inline void ABTI_mutex_fini(ABTI_mutex *p_mutex)
{
    ABTI_mutex_spinlock(p_mutex);
}

static inline void ABTI_mutex_lock(ABTI_local **pp_local, ABTI_mutex *p_mutex)
{
    ABTI_xstream *p_local_xstream = ABTI_local_get_xstream_or_null(*pp_local);
    if (ABTI_IS_EXT_THREAD_ENABLED && !p_local_xstream) {
        ABTI_mutex_spinlock(p_mutex);
        return;
    }
    ABTI_ythread *p_ythread =
        ABTI_thread_get_ythread_or_null(p_local_xstream->p_thread);
    if (!p_ythread) {
        ABTI_mutex_spinlock(p_mutex);
        return;
    }
    LOG_DEBUG("%p: lock - try\n", p_mutex);
    while (!ABTD_atomic_bool_cas_strong_uint32(&p_mutex->val, 0, 1)) {
        ABTI_ythread_yield(&p_local_xstream, p_ythread,
                           ABT_SYNC_EVENT_TYPE_MUTEX, (void *)p_mutex);
        *pp_local = ABTI_xstream_get_local(p_local_xstream);
    }
    LOG_DEBUG("%p: lock - acquired\n", p_mutex);
}

static inline int ABTI_mutex_trylock(ABTI_mutex *p_mutex)
{
    if (!ABTD_atomic_bool_cas_strong_uint32(&p_mutex->val, 0, 1)) {
        return ABT_ERR_MUTEX_LOCKED;
    }
    return ABT_SUCCESS;
}

static inline void ABTI_mutex_unlock(ABTI_local *p_local, ABTI_mutex *p_mutex)
{
    ABTD_atomic_mem_barrier();
    ABTD_atomic_release_store_uint32(&p_mutex->val, 0);
    LOG_DEBUG("%p: unlock w/o wake\n", p_mutex);
}

#endif /* ABTI_MUTEX_H_INCLUDED */
