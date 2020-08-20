/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"
#include "abti_thread_htable.h"

ABTI_thread_htable *ABTI_thread_htable_create(uint32_t num_rows)
{
    ABTI_STATIC_ASSERT(sizeof(ABTI_thread_queue) == 192);

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
    ABTI_spinlock_clear(&p_htable->mutex);
#endif
    ABTD_atomic_relaxed_store_uint32(&p_htable->num_elems, 0);
    p_htable->num_rows = num_rows;
    p_htable->queue = (ABTI_thread_queue *)ABTU_memalign(64, q_size);
    memset(p_htable->queue, 0, q_size);
    p_htable->h_list = NULL;
    p_htable->l_list = NULL;

    return p_htable;
}

void ABTI_thread_htable_free(ABTI_thread_htable *p_htable)
{
    ABTI_ASSERT(ABTD_atomic_relaxed_load_uint32(&p_htable->num_elems) == 0);

#if defined(HAVE_LH_LOCK_H)
    lh_lock_destroy(&p_htable->mutex);
#elif defined(HAVE_CLH_H)
    clh_destroy(&p_htable->mutex);
#elif defined(USE_PTHREAD_MUTEX)
    int ret = pthread_mutex_destroy(&p_htable->mutex);
    assert(!ret);
#else
    /* ABTI_spinlock needs no finalization. */
#endif
    ABTU_free(p_htable->queue);
    ABTU_free(p_htable);
}

void ABTI_thread_htable_push(ABTI_thread_htable *p_htable, int idx,
                             ABTI_ythread *p_thread)
{
    ABTI_thread_queue *p_queue;

    if (idx >= p_htable->num_rows) {
        ABTI_ASSERT(0);
        ABTU_unreachable();
#if 0
        /* Increase the hash table */
        uint32_t cur_size, new_size;
        cur_size = p_htable->num_rows;
        new_size = (idx / cur_size + 1) * cur_size;
        p_htable->queue = (ABTI_thread_queue *)
            ABTU_realloc(p_htable->queue, cur_size * sizeof(ABTI_thread_queue),
                         new_size * sizeof(ABTI_thread_queue));
        memset(&p_htable->queue[cur_size], 0,
               (new_size - cur_size) * sizeof(ABTI_thread_queue));
        p_htable->num_rows = new_size;
#endif
    }

    /* Add p_thread to the end of the idx-th row */
    p_queue = &p_htable->queue[idx];
    ABTI_thread_queue_acquire_mutex(p_queue);
    if (p_queue->head == NULL) {
        p_queue->head = p_thread;
        p_queue->tail = p_thread;
    } else {
        p_queue->tail->thread.p_next = &p_thread->thread;
        p_queue->tail = p_thread;
    }
    p_queue->num_threads++;
    ABTI_thread_queue_release_mutex(p_queue);
    ABTD_atomic_fetch_add_uint32(&p_htable->num_elems, 1);
}

/* Unlike ABTI_thread_htable_push, this function pushes p_thread to the queue
 * only when the queue is not empty. */
ABT_bool ABTI_thread_htable_add(ABTI_thread_htable *p_htable, int idx,
                                ABTI_ythread *p_thread)
{
    ABTI_thread_queue *p_queue;

    p_queue = &p_htable->queue[idx];

    ABTI_thread_queue_acquire_mutex(p_queue);
    if (p_queue->head == NULL) {
        ABTI_ASSERT(p_queue->num_threads == 0);
        ABTI_thread_queue_release_mutex(p_queue);
        return ABT_FALSE;
    } else {
        /* Change the ULT's state to BLOCKED */
        ABTI_thread_set_blocked(p_thread);

        p_queue->tail->thread.p_next = &p_thread->thread;
        p_queue->tail = p_thread;
    }
    p_queue->num_threads++;
    ABTI_thread_queue_release_mutex(p_queue);
    ABTD_atomic_fetch_add_uint32(&p_htable->num_elems, 1);
    return ABT_TRUE;
}

void ABTI_thread_htable_push_low(ABTI_thread_htable *p_htable, int idx,
                                 ABTI_ythread *p_thread)
{
    ABTI_thread_queue *p_queue;

    if (idx >= p_htable->num_rows) {
        ABTI_ASSERT(0);
        ABTU_unreachable();
#if 0
        /* Increase the hash table */
        uint32_t cur_size, new_size;
        cur_size = p_htable->num_rows;
        new_size = (idx / cur_size + 1) * cur_size;
        p_htable->queue = (ABTI_thread_queue *)
            ABTU_realloc(p_htable->queue, cur_size * sizeof(ABTI_thread_queue),
                         new_size * sizeof(ABTI_thread_queue));
        memset(&p_htable->queue[cur_size], 0,
               (new_size - cur_size) * sizeof(ABTI_thread_queue));
        p_htable->num_rows = new_size;
#endif
    }

    /* Add p_thread to the end of the idx-th row */
    p_queue = &p_htable->queue[idx];
    ABTI_thread_queue_acquire_low_mutex(p_queue);
    if (p_queue->low_head == NULL) {
        p_queue->low_head = p_thread;
        p_queue->low_tail = p_thread;
    } else {
        p_queue->low_tail->thread.p_next = &p_thread->thread;
        p_queue->low_tail = p_thread;
    }
    p_queue->low_num_threads++;
    ABTI_thread_queue_release_low_mutex(p_queue);
    ABTD_atomic_fetch_add_uint32(&p_htable->num_elems, 1);
}

