/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"

#ifdef ABT_CONFIG_USE_MEM_POOL
/* Currently the total memory allocated for stacks and task block pages is not
 * shrunk to avoid the thrashing overhead except that ESs are terminated or
 * ABT_finalize is called.  When an ES terminates its execution, stacks and
 * empty pages that it holds are deallocated.  Non-empty pages are added to the
 * global data.  When ABTI_finalize is called, all memory objects that we have
 * allocated are returned to the higher-level memory allocator. */

static inline void ABTI_mem_free_stack_list(ABTI_stack_header *p_stack);
static inline void ABTI_mem_free_page_list(ABTI_page_header *p_ph);
static inline void ABTI_mem_add_page(ABTI_xstream *p_local_xstream,
                                     ABTI_page_header *p_ph);
static inline void ABTI_mem_add_pages_to_global(ABTI_page_header *p_head,
                                                ABTI_page_header *p_tail);
static inline void ABTI_mem_free_sph_list(ABTI_sp_header *p_sph);
static ABTD_atomic_uint64 g_sp_id = ABTD_ATOMIC_UINT64_STATIC_INITIALIZER(0);

void ABTI_mem_init(ABTI_global *p_global)
{
    p_global->p_mem_stack = NULL;
    ABTI_spinlock_clear(&p_global->mem_task_lock);
    p_global->p_mem_task = NULL;
    p_global->p_mem_sph = NULL;

    ABTD_atomic_relaxed_store_uint64(&g_sp_id, 0);
}

void ABTI_mem_init_local(ABTI_xstream *p_local_xstream)
{
    /* TODO: preallocate some stacks? */
    p_local_xstream->num_stacks = 0;
    p_local_xstream->p_mem_stack = NULL;

    /* TODO: preallocate some task blocks? */
    p_local_xstream->p_mem_task_head = NULL;
    p_local_xstream->p_mem_task_tail = NULL;
}

void ABTI_mem_finalize(ABTI_global *p_global)
{
    /* Free all remaining stacks */
    ABTI_mem_free_stack_list(p_global->p_mem_stack);
    p_global->p_mem_stack = NULL;

    /* Free all task blocks */
    ABTI_mem_free_page_list(p_global->p_mem_task);
    p_global->p_mem_task = NULL;

    /* Free all stack pages */
    ABTI_mem_free_sph_list(p_global->p_mem_sph);
    p_global->p_mem_sph = NULL;
}

void ABTI_mem_finalize_local(ABTI_xstream *p_local_xstream)
{
    /* Free all remaining stacks */
    ABTI_mem_free_stack_list(p_local_xstream->p_mem_stack);
    p_local_xstream->num_stacks = 0;
    p_local_xstream->p_mem_stack = NULL;

    /* Free all task block pages */
    ABTI_page_header *p_rem_head = NULL;
    ABTI_page_header *p_rem_tail = NULL;
    ABTI_page_header *p_cur = p_local_xstream->p_mem_task_head;
    while (p_cur) {
        ABTI_page_header *p_tmp = p_cur;
        p_cur = p_cur->p_next;

        size_t num_free_blks =
            p_tmp->num_empty_blks +
            ABTD_atomic_acquire_load_uint32(&p_tmp->num_remote_free);
        if (num_free_blks == p_tmp->num_total_blks) {
            ABTU_free_largepage(p_tmp, gp_ABTI_global->mem_page_size,
                                p_tmp->lp_type);
        } else {
            if (p_tmp->p_free) {
                ABTI_mem_take_free(p_tmp);
            }

            p_tmp->owner_id = 0;
            p_tmp->p_prev = NULL;
            p_tmp->p_next = p_rem_head;
            p_rem_head = p_tmp;
            if (p_rem_tail == NULL) {
                p_rem_tail = p_tmp;
            }
        }

        if (p_cur == p_local_xstream->p_mem_task_head)
            break;
    }
    p_local_xstream->p_mem_task_head = NULL;
    p_local_xstream->p_mem_task_tail = NULL;

    /* If there are pages that have not been fully freed, we move them to the
     * global task page list. */
    if (p_rem_head) {
        ABTI_mem_add_pages_to_global(p_rem_head, p_rem_tail);
    }
}

