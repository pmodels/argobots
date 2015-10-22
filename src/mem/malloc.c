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
static inline void ABTI_mem_add_page(ABTI_local *p_local,
                                     ABTI_page_header *p_ph);
static inline void ABTI_mem_add_pages_to_global(ABTI_page_header *p_head,
                                                ABTI_page_header *p_tail);
static inline void ABTI_mem_free_sph_list(ABTI_sp_header *p_sph);


void ABTI_mem_init(ABTI_global *p_global)
{
    p_global->p_mem_stack = NULL;
    p_global->p_mem_task = NULL;
    p_global->p_mem_sph = NULL;
}

void ABTI_mem_init_local(ABTI_local *p_local)
{
    /* TODO: preallocate some stacks? */
    p_local->num_stacks = 0;
    p_local->p_mem_stack = NULL;

    /* TODO: preallocate some task blocks? */
    p_local->p_mem_task_head = NULL;
    p_local->p_mem_task_tail = NULL;
}

void ABTI_mem_finalize(ABTI_global *p_global)
{
    /* Free all ramaining stacks */
    ABTI_mem_free_stack_list(p_global->p_mem_stack);
    p_global->p_mem_stack = NULL;

    /* Free all task blocks */
    ABTI_mem_free_page_list(p_global->p_mem_task);
    p_global->p_mem_task = NULL;

    /* Free all stack pages */
    ABTI_mem_free_sph_list(p_global->p_mem_sph);
    p_global->p_mem_sph = NULL;
}

