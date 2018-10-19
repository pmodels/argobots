/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"

#ifdef ABT_CONFIG_USE_MEM_POOL

#include <sys/types.h>
#include <sys/mman.h>

static inline void ABTI_mem_free_stack_list(ABTI_stack_header *p_stack);
static inline void ABTI_mem_free_sph_list(ABTI_sp_header *p_sph);
static uint64_t g_sp_id = 0;


void ABTI_mem_init_thread(ABTI_global *p_global)
{
    p_global->p_mem_stack = NULL;
    p_global->p_mem_sph = NULL;

    g_sp_id = 0;
}

void ABTI_mem_init_local_thread(ABTI_local *p_local)
{
    /* TODO: preallocate some stacks? */
    p_local->num_stacks = 0;
    p_local->p_mem_stack = NULL;
}

void ABTI_mem_finalize_thread(ABTI_global *p_global)
{
    /* Free all ramaining stacks */
    ABTI_mem_free_stack_list(p_global->p_mem_stack);
    p_global->p_mem_stack = NULL;

    /* Free all stack pages */
    ABTI_mem_free_sph_list(p_global->p_mem_sph);
    p_global->p_mem_sph = NULL;
}

void ABTI_mem_finalize_local_thread(ABTI_local *p_local)
{
    /* Free all ramaining stacks */
    ABTI_mem_free_stack_list(p_local->p_mem_stack);
    p_local->num_stacks = 0;
    p_local->p_mem_stack = NULL;
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

char *ABTI_mem_take_global_stack(ABTI_local *p_local)
{
    ABTI_global *p_global = gp_ABTI_global;
    ABTI_stack_header *p_sh, *p_cur;
    uint32_t cnt_stacks = 0;

    void **ptr;
    void *old;
    do {
        p_sh = (ABTI_stack_header *)
            ABTD_atomic_load_ptr((void **)&p_global->p_mem_stack);
        ptr = (void **)&p_global->p_mem_stack;
        old = (void *)p_sh;
    } while (!ABTD_atomic_bool_cas_weak_ptr(ptr, old, NULL));

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
    void **ptr;
    void *old, *new;

    do {
        ABTI_stack_header *p_mem_stack = (ABTI_stack_header *)
            ABTD_atomic_load_ptr((void **)&p_global->p_mem_stack);
        p_sh->p_next = p_mem_stack;
        ptr = (void **)&p_global->p_mem_stack;
        old = (void *)p_mem_stack;
        new = (void *)p_sh;
    } while (!ABTD_atomic_bool_cas_weak_ptr(ptr, old, new));
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

    uint32_t header_size = ABTI_MEM_SH_SIZE;
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
    void **ptr = (void **)&gp_ABTI_global->p_mem_sph;
    void *old;
    do {
        p_sph->p_next = (ABTI_sp_header *)ABTD_atomic_load_ptr(ptr);
        old = (void *)p_sph->p_next;
    } while (!ABTD_atomic_bool_cas_weak_ptr(ptr, old, (void *)p_sph));

    return p_first;
}

#endif /* ABT_CONFIG_USE_MEM_POOL */

