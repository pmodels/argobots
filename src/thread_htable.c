/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"
#include "abti_thread_htable.h"

ABTI_thread_htable *ABTI_thread_htable_create(uint32_t num_rows)
{
    ABTI_ASSERT(sizeof(ABTI_thread_queue) == 192);

    ABTI_thread_htable *p_htable;
    size_t q_size = num_rows * sizeof(ABTI_thread_queue);

    p_htable = (ABTI_thread_htable *)ABTU_malloc(sizeof(ABTI_thread_htable));
#if defined(HAVE_LH_LOCK_H)
    lh_lock_init(&p_htable->mutex);
#elif defined(HAVE_CLH_H)
    clh_init(&p_htable->mutex);
#elif defined(USE_PTHREAD_MUTEX)
    int ret = pthread_mutex_init(&p_htable->mutex, NULL);
    assert(!ret);
#else
    p_htable->mutex[0] = 0;
    p_htable->mutex[1] = 0;
#endif
    p_htable->num_elems = 0;
    p_htable->num_rows = num_rows;
    p_htable->queue = (ABTI_thread_queue *)ABTU_memalign(64, q_size);
    memset(p_htable->queue, 0, q_size);
    p_htable->h_list = NULL;
    p_htable->l_list = NULL;

    return p_htable;
}

void ABTI_thread_htable_free(ABTI_thread_htable *p_htable)
{
    ABTI_ASSERT(p_htable->num_elems == 0);

#if defined(HAVE_LH_LOCK_H)
    lh_lock_destroy(&p_htable->mutex);
#elif defined(HAVE_CLH_H)
    clh_destroy(&p_htable->mutex);
#elif defined(USE_PTHREAD_MUTEX)
    int ret = pthread_mutex_destroy(&p_htable->mutex);
    assert(!ret);
#endif
    ABTU_free(p_htable->queue);
    ABTU_free(p_htable);
}

void ABTI_thread_htable_push(ABTI_thread_htable *p_htable, int idx,
                             ABTI_thread *p_thread)
{
    ABTI_thread_queue *p_queue;

    if (idx >= p_htable->num_rows) {
        ABTI_ASSERT(0);
        /* Increase the hash table */
        uint32_t new_size;
        new_size = (idx / p_htable->num_rows + 1) * p_htable->num_rows;
        p_htable->queue = (ABTI_thread_queue *)ABTU_realloc(
                p_htable->queue, new_size * sizeof(ABTI_thread_queue));
        memset(&p_htable->queue[p_htable->num_rows], 0,
               (new_size - p_htable->num_rows) * sizeof(ABTI_thread_queue));
        p_htable->num_rows = new_size;
    }

    /* Add p_thread to the end of the idx-th row */
    p_queue = &p_htable->queue[idx];
    ABTI_PTR_SPINLOCK(&p_queue->mutex);
    if (p_queue->head == NULL) {
        p_queue->head = p_thread;
        p_queue->tail = p_thread;
    } else {
        p_queue->tail->unit_def.p_next = &p_thread->unit_def;
        p_queue->tail = p_thread;
    }
    p_queue->num_threads++;
    ABTI_PTR_UNLOCK(&p_queue->mutex);
    ABTD_atomic_fetch_add_uint32(&p_htable->num_elems, 1);
}

/* Unlike ABTI_thread_htable_push, this function pushes p_thread to the queue
 * only when the queue is not empty. */
ABT_bool ABTI_thread_htable_add(ABTI_thread_htable *p_htable, int idx,
                                ABTI_thread *p_thread)
{
    ABTI_thread_queue *p_queue;

    p_queue = &p_htable->queue[idx];

    ABTI_PTR_SPINLOCK(&p_queue->mutex);
    if (p_queue->head == NULL) {
        ABTI_ASSERT(p_queue->num_threads == 0);
        ABTI_PTR_UNLOCK(&p_queue->mutex);
        return ABT_FALSE;
    } else {
        /* Change the ULT's state to BLOCKED */
        ABTI_thread_set_blocked(p_thread);

        p_queue->tail->unit_def.p_next = &p_thread->unit_def;
        p_queue->tail = p_thread;
    }
    p_queue->num_threads++;
    ABTI_PTR_UNLOCK(&p_queue->mutex);
    ABTD_atomic_fetch_add_uint32(&p_htable->num_elems, 1);
    return ABT_TRUE;
}