int ABTI_mem_check_lp_alloc(int lp_alloc)
{
    size_t sp_size = gp_ABTI_global->mem_sp_size;
    size_t pg_size = gp_ABTI_global->mem_page_size;
    size_t alignment = ABT_CONFIG_STATIC_CACHELINE_SIZE;
    switch (lp_alloc) {
        case ABTI_MEM_LP_MMAP_RP:
            if (ABTU_is_supported_largepage_type(pg_size, alignment,
                                                 ABTU_MEM_LARGEPAGE_MMAP)) {
                return ABTI_MEM_LP_MMAP_RP;
            } else {
                return ABTI_MEM_LP_MALLOC;
            }
        case ABTI_MEM_LP_MMAP_HP_RP:
            if (ABTU_is_supported_largepage_type(
                    sp_size, alignment, ABTU_MEM_LARGEPAGE_MMAP_HUGEPAGE)) {
                return ABTI_MEM_LP_MMAP_HP_RP;
            } else if (
                ABTU_is_supported_largepage_type(pg_size, alignment,
                                                 ABTU_MEM_LARGEPAGE_MMAP)) {
                return ABTI_MEM_LP_MMAP_RP;
            } else {
                return ABTI_MEM_LP_MALLOC;
            }
        case ABTI_MEM_LP_MMAP_HP_THP:
            if (ABTU_is_supported_largepage_type(
                    sp_size, alignment, ABTU_MEM_LARGEPAGE_MMAP_HUGEPAGE)) {
                return ABTI_MEM_LP_MMAP_HP_THP;
            } else if (
                ABTU_is_supported_largepage_type(pg_size,
                                                 gp_ABTI_global->huge_page_size,
                                                 ABTU_MEM_LARGEPAGE_MEMALIGN)) {
                return ABTI_MEM_LP_THP;
            } else {
                return ABTI_MEM_LP_MALLOC;
            }
        case ABTI_MEM_LP_THP:
            if (ABTU_is_supported_largepage_type(pg_size,
                                                 gp_ABTI_global->huge_page_size,
                                                 ABTU_MEM_LARGEPAGE_MEMALIGN)) {
                return ABTI_MEM_LP_THP;
            } else {
                return ABTI_MEM_LP_MALLOC;
            }
        default:
            return ABTI_MEM_LP_MALLOC;
    }
}

static inline void ABTI_mem_free_stack_list(ABTI_stack_header *p_stack)
{
    ABTI_stack_header *p_cur, *p_tmp;

    p_cur = p_stack;
    while (p_cur) {
        p_tmp = p_cur;
        p_cur = p_cur->p_next;
        ABTD_atomic_fetch_add_uint32(&p_tmp->p_sph->num_empty_stacks, 1);
    }
}

static inline void ABTI_mem_free_page_list(ABTI_page_header *p_ph)
{
    ABTI_page_header *p_cur, *p_tmp;

    p_cur = p_ph;
    while (p_cur) {
        p_tmp = p_cur;
        p_cur = p_cur->p_next;
        ABTU_free_largepage(p_tmp, gp_ABTI_global->mem_page_size,
                            p_tmp->lp_type);
    }
}

