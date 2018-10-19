/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#ifndef ABTI_MEM_THREAD_H_INCLUDED
#define ABTI_MEM_THREAD_H_INCLUDED

/* Header size should be a multiple of cache line size. It is constant. */
#define ABTI_MEM_SH_SIZE (((sizeof(ABTI_thread) + sizeof(ABTI_stack_header) \
                            + ABT_CONFIG_STATIC_CACHELINE_SIZE - 1) \
                           / ABT_CONFIG_STATIC_CACHELINE_SIZE) \
                          * ABT_CONFIG_STATIC_CACHELINE_SIZE)

#ifdef ABT_CONFIG_USE_MEM_POOL

struct ABTI_sp_header {
    uint32_t num_total_stacks;  /* Number of total stacks */
    uint32_t num_empty_stacks;  /* Number of empty stacks */
    size_t stacksize;           /* Stack size */
    uint64_t id;                /* ID */
    ABT_bool is_mmapped;        /* ABT_TRUE if it is mmapped */
    void *p_sp;                 /* Pointer to the allocated stack page */
    ABTI_sp_header *p_next;     /* Next stack page header */
};

struct ABTI_stack_header {
    ABTI_stack_header *p_next;
    ABTI_sp_header *p_sph;
    void *p_stack;
};

char *ABTI_mem_take_global_stack(ABTI_local *p_local);
void ABTI_mem_add_stack_to_global(ABTI_stack_header *p_sh);

char *ABTI_mem_alloc_sp(ABTI_local *p_local, size_t stacksize);


/******************************************************************************
 * Unless the stack is given by the user, we allocate a stack first and then
 * use the beginning of the allocated stack for allocating ABTI_thread and
 * ABTI_stack_header.  This way we need only one memory allocation call (e.g.,
 * ABTU_malloc).  The memory layout of the allocated stack will look like
 *  |-------------------|
 *  | ABTI_thread       |
 *  |-------------------|
 *  | ABTI_stack_header |
 *  |-------------------|
 *  | actual stack area |
 *  |-------------------|
 * Thus, the actual size of stack becomes
 * (requested stack size) - sizeof(ABTI_thread) - sizeof(ABTI_stack_header)
 * and it is set in the attribute field of ABTI_thread.
 *****************************************************************************/

/* Inline functions */
static inline
ABTI_thread *ABTI_mem_alloc_thread_with_stacksize(size_t stacksize,
                                                  ABTI_thread_attr *p_attr)
{
    size_t actual_stacksize;
    char *p_blk;
    ABTI_thread *p_thread;
    void *p_stack;

    /* Get the stack size */
    actual_stacksize = stacksize - sizeof(ABTI_thread);

    /* Allocate a stack */
    p_blk = (char *)ABTU_CA_MALLOC(stacksize);

    /* Allocate ABTI_thread, ABTI_stack_header, and the actual stack area in
     * the allocated stack memory */
    p_thread = (ABTI_thread *)p_blk;
    p_stack = (void *)(p_blk + sizeof(ABTI_thread));

    /* Set attributes */
    if (p_attr) {
        /* Copy p_attr. */
        ABTI_thread_attr_copy(&p_thread->attr, p_attr);
        p_thread->attr.stacksize = actual_stacksize;
        p_thread->attr.p_stack = p_stack;
        p_thread->attr.stacktype = ABTI_STACK_TYPE_MALLOC;
    } else {
        /* Initialize p_attr. */
        ABTI_thread_attr_init(&p_thread->attr, p_stack, actual_stacksize,
                              ABTI_STACK_TYPE_MALLOC, ABT_TRUE);
    }

    ABTI_VALGRIND_REGISTER_STACK(p_thread->attr.p_stack, actual_stacksize);
    return p_thread;
}