void ABTI_thread_htable_push_low(ABTI_thread_htable *p_htable, int idx,
                                 ABTI_thread *p_thread)
{
    ABTI_thread_queue *p_queue;

    if (idx >= p_htable->num_rows) {
        ABTI_ASSERT(0);
        /* Increase the hash table */
        uint32_t new_size;
        new_size = (idx / p_htable->num_rows + 1) * p_htable->num_rows;
        p_htable->queue = (ABTI_thread_queue *)ABTU_realloc(
                p_htable->queue, new_size * sizeof(ABTI_thread_queue));
        memset(&p_htable->queue[p_htable->num_rows], 0,
               (new_size - p_htable->num_rows) * sizeof(ABTI_thread_queue));
        p_htable->num_rows = new_size;
    }

    /* Add p_thread to the end of the idx-th row */
    p_queue = &p_htable->queue[idx];
    ABTI_PTR_SPINLOCK(&p_queue->low_mutex);
    if (p_queue->low_head == NULL) {
        p_queue->low_head = p_thread;
        p_queue->low_tail = p_thread;
    } else {
        p_queue->low_tail->unit_def.p_next = &p_thread->unit_def;
        p_queue->low_tail = p_thread;
    }
    p_queue->low_num_threads++;
    ABTI_PTR_UNLOCK(&p_queue->low_mutex);
    ABTD_atomic_fetch_add_uint32(&p_htable->num_elems, 1);
}

/* Unlike ABTI_thread_htable_push_low, this function pushes p_thread to the
 * queue only when the queue is not empty. */
ABT_bool ABTI_thread_htable_add_low(ABTI_thread_htable *p_htable, int idx,
                                    ABTI_thread *p_thread)
{
    ABTI_thread_queue *p_queue;

    p_queue = &p_htable->queue[idx];

    ABTI_PTR_SPINLOCK(&p_queue->low_mutex);
    if (p_queue->low_head == NULL) {
        ABTI_ASSERT(p_queue->low_num_threads == 0);
        ABTI_PTR_UNLOCK(&p_queue->low_mutex);
        return ABT_FALSE;
    } else {
        /* Change the ULT's state to BLOCKED */
        ABTI_thread_set_blocked(p_thread);

        p_queue->low_tail->unit_def.p_next = &p_thread->unit_def;
        p_queue->low_tail = p_thread;
    }
    p_queue->low_num_threads++;
    ABTI_PTR_UNLOCK(&p_queue->low_mutex);
    ABTD_atomic_fetch_add_uint32(&p_htable->num_elems, 1);
    return ABT_TRUE;
}

ABTI_thread *ABTI_thread_htable_pop(ABTI_thread_htable *p_htable,
                                    ABTI_thread_queue *p_queue)
{
    ABTI_thread *p_thread = NULL;

    ABTI_PTR_SPINLOCK(&p_queue->mutex);
    if (p_queue->head) {
        ABTD_atomic_fetch_sub_uint32(&p_htable->num_elems, 1);
        p_thread = p_queue->head;
        if (p_queue->head == p_queue->tail) {
            p_queue->head = NULL;
            p_queue->tail = NULL;
        } else {
            ABT_thread next = p_thread->unit_def.p_next->thread;
            p_queue->head = ABTI_thread_get_ptr(next);
        }

        p_queue->num_threads--;
    }
    ABTI_PTR_UNLOCK(&p_queue->mutex);

    return p_thread;
}

ABTI_thread *ABTI_thread_htable_pop_low(ABTI_thread_htable *p_htable,
                                        ABTI_thread_queue *p_queue)
{
    ABTI_thread *p_thread = NULL;

    ABTI_PTR_SPINLOCK(&p_queue->low_mutex);
    if (p_queue->low_head) {
        ABTD_atomic_fetch_sub_uint32(&p_htable->num_elems, 1);
        p_thread = p_queue->low_head;
        if (p_queue->low_head == p_queue->low_tail) {
            p_queue->low_head = NULL;
            p_queue->low_tail = NULL;
        } else {
            ABT_thread next = p_thread->unit_def.p_next->thread;
            p_queue->low_head = ABTI_thread_get_ptr(next);
        }

        p_queue->low_num_threads--;
    }
    ABTI_PTR_UNLOCK(&p_queue->low_mutex);

    return p_thread;
}

ABT_bool ABTI_thread_htable_switch_low(ABTI_thread_queue *p_queue,
                                       ABTI_thread *p_thread,
                                       ABTI_thread_htable *p_htable)
{
    ABTI_thread *p_target = NULL;

    ABTI_PTR_SPINLOCK(&p_queue->low_mutex);
    if (p_queue->low_head) {
        p_target = p_queue->low_head;

        /* Push p_thread to the queue */
        p_thread->state = ABT_THREAD_STATE_BLOCKED;
        if (p_queue->low_head == p_queue->low_tail) {
            p_queue->low_head = p_thread;
            p_queue->low_tail = p_thread;
        } else {
            ABT_thread next = p_target->unit_def.p_next->thread;
            p_queue->low_head = ABTI_thread_get_ptr(next);
            p_queue->low_tail->unit_def.p_next = &p_thread->unit_def;
            p_queue->low_tail = p_thread;
        }
    }
    ABTI_PTR_UNLOCK(&p_queue->low_mutex);

    if (p_target) {
        LOG_EVENT("switch -> U%" PRIu64 "\n", ABTI_thread_get_id(p_target));

        /* Context-switch to p_target */
        ABTI_local_set_thread(p_target);
        p_target->state = ABT_THREAD_STATE_RUNNING;
        ABTD_thread_context_switch(&p_thread->ctx, &p_target->ctx);
        return ABT_TRUE;
    } else {
        return ABT_FALSE;
    }
}

