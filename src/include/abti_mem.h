/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#ifndef ABTI_MEM_H_INCLUDED
#define ABTI_MEM_H_INCLUDED

/* Memory allocation */

#if defined(ABT_CONFIG_USE_ALIGNED_ALLOC)
#define ABTU_CA_MALLOC(s)   ABTU_memalign(ABT_CONFIG_STATIC_CACHELINE_SIZE, s)
#else
#define ABTU_CA_MALLOC(s)   ABTU_malloc(s)
#endif /* defined(ABT_CONFIG_USE_ALIGNED_ALLOC) */

#ifdef ABT_CONFIG_USE_MEM_POOL

enum {
    ABTI_MEM_LP_MALLOC = 0,
    ABTI_MEM_LP_MMAP_RP,
    ABTI_MEM_LP_MMAP_HP_RP,
    ABTI_MEM_LP_MMAP_HP_THP,
    ABTI_MEM_LP_THP
};

void ABTI_mem_init(ABTI_global *p_global);
void ABTI_mem_init_local(ABTI_local *p_local);
void ABTI_mem_finalize(ABTI_global *p_global);
void ABTI_mem_finalize_local(ABTI_local *p_local);
int ABTI_mem_check_lp_alloc(int lp_alloc);
char *ABTI_mem_alloc_large_page(int pgsize, ABT_bool *p_is_mmapped);

#else /* ABT_CONFIG_USE_MEM_POOL */

#define ABTI_mem_init(p)
#define ABTI_mem_init_local(p)
#define ABTI_mem_finalize(p)
#define ABTI_mem_finalize_local(p)

#endif /* ABT_CONFIG_USE_MEM_POOL */

#endif /* ABTI_MEM_H_INCLUDED */

