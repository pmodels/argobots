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

#include <sys/types.h>
#include <sys/mman.h>

#define PROTS           (PROT_READ | PROT_WRITE)

#if defined(HAVE_MAP_ANONYMOUS)
#define FLAGS_RP        (MAP_PRIVATE | MAP_ANONYMOUS)
#elif defined(HAVE_MAP_ANON)
#define FLAGS_RP        (MAP_PRIVATE | MAP_ANON)
#else
/* In this case, we don't allow using mmap. We always use malloc. */
#define FLAGS_RP        (MAP_PRIVATE)
#endif

#if defined(HAVE_MAP_HUGETLB)
#define FLAGS_HP        (FLAGS_RP | MAP_HUGETLB)
#define FD_HP           0
#define MMAP_DBG_MSG    "mmap a hugepage"
#else
/* NOTE: On Mac OS, we tried VM_FLAGS_SUPERPAGE_SIZE_ANY that is defined in
 * <mach/vm_statistics.h>, but mmap() failed with it and its execution was too
 * slow.  By that reason, we do not support it for now. */
#define FLAGS_HP        FLAGS_RP
#define FD_HP           0
#define MMAP_DBG_MSG    "mmap regular pages"
#endif

static inline void ABTI_mem_free_stack_list(ABTI_stack_header *p_stack);
static inline void ABTI_mem_free_page_list(ABTI_page_header *p_ph);
static inline void ABTI_mem_add_page(ABTI_local *p_local,
                                     ABTI_page_header *p_ph);
static inline void ABTI_mem_add_pages_to_global(ABTI_page_header *p_head,
                                                ABTI_page_header *p_tail);
static inline void ABTI_mem_free_sph_list(ABTI_sp_header *p_sph);
static uint64_t g_sp_id = 0;


