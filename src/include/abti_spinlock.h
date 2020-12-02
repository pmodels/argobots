/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#ifndef ABTI_SPINLOCK_H_INCLUDED
#define ABTI_SPINLOCK_H_INCLUDED

struct ABTI_spinlock {
    ABTD_atomic_bool val;
};

#define ABTI_SPINLOCK_STATIC_INITIALIZER()                                     \
    {                                                                          \
        ABTD_ATOMIC_BOOL_STATIC_INITIALIZER(0)                                 \
    }

static inline ABT_bool ABTI_spinlock_is_locked(const ABTI_spinlock *p_lock)
{
    return ABTD_atomic_acquire_load_bool(&p_lock->val);
}

static inline void ABTI_spinlock_clear(ABTI_spinlock *p_lock)
{
    ABTD_atomic_relaxed_clear_bool(&p_lock->val);
}

static inline void ABTI_spinlock_acquire(ABTI_spinlock *p_lock)
{
    while (ABTD_atomic_test_and_set_bool(&p_lock->val)) {
        while (ABTI_spinlock_is_locked(p_lock) != ABT_FALSE)
            ;
    }
}

/* Return ABT_FALSE if the lock is acquired. */
static inline ABT_bool ABTI_spinlock_try_acquire(ABTI_spinlock *p_lock)
{
    return ABTD_atomic_test_and_set_bool(&p_lock->val) ? ABT_TRUE : ABT_FALSE;
}

static inline void ABTI_spinlock_release(ABTI_spinlock *p_lock)
{
    ABTD_atomic_release_clear_bool(&p_lock->val);
}

#endif /* ABTI_SPINLOCK_H_INCLUDED */
