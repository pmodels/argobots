/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"

#ifdef ABT_CONFIG_USE_MEM_POOL

#include <sys/types.h>
#include <sys/mman.h>

static inline void ABTI_mem_free_page_list(ABTI_page_header *p_ph);
static inline void ABTI_mem_add_page(ABTI_local *p_local,
                                     ABTI_page_header *p_ph);
static inline void ABTI_mem_add_pages_to_global(ABTI_page_header *p_head,
                                                ABTI_page_header *p_tail);


void ABTI_mem_init_task(ABTI_global *p_global)
{
    ABTI_spinlock_create(&p_global->mem_task_lock);
    p_global->p_mem_task = NULL;
}

void ABTI_mem_init_local_task(ABTI_local *p_local)
{
    /* TODO: preallocate some task blocks? */
    p_local->p_mem_task_head = NULL;
    p_local->p_mem_task_tail = NULL;
}

void ABTI_mem_finalize_task(ABTI_global *p_global)
{
    /* Free all task blocks */
    ABTI_spinlock_free(&p_global->mem_task_lock);
    ABTI_mem_free_page_list(p_global->p_mem_task);
    p_global->p_mem_task = NULL;
}

void ABTI_mem_finalize_local_task(ABTI_local *p_local)
{
    /* Free all task block pages */
    ABTI_page_header *p_rem_head = NULL;
    ABTI_page_header *p_rem_tail = NULL;
    ABTI_page_header *p_cur = p_local->p_mem_task_head;
    while (p_cur) {
        ABTI_page_header *p_tmp = p_cur;
        p_cur = p_cur->p_next;

        size_t num_free_blks = p_tmp->num_empty_blks + p_tmp->num_remote_free;
        if (num_free_blks == p_tmp->num_total_blks) {
            if (p_tmp->is_mmapped == ABT_TRUE) {
                munmap(p_tmp, gp_ABTI_global->mem_page_size);
            } else {
                ABTU_free(p_tmp);
            }
        } else {
            if (p_tmp->p_free) {
                ABTI_mem_take_free(p_tmp);
            }

            p_tmp->p_owner = NULL;
            p_tmp->p_prev = NULL;
            p_tmp->p_next = p_rem_head;
            p_rem_head = p_tmp;
            if (p_rem_tail == NULL) {
                p_rem_tail = p_tmp;
            }
        }

        if (p_cur == p_local->p_mem_task_head) break;
    }
    p_local->p_mem_task_head = NULL;
    p_local->p_mem_task_tail = NULL;

    /* If there are pages that have not been fully freed, we move them to the
     * global task page list. */
    if (p_rem_head) {
        ABTI_mem_add_pages_to_global(p_rem_head, p_rem_tail);
    }
}

static inline void ABTI_mem_free_page_list(ABTI_page_header *p_ph)
{
    ABTI_page_header *p_cur, *p_tmp;

    p_cur = p_ph;
    while (p_cur) {
        p_tmp = p_cur;
        p_cur = p_cur->p_next;
        if (p_tmp->is_mmapped == ABT_TRUE) {
            munmap(p_tmp, gp_ABTI_global->mem_page_size);
        } else {
            ABTU_free(p_tmp);
        }
    }
}

static inline void ABTI_mem_add_page(ABTI_local *p_local,
                                     ABTI_page_header *p_ph)
{
    p_ph->p_owner = p_local->p_xstream;

    /* Add the page to the head */
    if (p_local->p_mem_task_head != NULL) {
        p_ph->p_prev = p_local->p_mem_task_tail;
        p_ph->p_next = p_local->p_mem_task_head;
        p_local->p_mem_task_head->p_prev = p_ph;
        p_local->p_mem_task_tail->p_next = p_ph;
        p_local->p_mem_task_head = p_ph;
    } else {
        p_ph->p_prev = p_ph;
        p_ph->p_next = p_ph;
        p_local->p_mem_task_head = p_ph;
        p_local->p_mem_task_tail = p_ph;
    }
}

static inline void ABTI_mem_add_pages_to_global(ABTI_page_header *p_head,
                                                ABTI_page_header *p_tail)
{
    ABTI_global *p_global = gp_ABTI_global;

    /* Add the page list to the global list */
    ABTI_spinlock_acquire(&p_global->mem_task_lock);
    p_tail->p_next = p_global->p_mem_task;
    p_global->p_mem_task = p_head;
    ABTI_spinlock_release(&p_global->mem_task_lock);
}