void ABTI_mem_init(ABTI_global *p_global)
{
    p_global->p_mem_stack = NULL;
    p_global->p_mem_task = NULL;
    p_global->p_mem_sph = NULL;

    /* Calculate the header size that should be a multiple of cache line size */
    size_t header_size = sizeof(ABTI_thread) + sizeof(ABTI_stack_header);
    uint32_t rem = header_size % p_global->cache_line_size;
    if (rem > 0) {
        header_size += (p_global->cache_line_size - rem);
    }
    p_global->mem_sh_size = header_size;

    g_sp_id = 0;
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

int ABTI_mem_check_lp_alloc(int lp_alloc)
{
    size_t sp_size = gp_ABTI_global->mem_sp_size;
    size_t pg_size = gp_ABTI_global->mem_page_size;
    size_t alignment;
    void *p_page = NULL;

    switch (lp_alloc) {
        case ABTI_MEM_LP_MMAP_RP:
            p_page = mmap(NULL, pg_size, PROTS, FLAGS_RP, 0, 0);
            if (p_page != MAP_FAILED) {
                munmap(p_page, pg_size);
            } else {
                lp_alloc = ABTI_MEM_LP_MALLOC;
            }
            break;

        case ABTI_MEM_LP_MMAP_HP_RP:
            p_page = mmap(NULL, sp_size, PROTS, FLAGS_HP, 0, 0);
            if (p_page != MAP_FAILED) {
                munmap(p_page, sp_size);
            } else {
                p_page = mmap(NULL, pg_size, PROTS, FLAGS_RP, 0, 0);
                if (p_page != MAP_FAILED) {
                    munmap(p_page, pg_size);
                    lp_alloc = ABTI_MEM_LP_MMAP_RP;
                } else {
                    lp_alloc = ABTI_MEM_LP_MALLOC;
                }
            }
            break;

        case ABTI_MEM_LP_MMAP_HP_THP:
            p_page = mmap(NULL, sp_size, PROTS, FLAGS_HP, 0, 0);
            if (p_page != MAP_FAILED) {
                munmap(p_page, sp_size);
            } else {
                alignment = gp_ABTI_global->huge_page_size;
                p_page = ABTU_memalign(alignment, pg_size);
                if (p_page) {
                    ABTU_free(p_page);
                    lp_alloc = ABTI_MEM_LP_THP;
                } else {
                    lp_alloc = ABTI_MEM_LP_MALLOC;
                }
            }
            break;

        case ABTI_MEM_LP_THP:
            alignment = gp_ABTI_global->huge_page_size;
            p_page = ABTU_memalign(alignment, pg_size);
            if (p_page) {
                ABTU_free(p_page);
                lp_alloc = ABTI_MEM_LP_THP;
            } else {
                lp_alloc = ABTI_MEM_LP_MALLOC;
            }
            break;

        default:
            break;
    }

    return lp_alloc;
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
    ABTI_spinlock_acquire(&p_global->lock);
    p_tail->p_next = p_global->p_mem_task;
    p_global->p_mem_task = p_head;
    ABTI_spinlock_release(&p_global->lock);
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

static char *ABTI_mem_alloc_large_page(int pgsize, ABT_bool *p_is_mmapped)
{
    char *p_page = NULL;

    switch (gp_ABTI_global->mem_lp_alloc) {
        case ABTI_MEM_LP_MALLOC:
            *p_is_mmapped = ABT_FALSE;
            p_page = (char *)ABTU_malloc(pgsize);
            LOG_DEBUG("malloc a regular page (%d): %p\n", pgsize, p_page);
            break;

        case ABTI_MEM_LP_MMAP_RP:
            p_page = (char *)mmap(NULL, pgsize, PROTS, FLAGS_RP, 0, 0);
            if ((void *)p_page != MAP_FAILED) {
                *p_is_mmapped = ABT_TRUE;
                LOG_DEBUG("mmap a regular page (%d): %p\n", pgsize, p_page);
            } else {
                /* mmap failed and thus we fall back to malloc. */
                p_page = (char *)ABTU_malloc(pgsize);
                *p_is_mmapped = ABT_FALSE;
                LOG_DEBUG("fall back to malloc a regular page (%d): %p\n",
                          pgsize, p_page);
            }
            break;

        case ABTI_MEM_LP_MMAP_HP_RP:
            /* We first try to mmap a huge page, and then if it fails, we mmap
             * a regular page. */
            p_page = (char *)mmap(NULL, pgsize, PROTS, FLAGS_HP, 0, 0);
            if ((void *)p_page != MAP_FAILED) {
                *p_is_mmapped = ABT_TRUE;
                LOG_DEBUG(MMAP_DBG_MSG" (%d): %p\n", pgsize, p_page);
            } else {
                /* Huge pages are run out of. Use a normal mmap. */
                p_page = (char *)mmap(NULL, pgsize, PROTS, FLAGS_RP, 0, 0);
                if ((void *)p_page != MAP_FAILED) {
                    *p_is_mmapped = ABT_TRUE;
                    LOG_DEBUG("fall back to mmap regular pages (%d): %p\n",
                              pgsize, p_page);
                } else {
                    /* mmap failed and thus we fall back to malloc. */
                    p_page = (char *)ABTU_malloc(pgsize);
                    *p_is_mmapped = ABT_FALSE;
                    LOG_DEBUG("fall back to malloc a regular page (%d): %p\n",
                              pgsize, p_page);
                }
            }
            break;

        case ABTI_MEM_LP_MMAP_HP_THP:
            /* We first try to mmap a huge page, and then if it fails, try to
             * use a THP. */
            p_page = (char *)mmap(NULL, pgsize, PROTS, FLAGS_HP, 0, 0);
            if ((void *)p_page != MAP_FAILED) {
                *p_is_mmapped = ABT_TRUE;
                LOG_DEBUG(MMAP_DBG_MSG" (%d): %p\n", pgsize, p_page);
            } else {
                *p_is_mmapped = ABT_FALSE;
                size_t alignment = gp_ABTI_global->huge_page_size;
                p_page = (char *)ABTU_memalign(alignment, pgsize);
                LOG_DEBUG("memalign a THP (%d): %p\n", pgsize, p_page);
            }
            break;

        case ABTI_MEM_LP_THP:
            *p_is_mmapped = ABT_FALSE;
            size_t alignment = gp_ABTI_global->huge_page_size;
            p_page = (char *)ABTU_memalign(alignment, pgsize);
            LOG_DEBUG("memalign a THP (%d): %p\n", pgsize, p_page);
            break;

        default:
            ABTI_ASSERT(0);
            break;
    }

    return p_page;
}

ABTI_page_header *ABTI_mem_alloc_page(ABTI_local *p_local, size_t blk_size)
{
    int i;
    ABTI_page_header *p_ph;
    ABTI_blk_header *p_cur;
    ABTI_global *p_global = gp_ABTI_global;
    uint32_t clsize = p_global->cache_line_size;
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
    ABTI_spinlock_acquire(&p_global->lock);
    if (p_global->p_mem_task) {
        p_ph = p_global->p_mem_task;
        p_global->p_mem_task = p_ph->p_next;
    }
    ABTI_spinlock_release(&p_global->lock);

    if (p_ph) {
        ABTI_mem_add_page(p_local, p_ph);
        if (p_ph->p_free) ABTI_mem_take_free(p_ph);
        if (p_ph->p_head == NULL) p_ph = NULL;
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

        if (p_tmp->num_total_stacks != p_tmp->num_empty_stacks) {
            LOG_DEBUG("%u ULTs are not freed\n",
                      p_tmp->num_total_stacks - p_tmp->num_empty_stacks);
        }

        if (p_tmp->is_mmapped == ABT_TRUE) {
            if (munmap(p_tmp->p_sp, gp_ABTI_global->mem_sp_size)) {
                ABTI_ASSERT(0);
            }
        } else {
            ABTU_free(p_tmp->p_sp);
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

    uint32_t header_size = gp_ABTI_global->mem_sh_size;
    uint32_t sp_size = gp_ABTI_global->mem_sp_size;
    size_t actual_stacksize = stacksize - header_size;
    void *p_stack = NULL;

    /* Allocate a stack page header */
    p_sph = (ABTI_sp_header *)ABTU_malloc(sizeof(ABTI_sp_header));
    num_stacks = sp_size / stacksize;
    p_sph->num_total_stacks = num_stacks;
    p_sph->num_empty_stacks = 0;
    p_sph->stacksize = stacksize;
    p_sph->id = ABTD_atomic_fetch_add_uint64(&g_sp_id, 1);

    /* Allocate a stack page */
    p_sp = ABTI_mem_alloc_large_page(sp_size, &p_sph->is_mmapped);

    /* Save the stack page pointer */
    p_sph->p_sp = p_sp;

    /* First stack */
    int first_pos = p_sph->id % num_stacks;
    p_first = p_sp + actual_stacksize * first_pos;
    p_sh = (ABTI_stack_header *)(p_first + sizeof(ABTI_thread));
    p_sh->p_sph = p_sph;
    p_stack = (first_pos == 0)
            ? (void *)(p_first + header_size * num_stacks) : (void *)p_sp;
    p_sh->p_stack = p_stack;

    if (num_stacks > 1) {
        /* Make a linked list with remaining stacks */
        p_sh = (ABTI_stack_header *)((char *)p_sh + header_size);

        p_local->num_stacks = num_stacks - 1;
        p_local->p_mem_stack = p_sh;

        for (i = 1; i < num_stacks; i++) {
            p_next = (i + 1) < num_stacks
                   ? (ABTI_stack_header *)((char *)p_sh + header_size)
                   : NULL;
            p_sh->p_next = p_next;
            p_sh->p_sph = p_sph;
            if (first_pos == 0) {
                p_sh->p_stack = (void *)((char *)p_stack + i * actual_stacksize);
            } else {
                if (i < first_pos) {
                    p_sh->p_stack = (void *)(p_sp + i * actual_stacksize);
                } else {
                    p_sh->p_stack = (void *)(p_first + header_size * num_stacks
                                  + (i - first_pos) * actual_stacksize);
                }
            }

            p_sh = p_next;
        }
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