static inline
ABTI_thread *ABTI_mem_alloc_thread(ABTI_thread_attr *p_attr)
{
    /* Basic idea: allocate a memory for stack and use the first some memory as
     * ABTI_stack_header and ABTI_thread. So, the effective stack area is
     * reduced as much as the size of ABTI_stack_header and ABTI_thread. */

    size_t stacksize, def_stacksize, actual_stacksize;
    ABTI_local *p_local = lp_ABTI_local;
    char *p_blk = NULL;
    ABTI_thread *p_thread;
    ABTI_stack_header *p_sh;
    void *p_stack;

    /* Get the stack size */
    def_stacksize = ABTI_global_get_thread_stacksize();
    if (p_attr == NULL) {
        stacksize = def_stacksize;

#ifndef ABT_CONFIG_DISABLE_EXT_THREAD
        /* If an external thread allocates a stack, we use ABTU_malloc. */
        if (p_local == NULL) {
            return ABTI_mem_alloc_thread_with_stacksize(stacksize, NULL);
        }
#endif

    } else {
        if (p_attr->stacktype == ABTI_STACK_TYPE_USER ||
            p_attr->stacktype == ABTI_STACK_TYPE_MAIN) {
            /* Since the stack is given by the user, we create ABTI_thread and
             * ABTI_stack_header explicitly with a single ABTU_malloc call. */
            p_thread = (ABTI_thread *)ABTU_CA_MALLOC(sizeof(ABTI_thread));
            ABTI_thread_attr_copy(&p_thread->attr, p_attr);

            if (p_attr->stacktype != ABTI_STACK_TYPE_MAIN) {
                ABTI_VALGRIND_REGISTER_STACK(p_thread->attr.p_stack,
                                             p_attr->stacksize);
            }
            return p_thread;
        }

        stacksize = p_attr->stacksize;
        if (stacksize != def_stacksize) {
            /* Since the stack size requested is not the same as default one,
             * we use ABTU_malloc. */
            return ABTI_mem_alloc_thread_with_stacksize(stacksize, p_attr);
        }
    }

    /* Use the stack pool */
    if (p_local->p_mem_stack) {
        /* ES's stack pool has an available stack */
        p_sh = p_local->p_mem_stack;
        p_local->p_mem_stack = p_sh->p_next;
        p_local->num_stacks--;

        p_sh->p_next = NULL;
        p_blk = (char *)p_sh - sizeof(ABTI_thread);

    } else {
        /* Check stacks in the global data */
        if (gp_ABTI_global->p_mem_stack) {
            p_blk = ABTI_mem_take_global_stack(p_local);
            if (p_blk == NULL) {
                p_blk = ABTI_mem_alloc_sp(p_local, stacksize);
            }
        } else {
            /* Allocate a new stack if we don't have any empty stack */
            p_blk = ABTI_mem_alloc_sp(p_local, stacksize);
        }

        p_sh = (ABTI_stack_header *)(p_blk + sizeof(ABTI_thread));
        p_sh->p_next = NULL;
    }

    /* Actual stack size */
    actual_stacksize = stacksize - ABTI_MEM_SH_SIZE;

    /* Get the ABTI_thread pointer and stack pointer */
    p_thread = (ABTI_thread *)p_blk;
    p_stack  = p_sh->p_stack;

    /* Set attributes */
    if (p_attr == NULL) {
        ABTI_thread_attr *p_myattr = &p_thread->attr;
        ABTI_thread_attr_init(p_myattr, p_stack, actual_stacksize,
                              ABTI_STACK_TYPE_MEMPOOL, ABT_TRUE);
    } else {
        ABTI_thread_attr_copy(&p_thread->attr, p_attr);
        p_thread->attr.stacksize = actual_stacksize;
        p_thread->attr.p_stack = p_stack;
    }

    ABTI_VALGRIND_REGISTER_STACK(p_thread->attr.p_stack, actual_stacksize);
    return p_thread;
}

static inline
void ABTI_mem_free_thread(ABTI_thread *p_thread)
{
    ABTI_local *p_local;
    ABTI_stack_header *p_sh;
    ABTI_VALGRIND_UNREGISTER_STACK(p_thread->attr.p_stack);

    if (p_thread->attr.stacktype != ABTI_STACK_TYPE_MEMPOOL) {
        ABTU_free((void *)p_thread);
        return;
    }
    p_local = lp_ABTI_local;
    p_sh = (ABTI_stack_header *)((char *)p_thread + sizeof(ABTI_thread));

#ifndef ABT_CONFIG_DISABLE_EXT_THREAD
    if (!p_local) {
        /* This thread has been allocated internally,
         * but now is being freed by an external thread. */
        ABTI_mem_add_stack_to_global(p_sh);
        return;
    }
#endif

    if (p_local->num_stacks <= gp_ABTI_global->mem_max_stacks) {
        p_sh->p_next = p_local->p_mem_stack;
        p_local->p_mem_stack = p_sh;
        p_local->num_stacks++;
    } else {
        ABTI_mem_add_stack_to_global(p_sh);
    }
}