ABTI_page_header *ABTI_mem_alloc_page(ABTI_local *p_local, size_t blk_size)
{
    int i;
    ABTI_page_header *p_ph;
    ABTI_blk_header *p_cur;
    ABTI_global *p_global = gp_ABTI_global;
    const uint32_t clsize = ABT_CONFIG_STATIC_CACHELINE_SIZE;
    size_t pgsize = p_global->mem_page_size;
    ABT_bool is_mmapped;

    /* Make the page header size a multiple of cache line size */
    const size_t ph_size = (sizeof(ABTI_page_header)+clsize) / clsize * clsize;

    uint32_t num_blks = (pgsize - ph_size) / blk_size;
    char *p_page = ABTI_mem_alloc_large_page(pgsize, &is_mmapped);

    /* Set the page header */
    p_ph = (ABTI_page_header *)p_page;
    p_ph->blk_size = blk_size;
    p_ph->num_total_blks = num_blks;
    p_ph->num_empty_blks = num_blks;
    p_ph->num_remote_free = 0;
    p_ph->p_head = (ABTI_blk_header *)(p_page + ph_size);
    p_ph->p_free = NULL;
    ABTI_mem_add_page(p_local, p_ph);
    p_ph->is_mmapped = is_mmapped;

    /* Make a liked list of all free blocks */
    p_cur = p_ph->p_head;
    for (i = 0; i < num_blks - 1; i++) {
        p_cur->p_ph = p_ph;
        p_cur->p_next = (ABTI_blk_header *)((char *)p_cur + blk_size);
        p_cur = p_cur->p_next;
    }
    p_cur->p_ph = p_ph;
    p_cur->p_next = NULL;

    return p_ph;
}

void ABTI_mem_free_page(ABTI_local *p_local, ABTI_page_header *p_ph)
{
    /* We keep one page for future use. */
    if (p_local->p_mem_task_head == p_local->p_mem_task_tail) return;

    uint32_t num_free_blks = p_ph->num_empty_blks + p_ph->num_remote_free;
    if (num_free_blks == p_ph->num_total_blks) {
        /* All blocks in the page have been freed */
        /* Remove from the list and free the page */
        p_ph->p_prev->p_next = p_ph->p_next;
        p_ph->p_next->p_prev = p_ph->p_prev;
        if (p_ph == p_local->p_mem_task_head) {
            p_local->p_mem_task_head = p_ph->p_next;
        } else if (p_ph == p_local->p_mem_task_tail) {
            p_local->p_mem_task_tail = p_ph->p_prev;
        }
        if (p_ph->is_mmapped == ABT_TRUE) {
            munmap(p_ph, gp_ABTI_global->mem_page_size);
        } else {
            ABTU_free(p_ph);
        }
    }
}

void ABTI_mem_take_free(ABTI_page_header *p_ph)
{
    /* Decrease the number of remote free blocks. */
    /* NOTE: p_ph->num_empty_blks p_ph->num_remote_free do not need to be
     * accurate as long as their sum is the same as the actual number of free
     * blocks. We keep these variables to avoid chasing the linked list to count
     * the number of free blocks. */
    uint32_t num_remote_free = p_ph->num_remote_free;
    void **ptr;
    void *old;

    ABTD_atomic_fetch_sub_uint32(&p_ph->num_remote_free, num_remote_free);
    p_ph->num_empty_blks += num_remote_free;

    /* Take the remote free pointer */
    do {
        ABTI_blk_header *p_free = (ABTI_blk_header *)
            ABTD_atomic_load_ptr((void **)&p_ph->p_free);
        p_ph->p_head = p_free;
        ptr = (void **)&p_ph->p_free;
        old = (void *)p_free;
    } while (!ABTD_atomic_bool_cas_weak_ptr(ptr, old, NULL));
}

void ABTI_mem_free_remote(ABTI_page_header *p_ph, ABTI_blk_header *p_bh)
{
    void **ptr;
    void *old, *new;
    do {
        ABTI_blk_header *p_free = (ABTI_blk_header *)
            ABTD_atomic_load_ptr((void **)&p_ph->p_free);
        p_bh->p_next = p_free;
        ptr = (void **)&p_ph->p_free;
        old = (void *)p_free;
        new = (void *)p_bh;
    } while (!ABTD_atomic_bool_cas_weak_ptr(ptr, old, new));

    /* Increase the number of remote free blocks */
    ABTD_atomic_fetch_add_uint32(&p_ph->num_remote_free, 1);
}

ABTI_page_header *ABTI_mem_take_global_page(ABTI_local *p_local)
{
    ABTI_global *p_global = gp_ABTI_global;
    ABTI_page_header *p_ph = NULL;

    /* Take the first page out */
    ABTI_spinlock_acquire(&p_global->mem_task_lock);
    if (p_global->p_mem_task) {
        p_ph = p_global->p_mem_task;
        p_global->p_mem_task = p_ph->p_next;
    }
    ABTI_spinlock_release(&p_global->mem_task_lock);

    if (p_ph) {
        ABTI_mem_add_page(p_local, p_ph);
        if (p_ph->p_free) ABTI_mem_take_free(p_ph);
        if (p_ph->p_head == NULL) p_ph = NULL;
    }

    return p_ph;
}

#endif /* ABT_CONFIG_USE_MEM_POOL */

