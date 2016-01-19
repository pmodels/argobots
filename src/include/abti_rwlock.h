/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#ifndef RWLOCK_H_INCLUDED
#define RWLOCK_H_INCLUDED

#include "abti_mutex.h"
#include "abti_cond.h"

/* Inlined functions for RWLock */

static inline
ABTI_rwlock *ABTI_rwlock_get_ptr(ABT_rwlock rwlock)
{
#ifndef ABT_CONFIG_DISABLE_ERROR_CHECK
    ABTI_rwlock *p_rwlock;
    if (rwlock == ABT_RWLOCK_NULL) {
        p_rwlock = NULL;
    } else {
        p_rwlock = (ABTI_rwlock *)rwlock;
    }
    return p_rwlock;
#else
    return (ABTI_rwlock *)rwlock;
#endif
}

static inline
ABT_rwlock ABTI_rwlock_get_handle(ABTI_rwlock *p_rwlock)
{
#ifndef ABT_CONFIG_DISABLE_ERROR_CHECK
    ABT_rwlock h_rwlock;
    if (p_rwlock == NULL) {
        h_rwlock = ABT_RWLOCK_NULL;
    } else {
        h_rwlock = (ABT_rwlock)p_rwlock;
    }
    return h_rwlock;
#else
    return (ABT_rwlock)p_rwlock;
#endif
}

static inline
void ABTI_rwlock_init(ABTI_rwlock *p_rwlock)
{
    ABTI_mutex_init(&p_rwlock->mutex);
    ABTI_cond_init(&p_rwlock->cond);
    p_rwlock->reader_count = 0;
    p_rwlock->write_flag = 0;
}

static inline
void ABTI_rwlock_fini(ABTI_rwlock *p_rwlock)
{
    ABTI_mutex_fini(&p_rwlock->mutex);
    ABTI_cond_fini(&p_rwlock->cond);
}

static inline
int ABTI_rwlock_rdlock(ABTI_rwlock *p_rwlock)
{
    int abt_errno = ABT_SUCCESS;

    ABTI_mutex_lock(&p_rwlock->mutex);

    while (p_rwlock->write_flag && abt_errno == ABT_SUCCESS) {
        abt_errno = ABTI_cond_wait(&p_rwlock->cond, &p_rwlock->mutex);
    }

    if (abt_errno == ABT_SUCCESS) {
        p_rwlock->reader_count++;
    }

    ABTI_mutex_unlock(&p_rwlock->mutex);
    return abt_errno;
}

static inline
int ABTI_rwlock_wrlock(ABTI_rwlock *p_rwlock)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_mutex_lock(&p_rwlock->mutex);

    while ((p_rwlock->write_flag || p_rwlock->reader_count)
            && abt_errno == ABT_SUCCESS) {
        abt_errno = ABTI_cond_wait(&p_rwlock->cond, &p_rwlock->mutex);
    }

    if (abt_errno == ABT_SUCCESS) {
        p_rwlock->write_flag = 1;
    }

    ABTI_mutex_unlock(&p_rwlock->mutex);
    return abt_errno;
}

static inline
void ABTI_rwlock_unlock(ABTI_rwlock *p_rwlock)
{
    ABTI_mutex_lock(&p_rwlock->mutex);

    if (p_rwlock->write_flag) {
        p_rwlock->write_flag = 0;
    }
    else {
        p_rwlock->reader_count--;
    }

    /* TODO: elision */
    ABTI_cond_broadcast(&p_rwlock->cond);

    ABTI_mutex_unlock(&p_rwlock->mutex);
}

#endif /* RWLOCK_H_INCLUDED */