void ABTI_mem_finalize_local(ABTI_local *p_local)
{
    /* Free all ramaining stacks */
    ABTI_mem_free_stack_list(p_local->p_mem_stack);
    p_local->num_stacks = 0;
    p_local->p_mem_stack = NULL;

    /* Free all task block pages */
    ABTI_page_header *p_rem_head = NULL;
    ABTI_page_header *p_rem_tail = NULL;
    ABTI_page_header *p_cur = p_local->p_mem_task_head;
    while (p_cur) {
        ABTI_page_header *p_tmp = p_cur;
        p_cur = p_cur->p_next;

        size_t num_free_blks = p_tmp->num_empty_blks + p_tmp->num_remote_free;
        if (num_free_blks == p_tmp->num_total_blks) {
            ABTU_free(p_tmp);
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
        ABTU_free(p_tmp);
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
    ABTI_mutex_spinlock(&p_global->mutex);
    p_tail->p_next = p_global->p_mem_task;
    p_global->p_mem_task = p_head;
    ABTI_mutex_unlock(&p_global->mutex);
}

char *ABTI_mem_take_global_stack(ABTI_local *p_local)
{
    ABTI_global *p_global = gp_ABTI_global;
    const size_t ptr_size = sizeof(void *);
    ABTI_stack_header *p_sh, *p_cur;
    uint32_t cnt_stacks = 0;

    if (ptr_size == 8) {
        uint64_t *ptr;
        uint64_t old, ret;
        do {
            p_sh = p_global->p_mem_stack;
            ptr = (uint64_t *)&p_global->p_mem_stack;
            old = (uint64_t)p_sh;
            ret = ABTD_atomic_cas_uint64(ptr, old, (uint64_t)NULL);
        } while (old != ret);
    } else if (ptr_size == 4) {
        uint32_t *ptr;
        uint32_t old, ret;
        do {
            p_sh = p_global->p_mem_stack;
            ptr = (uint32_t *)&p_global->p_mem_stack;
            old = (uint32_t)(uintptr_t)p_sh;
            ret = ABTD_atomic_cas_uint32(ptr, old, (uint64_t)NULL);
        } while (old != ret);
    }

    if (p_sh == NULL) return NULL;

    /* TODO: need a better counting method */
    /* TODO: if there are too many stacks in the global stack pool, we should
     * only take some of them (e.g., take max_stacks count) and add the rest
     * back to the global stack pool. */
    p_cur = p_sh;
    while (p_cur->p_next) {
        p_cur = p_cur->p_next;
        cnt_stacks++;
    }

    /* Return the first one and keep the rest in p_local */
    p_local->num_stacks = cnt_stacks;
    p_local->p_mem_stack = p_sh->p_next;

    return (char *)p_sh - sizeof(ABTI_thread);
}

void ABTI_mem_add_stack_to_global(ABTI_stack_header *p_sh)
{
    ABTI_global *p_global = gp_ABTI_global;
    const size_t ptr_size = sizeof(void *);

    if (ptr_size == 8) {
        uint64_t *ptr;
        uint64_t old, new, ret;
        do {
            p_sh->p_next = p_global->p_mem_stack;
            ptr = (uint64_t *)&p_global->p_mem_stack;
            old = (uint64_t)p_sh->p_next;
            new = (uint64_t)p_sh;
            ret = ABTD_atomic_cas_uint64(ptr, old, new);
        } while (old != ret);
    } else if (ptr_size == 4) {
        uint32_t *ptr;
        uint32_t old, new, ret;
        do {
            p_sh->p_next = p_global->p_mem_stack;
            ptr = (uint32_t *)&p_global->p_mem_stack;
            old = (uint32_t)(uintptr_t)p_sh->p_next;
            new = (uint32_t)(uintptr_t)p_sh;
            ret = ABTD_atomic_cas_uint32(ptr, old, new);
        } while (old != ret);
    } else {
        ABTI_ASSERT(0);
    }
}

ABTI_page_header *ABTI_mem_alloc_page(ABTI_local *p_local, size_t blk_size)
{
    int i;
    ABTI_page_header *p_ph;
    ABTI_blk_header *p_cur;
    ABTI_global *p_global = gp_ABTI_global;
    uint32_t clsize = p_global->cache_line_size;
    size_t pgsize = p_global->page_size;

    /* Make the page header size a multiple of cache line size */
    const size_t ph_size = (sizeof(ABTI_page_header)+clsize) / clsize * clsize;

    uint32_t num_blks = (pgsize - ph_size) / blk_size;
    char *p_page = (char *)ABTU_memalign(clsize, pgsize);

    /* Set the page header */
    p_ph = (ABTI_page_header *)p_page;
    p_ph->blk_size = blk_size;
    p_ph->num_total_blks = num_blks;
    p_ph->num_empty_blks = num_blks;
    p_ph->num_remote_free = 0;
    p_ph->p_head = (ABTI_blk_header *)(p_page + ph_size);
    p_ph->p_free = NULL;
    ABTI_mem_add_page(p_local, p_ph);

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
        ABTU_free(p_ph);
    }
}

void ABTI_mem_take_free(ABTI_page_header *p_ph)
{
    const size_t ptr_size = sizeof(void *);

    /* Decrease the number of remote free blocks. */
    /* NOTE: p_ph->num_empty_blks p_ph->num_remote_free do not need to be
     * accurate as long as their sum is the same as the actual number of free
     * blocks. We keep these variables to avoid chasing the linked list to count
     * the number of free blocks. */
    uint32_t num_remote_free = p_ph->num_remote_free;
    ABTD_atomic_fetch_sub_uint32(&p_ph->num_remote_free, num_remote_free);
    p_ph->num_empty_blks += num_remote_free;

    /* Take the remote free pointer */
    if (ptr_size == 8) {
        uint64_t *ptr;
        uint64_t old, ret;
        do {
            p_ph->p_head = p_ph->p_free;
            ptr = (uint64_t *)&p_ph->p_free;
            old = (uint64_t)p_ph->p_head;
            ret = ABTD_atomic_cas_uint64(ptr, old, (uint64_t)NULL);
        } while (old != ret);
    } else if (ptr_size == 4) {
        uint32_t *ptr;
        uint32_t old, ret;
        do {
            p_ph->p_head = p_ph->p_free;
            ptr = (uint32_t *)&p_ph->p_free;
            old = (uint32_t)(uintptr_t)p_ph->p_head;
            ret = ABTD_atomic_cas_uint32(ptr, old, (uint32_t)(uintptr_t)NULL);
        } while (old != ret);
    } else {
        ABTI_ASSERT(0);
    }
}

void ABTI_mem_free_remote(ABTI_page_header *p_ph, ABTI_blk_header *p_bh)
{
    const size_t ptr_size = sizeof(void *);

    if (ptr_size == 8) {
        uint64_t *ptr;
        uint64_t old, new, ret;
        do {
            p_bh->p_next = p_ph->p_free;
            ptr = (uint64_t *)&p_ph->p_free;
            old = (uint64_t)p_bh->p_next;
            new = (uint64_t)p_bh;
            ret = ABTD_atomic_cas_uint64(ptr, old, new);
        } while (old != ret);
    } else if (ptr_size == 4) {
        uint32_t *ptr;
        uint32_t old, new, ret;
        do {
            p_bh->p_next = p_ph->p_free;
            ptr = (uint32_t *)&p_ph->p_free;
            old = (uint32_t)(uintptr_t)p_bh->p_next;
            new = (uint32_t)(uintptr_t)p_bh;
            ret = ABTD_atomic_cas_uint32(ptr, old, new);
        } while (old != ret);
    } else {
        ABTI_ASSERT(0);
    }

    /* Increase the number of remote free blocks */
    ABTD_atomic_fetch_add_uint32(&p_ph->num_remote_free, 1);
}

ABTI_page_header *ABTI_mem_take_global_page(ABTI_local *p_local)
{
    ABTI_global *p_global = gp_ABTI_global;
    ABTI_page_header *p_ph = NULL;

    /* Take the first page out */
    ABTI_mutex_spinlock(&p_global->mutex);
    if (p_global->p_mem_task) {
        p_ph = p_global->p_mem_task;
        p_global->p_mem_task = p_ph->p_next;
    }
    ABTI_mutex_unlock(&p_global->mutex);

    if (p_ph) {
        ABTI_mem_add_page(p_local, p_ph);
        if (p_ph->p_free) ABTI_mem_take_free(p_ph);
        if (p_ph->p_head == NULL) p_ph = NULL;
    }

    return p_ph;
}

#include <sys/types.h>
#include <sys/mman.h>

#define PROTS       (PROT_READ | PROT_WRITE)
#define FLAGS_HP    (MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB)
#define FLAGS_RP    (MAP_PRIVATE | MAP_ANONYMOUS)
#define PAGESIZE    (2 * 1024 * 1024)

static inline void ABTI_mem_free_sph_list(ABTI_sp_header *p_sph)
{
    ABTI_sp_header *p_cur, *p_tmp;

    p_cur = p_sph;
    while (p_cur) {
        p_tmp = p_cur;
        p_cur = p_cur->p_next;

        if (p_tmp->num_total_stacks != p_tmp->num_empty_stacks) {
            LOG_DEBUG("%u ULTs are not freed\n",
                      p_tmp->num_total_stacks - p_tmp->num_empty_stacks);
        }

        if (munmap(p_tmp->p_sp, PAGESIZE)) {
            ABTI_ASSERT(0);
        }
        ABTU_free(p_tmp);
    }
}

/* Allocate a stack page and divide it to multiple stacks by making a liked
 * list.  Then, the first stack is returned. */
char *ABTI_mem_alloc_sp(ABTI_local *p_local, size_t stacksize)
{
    char *p_sp, *p_first;
    ABTI_sp_header *p_sph;
    ABTI_stack_header *p_sh, *p_next;
    uint32_t num_stacks;
    int i;

    /* Allocate a stack page. We first try to mmap a huge page, and then if it
     * fails, we mmap a normal page. */
    p_sp = (char *)mmap(NULL, PAGESIZE, PROTS, FLAGS_HP, 0, 0);
    if ((void *)p_sp == MAP_FAILED) {
        /* Huge pages are run out of. Use a normal mmap. */
        p_sp = (char *)mmap(NULL, PAGESIZE, PROTS, FLAGS_RP, 0, 0);
        ABTI_ASSERT((void *)p_sp != MAP_FAILED);
        LOG_DEBUG("mmap normal pages (2MB): %p\n", p_sp);
    } else {
        LOG_DEBUG("mmap a hugepage (2MB):%p\n", p_sp);
    }

    /* Allocate a stack page header */
    p_sph = (ABTI_sp_header *)ABTU_malloc(sizeof(ABTI_sp_header));
    num_stacks = PAGESIZE / stacksize;
    p_sph->num_total_stacks = num_stacks;
    p_sph->num_empty_stacks = 0;
    p_sph->stacksize = stacksize;
    p_sph->p_sp = p_sp;

    /* First stack */
    p_first = p_sp;
    p_sh = (ABTI_stack_header *)(p_first + sizeof(ABTI_thread));
    p_sh->p_sph = p_sph;

    if (num_stacks > 1) {
        /* Make a linked list with remaining stacks */
        p_sh = (ABTI_stack_header *)(p_sp + stacksize + sizeof(ABTI_thread));
        for (i = 1; i < num_stacks; i++) {
            p_next = (i + 1) < num_stacks
                   ? (ABTI_stack_header *)((char *)p_sh + stacksize)
                   : NULL;
            p_sh->p_next = p_next;
            p_sh->p_sph = p_sph;

            p_sh = p_next;
        }

        p_local->num_stacks = num_stacks - 1;
        p_local->p_mem_stack = (ABTI_stack_header *)
                               (p_sp + stacksize + sizeof(ABTI_thread));
    }

    /* Add this stack page to the global stack page list */
    uint64_t *ptr = (uint64_t *)&gp_ABTI_global->p_mem_sph;
    uint64_t old, ret;
    do {
        p_sph->p_next = gp_ABTI_global->p_mem_sph;
        old = (uint64_t)p_sph->p_next;
        ret = ABTD_atomic_cas_uint64(ptr, old, (uint64_t)p_sph);
    } while (old != ret);

    return p_first;
}

#endif /* ABT_CONFIG_USE_MEM_POOL */

