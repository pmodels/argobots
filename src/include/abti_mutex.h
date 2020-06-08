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

static inline void ABTI_mutex_init(ABTI_mutex *p_mutex)
{
    ABTD_atomic_relaxed_store_uint32(&p_mutex->val, 0);
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
static inline void ABTI_mutex_fini(ABTI_mutex *p_mutex)
{
    ABTI_thread_htable_free(p_mutex->p_htable);
}
#endif

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

static inline void ABTI_mutex_lock(ABTI_xstream **pp_local_xstream,
                                   ABTI_mutex *p_mutex)
{
#ifdef ABT_CONFIG_USE_SIMPLE_MUTEX
    ABTI_xstream *p_local_xstream = *pp_local_xstream;
    ABTI_unit_type type = ABTI_self_get_type(p_local_xstream);
    if (ABTI_unit_type_is_thread(type)) {
        LOG_DEBUG("%p: lock - try\n", p_mutex);
        while (!ABTD_atomic_bool_cas_weak_uint32(&p_mutex->val, 0, 1)) {
            ABTI_thread_yield(pp_local_xstream,
                              ABTI_unit_get_thread(p_local_xstream->p_unit));
            p_local_xstream = *pp_local_xstream;
        }
        LOG_DEBUG("%p: lock - acquired\n", p_mutex);
    } else {
        ABTI_mutex_spinlock(p_mutex);
    }
#else
    int abt_errno;
    ABTI_unit_type type = ABTI_self_get_type(*pp_local_xstream);

    /* Only ULTs can yield when the mutex has been locked. For others,
     * just call mutex_spinlock. */
    if (ABTI_unit_type_is_thread(type)) {
        LOG_DEBUG("%p: lock - try\n", p_mutex);
        int c;
        if ((c = ABTD_atomic_val_cas_strong_uint32(&p_mutex->val, 0, 1)) != 0) {
            if (c != 2) {
                c = ABTD_atomic_exchange_uint32(&p_mutex->val, 2);
            }
            while (c != 0) {
                ABTI_mutex_wait(pp_local_xstream, p_mutex, 2);

                /* If the mutex has been handed over to the current ULT from
                 * other ULT on the same ES, we don't need to change the mutex
                 * state. */
                if (p_mutex->p_handover) {
                    ABTI_thread *p_self =
                        ABTI_unit_get_thread((*pp_local_xstream)->p_unit);
                    if (p_self == p_mutex->p_handover) {
                        p_mutex->p_handover = NULL;
                        ABTD_atomic_release_store_uint32(&p_mutex->val, 2);

                        /* Push the previous ULT to its pool */
                        ABTI_thread *p_giver = p_mutex->p_giver;
                        ABTD_atomic_release_store_int(&p_giver->unit_def.state,
                                                      ABTI_UNIT_STATE_READY);
                        ABTI_POOL_PUSH(p_giver->unit_def.p_pool,
                                       p_giver->unit_def.unit,
                                       ABTI_self_get_native_thread_id(
                                           *pp_local_xstream));
                        break;
                    }
                }

                c = ABTD_atomic_exchange_uint32(&p_mutex->val, 2);
            }
        }
        LOG_DEBUG("%p: lock - acquired\n", p_mutex);
    } else {
        ABTI_mutex_spinlock(p_mutex);
    }

fn_exit:
    return;

fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
#endif
}

static inline int ABTI_mutex_trylock(ABTI_mutex *p_mutex)
{
    if (!ABTD_atomic_bool_cas_strong_uint32(&p_mutex->val, 0, 1)) {
        return ABT_ERR_MUTEX_LOCKED;
    }
    return ABT_SUCCESS;
}

static inline void ABTI_mutex_unlock(ABTI_xstream *p_local_xstream,
                                     ABTI_mutex *p_mutex)
{
#ifdef ABT_CONFIG_USE_SIMPLE_MUTEX
    ABTD_atomic_mem_barrier();
    ABTD_atomic_release_store_uint32(&p_mutex->val, 0);
    LOG_DEBUG("%p: unlock w/o wake\n", p_mutex);
#else
    if (ABTD_atomic_fetch_sub_uint32(&p_mutex->val, 1) != 1) {
        ABTD_atomic_release_store_uint32(&p_mutex->val, 0);
        LOG_DEBUG("%p: unlock with wake\n", p_mutex);
        ABTI_mutex_wake_de(p_local_xstream, p_mutex);
    } else {
        LOG_DEBUG("%p: unlock w/o wake\n", p_mutex);
    }
#endif
}

static inline ABT_bool ABTI_mutex_equal(ABTI_mutex *p_mutex1,
                                        ABTI_mutex *p_mutex2)
{
    return (p_mutex1 == p_mutex2) ? ABT_TRUE : ABT_FALSE;
}

#endif /* ABTI_MUTEX_H_INCLUDED */