#else /* ABT_CONFIG_USE_MEM_POOL */

static inline
ABTI_thread *ABTI_mem_alloc_thread_with_stacksize(size_t *p_stacksize)
{
    size_t stacksize, actual_stacksize;
    char *p_blk;
    void *p_stack;
    ABTI_thread *p_thread;

    /* Get the stack size */
    stacksize = *p_stacksize;
    actual_stacksize = stacksize - sizeof(ABTI_thread);

    /* Allocate ABTI_thread and a stack */
    p_blk = (char *)ABTU_CA_MALLOC(stacksize);
    p_thread = (ABTI_thread *)p_blk;
    p_stack = (void *)(p_blk + sizeof(ABTI_thread));

    /* Set attributes */
    ABTI_thread_attr *p_myattr = &p_thread->attr;
    ABTI_thread_attr_init(p_myattr, p_stack, actual_stacksize,
                          ABTI_STACK_TYPE_MALLOC, ABT_TRUE);

    *p_stacksize = actual_stacksize;
    ABTI_VALGRIND_REGISTER_STACK(p_thread->attr.p_stack, *p_stacksize);
    return p_thread;
}

static inline
ABTI_thread *ABTI_mem_alloc_thread(ABT_thread_attr attr, size_t *p_stacksize)
{
    ABTI_thread *p_thread;

    if (attr == ABT_THREAD_ATTR_NULL) {
        *p_stacksize = ABTI_global_get_thread_stacksize();
        return ABTI_mem_alloc_thread_with_stacksize(p_stacksize);
    }

    /* Allocate a stack and set attributes */
    ABTI_thread_attr *p_attr = ABTI_thread_attr_get_ptr(attr);
    if (p_attr->p_stack == NULL) {
        ABTI_ASSERT(p_attr->userstack == ABT_FALSE);

        char *p_blk = (char *)ABTU_CA_MALLOC(p_attr->stacksize);
        p_thread = (ABTI_thread *)p_blk;

        ABTI_thread_attr_copy(&p_thread->attr, p_attr);
        p_thread->attr.stacksize -= sizeof(ABTI_thread);
        p_thread->attr.p_stack = (void *)(p_blk + sizeof(ABTI_thread));

    } else {
        /* Since the stack is given by the user, we create ABTI_thread
         * explicitly instead of using a part of stack because the stack
         * will be freed by the user. */
        p_thread = (ABTI_thread *)ABTU_CA_MALLOC(sizeof(ABTI_thread));
        ABTI_thread_attr_copy(&p_thread->attr, p_attr);
    }

    *p_stacksize = p_thread->attr.stacksize;
    return p_thread;
}

static inline
ABTI_thread *ABTI_mem_alloc_main_thread(ABT_thread_attr attr)
{
    ABTI_thread *p_thread;

    p_thread = (ABTI_thread *)ABTU_CA_MALLOC(sizeof(ABTI_thread));

    /* Set attributes */
    /* TODO: Need to set the actual stack address and size for the main ULT */
    ABTI_thread_attr *p_attr = &p_thread->attr;
    ABTI_thread_attr_init(p_attr, NULL, 0, ABTI_STACK_TYPE_MAIN, ABT_FALSE);

    return p_thread;
}

static inline
void ABTI_mem_free_thread(ABTI_thread *p_thread)
{
    ABTI_VALGRIND_UNREGISTER_STACK(p_thread->attr.p_stack);
    ABTU_free(p_thread);
}

#endif /* ABT_CONFIG_USE_MEM_POOL */

#endif /* ABTI_MEM_THREAD_H_INCLUDED */

