/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#ifndef THREAD_HTABLE_H_INCLUDED
#define THREAD_HTABLE_H_INCLUDED

#include "abt_config.h"

#if defined(HAVE_LH_LOCK_H)
#include <lh_lock.h>
#elif defined(HAVE_CLH_H)
#include <clh.h>
#else
#define USE_PTHREAD_MUTEX
#endif

struct ABTI_thread_queue {
    uint32_t mutex;
    uint32_t num_handovers;
    uint32_t num_threads;
    uint32_t pad0;
    ABTI_thread *head;
    ABTI_thread *tail;
    char pad1[64-8*4];

    /* low priority queue */
    uint32_t low_mutex;
    uint32_t low_num_threads;
    ABTI_thread *low_head;
    ABTI_thread *low_tail;
    char pad2[64-8*3];

    /* two doubly-linked lists */
    ABTI_thread_queue *p_h_next;
    ABTI_thread_queue *p_h_prev;
    ABTI_thread_queue *p_l_next;
    ABTI_thread_queue *p_l_prev;
    char pad3[64-8*4];
};

struct ABTI_thread_htable {
#if defined(HAVE_LH_LOCK_H)
    lh_lock_t mutex;
#elif defined(HAVE_CLH_H)
    clh_lock_t mutex;
#elif defined(USE_PTHREAD_MUTEX)
    pthread_mutex_t mutex;
#else
    uint32_t mutex[2];          /* To protect table */
#endif
    uint32_t num_elems;
    uint32_t num_rows;
    ABTI_thread_queue *queue;

    ABTI_thread_queue *h_list;  /* list of non-empty high prio. queues */
    ABTI_thread_queue *l_list;  /* list of non-empty low prio. queues */
};

#if defined(HAVE_LH_LOCK_H)
#define ABTI_THREAD_HTABLE_LOCK(m)      lh_acquire_lock(&m)
#define ABTI_THREAD_HTABLE_UNLOCK(m)    lh_release_lock(&m)
#elif defined(HAVE_CLH_H)
#define ABTI_THREAD_HTABLE_LOCK(m)      clh_acquire(&m)
#define ABTI_THREAD_HTABLE_UNLOCK(m)    clh_release(&m)
#elif defined(USE_PTHREAD_MUTEX)
#define ABTI_THREAD_HTABLE_LOCK(m)      pthread_mutex_lock(&m)
#define ABTI_THREAD_HTABLE_UNLOCK(m)    pthread_mutex_unlock(&m)
#else
#define ABTI_THREAD_HTABLE_LOCK(m)      ABTI_PTR_SPINLOCK_LOW(m)
#define ABTI_THREAD_HTABLE_UNLOCK(m)    ABTI_PTR_UNLOCK_LOW(m)
#endif

static inline
void ABTI_thread_htable_add_h_node(ABTI_thread_htable *p_htable,
                                   ABTI_thread_queue *p_node)
{
    ABTI_thread_queue *p_curr = p_htable->h_list;
    if (!p_curr) {
        p_node->p_h_next = p_node;
        p_node->p_h_prev = p_node;
        p_htable->h_list = p_node;
    } else if (!p_node->p_h_next) {
        p_node->p_h_next = p_curr;
        p_node->p_h_prev = p_curr->p_h_prev;
        p_curr->p_h_prev->p_h_next = p_node;
        p_curr->p_h_prev = p_node;
    }
}

static inline
void ABTI_thread_htable_del_h_head(ABTI_thread_htable *p_htable)
{
    ABTI_thread_queue *p_prev, *p_next;
    ABTI_thread_queue *p_node = p_htable->h_list;

    if (p_node == p_node->p_h_next) {
        p_node->p_h_next = NULL;
        p_node->p_h_prev = NULL;
        p_htable->h_list = NULL;
    } else {
        p_prev = p_node->p_h_prev;
        p_next = p_node->p_h_next;
        p_prev->p_h_next = p_next;
        p_next->p_h_prev = p_prev;
        p_node->p_h_next = NULL;
        p_node->p_h_prev = NULL;
        p_htable->h_list = p_next;
    }
}

static inline
void ABTI_thread_htable_add_l_node(ABTI_thread_htable *p_htable,
                                   ABTI_thread_queue *p_node)
{
    ABTI_thread_queue *p_curr = p_htable->l_list;
    if (!p_curr) {
        p_node->p_l_next = p_node;
        p_node->p_l_prev = p_node;
        p_htable->l_list = p_node;
    } else if (!p_node->p_l_next) {
        p_node->p_l_next = p_curr;
        p_node->p_l_prev = p_curr->p_l_prev;
        p_curr->p_l_prev->p_l_next = p_node;
        p_curr->p_l_prev = p_node;
    }
}

static inline
void ABTI_thread_htable_del_l_head(ABTI_thread_htable *p_htable)
{
    ABTI_thread_queue *p_prev, *p_next;
    ABTI_thread_queue *p_node = p_htable->l_list;

    if (p_node == p_node->p_l_next) {
        p_node->p_l_next = NULL;
        p_node->p_l_prev = NULL;
        p_htable->l_list = NULL;
    } else {
        p_prev = p_node->p_l_prev;
        p_next = p_node->p_l_next;
        p_prev->p_l_next = p_next;
        p_next->p_l_prev = p_prev;
        p_node->p_l_next = NULL;
        p_node->p_l_prev = NULL;
        p_htable->l_list = p_next;
    }
}

#endif /* THREAD_HTABLE_H_INCLUDED */
