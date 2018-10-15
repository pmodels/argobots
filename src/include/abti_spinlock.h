/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#ifndef SPINLOCK_H_INCLUDED
#define SPINLOCK_H_INCLUDED

struct ABTI_spinlock {
    uint8_t val;
};

static inline void ABTI_spinlock_create(ABTI_spinlock *p_lock)
{
    p_lock->val = 0;
}

static inline void ABTI_spinlock_free(ABTI_spinlock *p_lock)
{
    ABTI_UNUSED(p_lock);
}

static inline void ABTI_spinlock_acquire(ABTI_spinlock *p_lock)
{
    while (ABTD_atomic_test_and_set_uint8((uint8_t *)&p_lock->val)) {
        while (ABTD_atomic_load_uint8((uint8_t *)&p_lock->val) != 0);
    }
}

static inline void ABTI_spinlock_release(ABTI_spinlock *p_lock)
{
    ABTD_atomic_clear_uint8((uint8_t *)&p_lock->val);
}

#endif /* SPINLOCK_H_INCLUDED */
