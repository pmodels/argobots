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

void ABTI_mem_init_thread(ABTI_global *p_global);
void ABTI_mem_init_task(ABTI_global *p_global);
void ABTI_mem_init_local_thread(ABTI_local *p_local);
void ABTI_mem_init_local_task(ABTI_local *p_local);
void ABTI_mem_finalize_thread(ABTI_global *p_global);
void ABTI_mem_finalize_task(ABTI_global *p_global);
void ABTI_mem_finalize_local_thread(ABTI_local *p_local);
void ABTI_mem_finalize_local_task(ABTI_local *p_local);

void ABTI_mem_init(ABTI_global *p_global)
{
    ABTI_mem_init_thread(p_global);
    ABTI_mem_init_task(p_global);
}

void ABTI_mem_init_local(ABTI_local *p_local)
{
    ABTI_mem_init_local_thread(p_local);
    ABTI_mem_init_local_task(p_local);
}

void ABTI_mem_finalize(ABTI_global *p_global)
{
    ABTI_mem_finalize_thread(p_global);
    ABTI_mem_finalize_task(p_global);
}

void ABTI_mem_finalize_local(ABTI_local *p_local)
{
    ABTI_mem_finalize_local_thread(p_local);
    ABTI_mem_finalize_local_task(p_local);
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

char *ABTI_mem_alloc_large_page(int pgsize, ABT_bool *p_is_mmapped)
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

#endif /* ABT_CONFIG_USE_MEM_POOL */

