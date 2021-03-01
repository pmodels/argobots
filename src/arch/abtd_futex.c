/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"

#ifndef ABT_CONFIG_ACTIVE_WAIT_POLICY

#ifdef ABT_CONFIG_USE_LINUX_FUTEX

/* Use Linux futex. */
#include <unistd.h>
#include <linux/futex.h>
#include <syscall.h>

void ABTD_futex_wait_and_unlock(ABTD_futex_multiple *p_futex,
                                ABTD_spinlock *p_lock)
{
    const int original_val = ABTD_atomic_relaxed_load_int(&p_futex->val);
    ABTD_spinlock_release(p_lock);
    do {
        syscall(SYS_futex, &p_futex->val.val, FUTEX_WAIT_PRIVATE, original_val,
                NULL, NULL, 0);
    } while (ABTD_atomic_relaxed_load_int(&p_futex->val) == original_val);
}

void ABTD_futex_timedwait_and_unlock(ABTD_futex_multiple *p_futex,
                                     ABTD_spinlock *p_lock,
                                     double wait_time_sec)
{
    const int original_val = ABTD_atomic_relaxed_load_int(&p_futex->val);
    ABTD_spinlock_release(p_lock);
    struct timespec wait_time; /* This wait_time must be **relative**. */
    wait_time.tv_sec = (time_t)wait_time_sec;
    wait_time.tv_nsec =
        (long)((wait_time_sec - (double)(time_t)wait_time_sec) * 1.0e9);
    syscall(SYS_futex, &p_futex->val.val, FUTEX_WAIT_PRIVATE, original_val,
            &wait_time, NULL, 0);
}

void ABTD_futex_broadcast(ABTD_futex_multiple *p_futex)
{
    int current_val = ABTD_atomic_relaxed_load_int(&p_futex->val);
    ABTD_atomic_relaxed_store_int(&p_futex->val, current_val + 1);
    syscall(SYS_futex, &p_futex->val.val, FUTEX_WAKE_PRIVATE, INT_MAX, NULL,
            NULL, 0);
}

#else /* ABT_CONFIG_USE_LINUX_FUTEX */

/* Use Pthreads. */
#include <pthread.h>

typedef struct pthread_sync {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    struct pthread_sync *p_next;
    struct pthread_sync *p_prev;
    ABTD_atomic_int val;
} pthread_sync;

#define PTHREAD_SYNC_STATIC_INITIALIZER                                        \
    {                                                                          \
        PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER, NULL, NULL,       \
            ABTD_ATOMIC_INT_STATIC_INITIALIZER(0),                             \
    }

void ABTD_futex_wait_and_unlock(ABTD_futex_multiple *p_futex,
                                ABTD_spinlock *p_lock)
{
    pthread_sync sync_obj = PTHREAD_SYNC_STATIC_INITIALIZER;
    pthread_mutex_lock(&sync_obj.mutex);
    /* This p_next updates must be done "after" taking mutex but "before"
     * releasing p_lock. */
    pthread_sync *p_next = (pthread_sync *)p_futex->p_next;
    if (p_next)
        p_next->p_prev = &sync_obj;
    sync_obj.p_next = p_next;
    p_futex->p_next = (void *)&sync_obj;
    ABTD_spinlock_release(p_lock);
    while (ABTD_atomic_relaxed_load_int(&sync_obj.val) == 0) {
        pthread_cond_wait(&sync_obj.cond, &sync_obj.mutex);
    }
    /* I cannot find whether a statically initialized mutex must be unlocked
     * before it gets out of scope or not, but let's choose a safer way. */
    pthread_mutex_unlock(&sync_obj.mutex);

    /* Since now val is 1, there's no possibility that the signaler is still
     * touching this sync_obj.  sync_obj can be safely released by exiting this
     * function. */
}

void ABTD_futex_timedwait_and_unlock(ABTD_futex_multiple *p_futex,
                                     ABTD_spinlock *p_lock,
                                     double wait_time_sec)
{
    pthread_sync sync_obj = PTHREAD_SYNC_STATIC_INITIALIZER;

    struct timespec wait_time; /* This time must be **relative**. */
    clock_gettime(CLOCK_REALTIME, &wait_time);
    wait_time.tv_sec += (time_t)wait_time_sec;
    wait_time.tv_nsec +=
        (long)((wait_time_sec - (double)(time_t)wait_time_sec) * 1.0e9);
    if (wait_time.tv_nsec >= 1e9) {
        wait_time.tv_sec += 1;
        wait_time.tv_nsec -= 1e9;
    }
    pthread_mutex_lock(&sync_obj.mutex);
    /* This p_next updates must be done "after" taking mutex but "before"
     * releasing p_lock. */
    pthread_sync *p_next = (pthread_sync *)p_futex->p_next;
    if (p_next)
        p_next->p_prev = &sync_obj;
    sync_obj.p_next = p_next;
    p_futex->p_next = (void *)&sync_obj;
    ABTD_spinlock_release(p_lock);
    pthread_cond_timedwait(&sync_obj.cond, &sync_obj.mutex, &wait_time);

    /* I cannot find whether a statically initialized mutex must be unlocked
     * before it gets out of scope or not, but let's choose a safer way. */
    pthread_mutex_unlock(&sync_obj.mutex);

    if (ABTD_atomic_acquire_load_int(&sync_obj.val) != 0) {
        /* Since now val is 1, there's no possibility that the signaler is still
         * touching sync_obj.  sync_obj can be safely released by exiting this
         * function. */
    } else {
        /* Maybe this sync_obj is being touched by the signaler.  Take a lock
         * and remove it from the list. */
        ABTD_spinlock_acquire(p_lock);
        /* Double check the value in a lock. */
        if (ABTD_atomic_acquire_load_int(&sync_obj.val) == 0) {
            /* timedout or spurious wakeup happens. Remove this sync_obj from
             * p_futex carefully. */
            if (p_futex->p_next == (void *)&sync_obj) {
                p_futex->p_next = (void *)sync_obj.p_next;
            } else {
                ABTI_ASSERT(sync_obj.p_prev);
                sync_obj.p_prev->p_next = sync_obj.p_next;
                sync_obj.p_next->p_prev = sync_obj.p_prev;
            }
        }
        ABTD_spinlock_release(p_lock);
    }
}

void ABTD_futex_broadcast(ABTD_futex_multiple *p_futex)
{
    /* The caller must be holding a lock (p_lock above). */
    pthread_sync *p_cur = (pthread_sync *)p_futex->p_next;
    while (p_cur) {
        pthread_sync *p_next = p_cur->p_next;
        pthread_mutex_lock(&p_cur->mutex);
        ABTD_atomic_relaxed_store_int(&p_cur->val, 1);
        pthread_cond_broadcast(&p_cur->cond);
        pthread_mutex_unlock(&p_cur->mutex);
        /* After "val" is updated and the mutex is unlocked, that pthread_sync
         * may not be touched. */
        p_cur = p_next;
    }
    p_futex->p_next = NULL;
}

#endif /* !ABT_CONFIG_USE_LINUX_FUTEX */

#endif /* !ABT_CONFIG_ACTIVE_WAIT_POLICY */
