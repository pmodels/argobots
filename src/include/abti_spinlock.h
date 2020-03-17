/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#ifndef ABTI_SPINLOCK_H_INCLUDED
#define ABTI_SPINLOCK_H_INCLUDED

struct ABTI_spinlock {
    uint8_t val;
};

#define ABTI_SPINLOCK_STATIC_INITIALIZER()                                     \
    {                                                                          \
        0                                                                      \
    }

static inline void ABTI_spinlock_clear(ABTI_spinlock *p_lock)
{
    p_lock->val = 0;
}

static inline void ABTI_spinlock_acquire(ABTI_spinlock *p_lock)
{
    while (ABTD_atomic_test_and_set_uint8((uint8_t *)&p_lock->val)) {
        while (ABTD_atomic_acquire_load_uint8((uint8_t *)&p_lock->val) != 0)
            ;
    }
}

static inline void ABTI_spinlock_release(ABTI_spinlock *p_lock)
{
    ABTD_atomic_release_clear_uint8((uint8_t *)&p_lock->val);
}

#endif /* ABTI_SPINLOCK_H_INCLUDED */