/* Unlike ABTI_thread_htable_push_low, this function pushes p_thread to the
 * queue only when the queue is not empty. */
ABT_bool ABTI_thread_htable_add_low(ABTI_thread_htable *p_htable, int idx,
                                    ABTI_ythread *p_thread)
{
    ABTI_thread_queue *p_queue;

    p_queue = &p_htable->queue[idx];

    ABTI_thread_queue_acquire_low_mutex(p_queue);
    if (p_queue->low_head == NULL) {
        ABTI_ASSERT(p_queue->low_num_threads == 0);
        ABTI_thread_queue_release_low_mutex(p_queue);
        return ABT_FALSE;
    } else {
        /* Change the ULT's state to BLOCKED */
        ABTI_thread_set_blocked(p_thread);

        p_queue->low_tail->thread.p_next = &p_thread->thread;
        p_queue->low_tail = p_thread;
    }
    p_queue->low_num_threads++;
    ABTI_thread_queue_release_low_mutex(p_queue);
    ABTD_atomic_fetch_add_uint32(&p_htable->num_elems, 1);
    return ABT_TRUE;
}

ABTI_ythread *ABTI_thread_htable_pop(ABTI_thread_htable *p_htable,
                                     ABTI_thread_queue *p_queue)
{
    ABTI_ythread *p_thread = NULL;

    ABTI_thread_queue_acquire_mutex(p_queue);
    if (p_queue->head) {
        ABTD_atomic_fetch_sub_uint32(&p_htable->num_elems, 1);
        p_thread = p_queue->head;
        if (p_queue->head == p_queue->tail) {
            p_queue->head = NULL;
            p_queue->tail = NULL;
        } else {
            p_queue->head = ABTI_thread_get_ythread(p_thread->thread.p_next);
        }

        p_queue->num_threads--;
    }
    ABTI_thread_queue_release_mutex(p_queue);

    return p_thread;
}

ABTI_ythread *ABTI_thread_htable_pop_low(ABTI_thread_htable *p_htable,
                                         ABTI_thread_queue *p_queue)
{
    ABTI_ythread *p_thread = NULL;

    ABTI_thread_queue_acquire_low_mutex(p_queue);
    if (p_queue->low_head) {
        ABTD_atomic_fetch_sub_uint32(&p_htable->num_elems, 1);
        p_thread = p_queue->low_head;
        if (p_queue->low_head == p_queue->low_tail) {
            p_queue->low_head = NULL;
            p_queue->low_tail = NULL;
        } else {
            p_queue->low_head =
                ABTI_thread_get_ythread(p_thread->thread.p_next);
        }

        p_queue->low_num_threads--;
    }
    ABTI_thread_queue_release_low_mutex(p_queue);

    return p_thread;
}

ABT_bool ABTI_thread_htable_switch_low(ABTI_xstream **pp_local_xstream,
                                       ABTI_thread_queue *p_queue,
                                       ABTI_ythread *p_thread,
                                       ABTI_thread_htable *p_htable,
                                       ABT_sync_event_type sync_event_type,
                                       void *p_sync)
{
    ABTI_ythread *p_target = NULL;
    ABTI_xstream *p_local_xstream = *pp_local_xstream;

    ABTI_thread_queue_acquire_low_mutex(p_queue);
    if (p_queue->low_head) {
        p_target = p_queue->low_head;

        /* Push p_thread to the queue */
        ABTD_atomic_release_store_int(&p_thread->thread.state,
                                      ABTI_THREAD_STATE_BLOCKED);
        ABTI_tool_event_thread_suspend(p_local_xstream, p_thread,
                                       p_thread->thread.p_parent,
                                       sync_event_type, p_sync);
        if (p_queue->low_head == p_queue->low_tail) {
            p_queue->low_head = p_thread;
            p_queue->low_tail = p_thread;
        } else {
            p_queue->low_head =
                ABTI_thread_get_ythread(p_target->thread.p_next);
            p_queue->low_tail->thread.p_next = &p_thread->thread;
            p_queue->low_tail = p_thread;
        }
    }
    ABTI_thread_queue_release_low_mutex(p_queue);

    if (p_target) {
        LOG_DEBUG("switch -> U%" PRIu64 "\n", ABTI_thread_get_id(p_target));

        /* Context-switch to p_target */
        ABTD_atomic_release_store_int(&p_target->thread.state,
                                      ABTI_THREAD_STATE_RUNNING);
        ABTI_tool_event_thread_resume(p_local_xstream, p_target,
                                      p_local_xstream
                                          ? p_local_xstream->p_thread
                                          : NULL);
        ABTI_ythread *p_prev =
            ABTI_thread_context_switch_to_sibling(pp_local_xstream, p_thread,
                                                  p_target);
        ABTI_tool_event_thread_run(*pp_local_xstream, p_thread, &p_prev->thread,
                                   p_thread->thread.p_parent);
        return ABT_TRUE;
    } else {
        return ABT_FALSE;
    }
}