static inline void ABTI_mem_add_page(ABTI_xstream *p_local_xstream,
                                     ABTI_page_header *p_ph)
{
    p_ph->owner_id = ABTI_self_get_native_thread_id(p_local_xstream);

    /* Add the page to the head */
    if (p_local_xstream->p_mem_task_head != NULL) {
        p_ph->p_prev = p_local_xstream->p_mem_task_tail;
        p_ph->p_next = p_local_xstream->p_mem_task_head;
        p_local_xstream->p_mem_task_head->p_prev = p_ph;
        p_local_xstream->p_mem_task_tail->p_next = p_ph;
        p_local_xstream->p_mem_task_head = p_ph;
    } else {
        p_ph->p_prev = p_ph;
        p_ph->p_next = p_ph;
        p_local_xstream->p_mem_task_head = p_ph;
        p_local_xstream->p_mem_task_tail = p_ph;
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

char *ABTI_mem_take_global_stack(ABTI_xstream *p_local_xstream)
{
    ABTI_global *p_global = gp_ABTI_global;
    ABTI_stack_header *p_sh, *p_cur;
    uint32_t cnt_stacks = 0;

    ABTD_atomic_ptr *ptr;
    void *old;
    do {
        p_sh = (ABTI_stack_header *)ABTD_atomic_acquire_load_ptr(
            (ABTD_atomic_ptr *)&p_global->p_mem_stack);
        ptr = (ABTD_atomic_ptr *)&p_global->p_mem_stack;
        old = (void *)p_sh;
    } while (!ABTD_atomic_bool_cas_weak_ptr(ptr, old, NULL));

    if (p_sh == NULL)
        return NULL;

    /* TODO: need a better counting method */
    /* TODO: if there are too many stacks in the global stack pool, we should
     * only take some of them (e.g., take max_stacks count) and add the rest
     * back to the global stack pool. */
    p_cur = p_sh;
    while (p_cur->p_next) {
        p_cur = p_cur->p_next;
        cnt_stacks++;
    }

    /* Return the first one and keep the rest in p_local_xstream */
    p_local_xstream->num_stacks = cnt_stacks;
    p_local_xstream->p_mem_stack = p_sh->p_next;

    return (char *)p_sh - sizeof(ABTI_thread);
}

void ABTI_mem_add_stack_to_global(ABTI_stack_header *p_sh)
{
    ABTI_global *p_global = gp_ABTI_global;
    ABTD_atomic_ptr *ptr;
    void *old, *new;

    do {
        ABTI_stack_header *p_mem_stack =
            (ABTI_stack_header *)ABTD_atomic_acquire_load_ptr(
                (ABTD_atomic_ptr *)&p_global->p_mem_stack);
        p_sh->p_next = p_mem_stack;
        ptr = (ABTD_atomic_ptr *)&p_global->p_mem_stack;
        old = (void *)p_mem_stack;
        new = (void *)p_sh;
    } while (!ABTD_atomic_bool_cas_weak_ptr(ptr, old, new));
}

static char *ABTI_mem_alloc_large_page(int pgsize,
                                       ABTU_MEM_LARGEPAGE_TYPE *p_page_type)
{
    char *p_page = NULL;
    int num_requested_types = 0;
    ABTU_MEM_LARGEPAGE_TYPE requested_types[3];

    switch (gp_ABTI_global->mem_lp_alloc) {
        case ABTI_MEM_LP_MMAP_RP:
            requested_types[num_requested_types++] = ABTU_MEM_LARGEPAGE_MMAP;
            requested_types[num_requested_types++] = ABTU_MEM_LARGEPAGE_MALLOC;
            break;
        case ABTI_MEM_LP_MMAP_HP_RP:
            requested_types[num_requested_types++] =
                ABTU_MEM_LARGEPAGE_MMAP_HUGEPAGE;
            requested_types[num_requested_types++] = ABTU_MEM_LARGEPAGE_MMAP;
            requested_types[num_requested_types++] = ABTU_MEM_LARGEPAGE_MALLOC;
            break;
        case ABTI_MEM_LP_MMAP_HP_THP:
            requested_types[num_requested_types++] =
                ABTU_MEM_LARGEPAGE_MMAP_HUGEPAGE;
            requested_types[num_requested_types++] =
                ABTU_MEM_LARGEPAGE_MEMALIGN;
            requested_types[num_requested_types++] = ABTU_MEM_LARGEPAGE_MALLOC;
            break;
        case ABTI_MEM_LP_THP:
            requested_types[num_requested_types++] =
                ABTU_MEM_LARGEPAGE_MEMALIGN;
            requested_types[num_requested_types++] = ABTU_MEM_LARGEPAGE_MALLOC;
            break;
        default:
            requested_types[num_requested_types++] = ABTU_MEM_LARGEPAGE_MALLOC;
            break;
    }

    p_page =
        (char *)ABTU_alloc_largepage(pgsize, gp_ABTI_global->huge_page_size,
                                     requested_types, num_requested_types,
                                     p_page_type);
#ifdef ABT_CONFIG_USE_DEBUG_LOG
    if (!p_page) {
        LOG_DEBUG("page allocation failed (%d)\n", pgsize);
    } else if (*p_page_type == ABTU_MEM_LARGEPAGE_MALLOC) {
        LOG_DEBUG("malloc a regular page (%d): %p\n", pgsize, p_page);
    } else if (*p_page_type == ABTU_MEM_LARGEPAGE_MEMALIGN) {
        LOG_DEBUG("memalign a THP (%d): %p\n", pgsize, p_page);
    } else if (*p_page_type == ABTU_MEM_LARGEPAGE_MMAP) {
        LOG_DEBUG("mmap a regular page (%d): %p\n", pgsize, p_page);
    } else if (*p_page_type == ABTU_MEM_LARGEPAGE_MMAP_HUGEPAGE) {
        LOG_DEBUG("mmap a huge page (%d): %p\n", pgsize, p_page);
    }
#endif
    return p_page;
}

ABTI_page_header *ABTI_mem_alloc_page(ABTI_xstream *p_local_xstream,
                                      size_t blk_size)
{
    int i;
    ABTI_page_header *p_ph;
    ABTI_blk_header *p_cur;
    ABTI_global *p_global = gp_ABTI_global;
    const uint32_t clsize = ABT_CONFIG_STATIC_CACHELINE_SIZE;
    size_t pgsize = p_global->mem_page_size;
    ABTU_MEM_LARGEPAGE_TYPE lp_type;

    /* Make the page header size a multiple of cache line size */
    const size_t ph_size =
        (sizeof(ABTI_page_header) + clsize) / clsize * clsize;

    uint32_t num_blks = (pgsize - ph_size) / blk_size;
    char *p_page = ABTI_mem_alloc_large_page(pgsize, &lp_type);

    /* Set the page header */
    p_ph = (ABTI_page_header *)p_page;
    p_ph->blk_size = blk_size;
    p_ph->num_total_blks = num_blks;
    p_ph->num_empty_blks = num_blks;
    ABTD_atomic_relaxed_store_uint32(&p_ph->num_remote_free, 0);
    p_ph->p_head = (ABTI_blk_header *)(p_page + ph_size);
    p_ph->p_free = NULL;
    ABTI_mem_add_page(p_local_xstream, p_ph);
    p_ph->lp_type = lp_type;

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

void ABTI_mem_free_page(ABTI_xstream *p_local_xstream, ABTI_page_header *p_ph)
{
    /* We keep one page for future use. */
    if (p_local_xstream->p_mem_task_head == p_local_xstream->p_mem_task_tail)
        return;

    uint32_t num_free_blks =
        p_ph->num_empty_blks +
        ABTD_atomic_acquire_load_uint32(&p_ph->num_remote_free);
    if (num_free_blks == p_ph->num_total_blks) {
        /* All blocks in the page have been freed */
        /* Remove from the list and free the page */
        p_ph->p_prev->p_next = p_ph->p_next;
        p_ph->p_next->p_prev = p_ph->p_prev;
        if (p_ph == p_local_xstream->p_mem_task_head) {
            p_local_xstream->p_mem_task_head = p_ph->p_next;
        } else if (p_ph == p_local_xstream->p_mem_task_tail) {
            p_local_xstream->p_mem_task_tail = p_ph->p_prev;
        }
        ABTU_free_largepage(p_ph, gp_ABTI_global->mem_page_size, p_ph->lp_type);
    }
}

void ABTI_mem_take_free(ABTI_page_header *p_ph)
{
    /* Decrease the number of remote free blocks. */
    /* NOTE: p_ph->num_empty_blks p_ph->num_remote_free do not need to be
     * accurate as long as their sum is the same as the actual number of free
     * blocks. We keep these variables to avoid chasing the linked list to count
     * the number of free blocks. */
    uint32_t num_remote_free =
        ABTD_atomic_acquire_load_uint32(&p_ph->num_remote_free);
    ABTD_atomic_ptr *ptr;
    void *old;

    ABTD_atomic_fetch_sub_uint32(&p_ph->num_remote_free, num_remote_free);
    p_ph->num_empty_blks += num_remote_free;

    /* Take the remote free pointer */
    do {
        ABTI_blk_header *p_free =
            (ABTI_blk_header *)ABTD_atomic_acquire_load_ptr(
                (ABTD_atomic_ptr *)&p_ph->p_free);
        p_ph->p_head = p_free;
        ptr = (ABTD_atomic_ptr *)&p_ph->p_free;
        old = (void *)p_free;
    } while (!ABTD_atomic_bool_cas_weak_ptr(ptr, old, NULL));
}

void ABTI_mem_free_remote(ABTI_page_header *p_ph, ABTI_blk_header *p_bh)
{
    ABTD_atomic_ptr *ptr;
    void *old, *new;
    do {
        ABTI_blk_header *p_free =
            (ABTI_blk_header *)ABTD_atomic_acquire_load_ptr(
                (ABTD_atomic_ptr *)&p_ph->p_free);
        p_bh->p_next = p_free;
        ptr = (ABTD_atomic_ptr *)&p_ph->p_free;
        old = (void *)p_free;
        new = (void *)p_bh;
    } while (!ABTD_atomic_bool_cas_weak_ptr(ptr, old, new));

    /* Increase the number of remote free blocks */
    ABTD_atomic_fetch_add_uint32(&p_ph->num_remote_free, 1);
}

ABTI_page_header *ABTI_mem_take_global_page(ABTI_xstream *p_local_xstream)
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
        ABTI_mem_add_page(p_local_xstream, p_ph);
        if (p_ph->p_free)
            ABTI_mem_take_free(p_ph);
        if (p_ph->p_head == NULL)
            p_ph = NULL;
    }

    return p_ph;
}

static inline void ABTI_mem_free_sph_list(ABTI_sp_header *p_sph)
{
    ABTI_sp_header *p_cur, *p_tmp;

    p_cur = p_sph;
    while (p_cur) {
        p_tmp = p_cur;
        p_cur = p_cur->p_next;

        if (p_tmp->num_total_stacks !=
            ABTD_atomic_acquire_load_uint32(&p_tmp->num_empty_stacks)) {
            LOG_DEBUG("%u ULTs are not freed\n",
                      p_tmp->num_total_stacks - ABTD_atomic_acquire_load_uint32(
                                                    &p_tmp->num_empty_stacks));
        }

        ABTU_free_largepage(p_tmp->p_sp, gp_ABTI_global->mem_sp_size,
                            p_tmp->lp_type);
        ABTU_free(p_tmp);
    }
}

/* Allocate a stack page and divide it to multiple stacks by making a liked
 * list.  Then, the first stack is returned. */
char *ABTI_mem_alloc_sp(ABTI_xstream *p_local_xstream, size_t stacksize)
{
    char *p_sp, *p_first;
    ABTI_sp_header *p_sph;
    ABTI_stack_header *p_sh, *p_next;
    uint32_t num_stacks;
    int i;

    uint32_t header_size = ABTI_MEM_SH_SIZE;
    uint32_t sp_size = gp_ABTI_global->mem_sp_size;
    size_t actual_stacksize = stacksize - header_size;
    void *p_stack = NULL;

    /* Allocate a stack page header */
    p_sph = (ABTI_sp_header *)ABTU_malloc(sizeof(ABTI_sp_header));
    num_stacks = sp_size / stacksize;
    p_sph->num_total_stacks = num_stacks;
    ABTD_atomic_relaxed_store_uint32(&p_sph->num_empty_stacks, 0);
    p_sph->stacksize = stacksize;
    p_sph->id = ABTD_atomic_fetch_add_uint64(&g_sp_id, 1);

    /* Allocate a stack page */
    p_sp = ABTI_mem_alloc_large_page(sp_size, &p_sph->lp_type);

    /* Save the stack page pointer */
    p_sph->p_sp = p_sp;

    /* First stack */
    int first_pos = p_sph->id % num_stacks;
    p_first = p_sp + actual_stacksize * first_pos;
    p_sh = (ABTI_stack_header *)(p_first + sizeof(ABTI_thread));
    p_sh->p_sph = p_sph;
    p_stack = (first_pos == 0) ? (void *)(p_first + header_size * num_stacks)
                               : (void *)p_sp;
    p_sh->p_stack = p_stack;

    if (num_stacks > 1) {
        /* Make a linked list with remaining stacks */
        p_sh = (ABTI_stack_header *)((char *)p_sh + header_size);

        p_local_xstream->num_stacks = num_stacks - 1;
        p_local_xstream->p_mem_stack = p_sh;

        for (i = 1; i < num_stacks; i++) {
            p_next = (i + 1) < num_stacks
                         ? (ABTI_stack_header *)((char *)p_sh + header_size)
                         : NULL;
            p_sh->p_next = p_next;
            p_sh->p_sph = p_sph;
            if (first_pos == 0) {
                p_sh->p_stack =
                    (void *)((char *)p_stack + i * actual_stacksize);
            } else {
                if (i < first_pos) {
                    p_sh->p_stack = (void *)(p_sp + i * actual_stacksize);
                } else {
                    p_sh->p_stack =
                        (void *)(p_first + header_size * num_stacks +
                                 (i - first_pos) * actual_stacksize);
                }
            }

            p_sh = p_next;
        }
    }

    /* Add this stack page to the global stack page list */
    ABTD_atomic_ptr *ptr = (ABTD_atomic_ptr *)&gp_ABTI_global->p_mem_sph;
    void *old;
    do {
        p_sph->p_next = (ABTI_sp_header *)ABTD_atomic_acquire_load_ptr(ptr);
        old = (void *)p_sph->p_next;
    } while (!ABTD_atomic_bool_cas_weak_ptr(ptr, old, (void *)p_sph));

    return p_first;
}

#endif /* ABT_CONFIG_USE_MEM_POOL */
