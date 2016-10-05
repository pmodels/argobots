/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#ifndef ABTI_MEM_H_INCLUDED
#define ABTI_MEM_H_INCLUDED

/* Memory allocation */

#ifdef ABT_CONFIG_USE_ALIGNED_ALLOC
#define ABTU_CA_MALLOC(s)   ABTU_memalign(gp_ABTI_global->cache_line_size,s)
#else
#define ABTU_CA_MALLOC(s)   ABTU_malloc(s)
#endif

#ifdef ABT_CONFIG_USE_MEM_POOL
typedef struct ABTI_blk_header  ABTI_blk_header;

enum {
    ABTI_MEM_LP_MALLOC = 0,
    ABTI_MEM_LP_MMAP_RP,
    ABTI_MEM_LP_MMAP_HP_RP,
    ABTI_MEM_LP_MMAP_HP_THP,
    ABTI_MEM_LP_THP
};

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

struct ABTI_page_header {
    uint32_t blk_size;          /* Block size in bytes */
    uint32_t num_total_blks;    /* Number of total blocks */
    uint32_t num_empty_blks;    /* Number of empty blocks */
    uint32_t num_remote_free;   /* Number of remote free blocks */
    ABTI_blk_header *p_head;    /* First empty block */
    ABTI_blk_header *p_free;    /* For remote free */
    ABTI_xstream *p_owner;      /* Owner ES */
    ABTI_page_header *p_prev;   /* Prev page header */
    ABTI_page_header *p_next;   /* Next page header */
    ABT_bool is_mmapped;        /* ABT_TRUE if it is mmapped */
};

struct ABTI_blk_header {
    ABTI_page_header *p_ph;     /* Page header */
    ABTI_blk_header *p_next;    /* Next block header */
};

void ABTI_mem_init(ABTI_global *p_global);
void ABTI_mem_init_local(ABTI_local *p_local);
void ABTI_mem_finalize(ABTI_global *p_global);
void ABTI_mem_finalize_local(ABTI_local *p_local);
int ABTI_mem_check_lp_alloc(int lp_alloc);

char *ABTI_mem_take_global_stack(ABTI_local *p_local);
void ABTI_mem_add_stack_to_global(ABTI_stack_header *p_sh);
ABTI_page_header *ABTI_mem_alloc_page(ABTI_local *p_local, size_t blk_size);
void ABTI_mem_free_page(ABTI_local *p_local, ABTI_page_header *p_ph);
void ABTI_mem_take_free(ABTI_page_header *p_ph);
void ABTI_mem_free_remote(ABTI_page_header *p_ph, ABTI_blk_header *p_bh);
ABTI_page_header *ABTI_mem_take_global_page(ABTI_local *p_local);

char *ABTI_mem_alloc_sp(ABTI_local *p_local, size_t stacksize);

/* ABTI_EXT_STACK means that the block will not be managed by ES's stack pool
 * and it needs to be freed with ABTU_free. */
#define ABTI_EXT_STACK      (ABTI_stack_header *)(UINTPTR_MAX)


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
ABTI_thread *ABTI_mem_alloc_thread_with_stacksize(size_t *p_stacksize)
{
    const size_t header_size = gp_ABTI_global->mem_sh_size;
    size_t stacksize, actual_stacksize;
    char *p_blk;
    ABTI_thread *p_thread;
    ABTI_stack_header *p_sh;
    void *p_stack;

    /* Get the stack size */
    stacksize = *p_stacksize;
    actual_stacksize = stacksize - header_size;

    /* Allocate a stack */
    p_blk = (char *)ABTU_CA_MALLOC(stacksize);

    /* Allocate ABTI_thread, ABTI_stack_header, and the actual stack area in
     * the allocated stack memory */
    p_thread = (ABTI_thread *)p_blk;
    p_sh = (ABTI_stack_header *)(p_blk + sizeof(ABTI_thread));
    p_sh->p_next = ABTI_EXT_STACK;
    p_stack = (void *)(p_blk + header_size);

    /* Set attributes */
    ABTI_thread_attr *p_myattr = &p_thread->attr;
    ABTI_thread_attr_init(p_myattr, p_stack, actual_stacksize, ABT_TRUE);

    *p_stacksize = actual_stacksize;
    return p_thread;
}

static inline
ABTI_thread *ABTI_mem_alloc_thread(ABT_thread_attr attr, size_t *p_stacksize)
{
    /* Basic idea: allocate a memory for stack and use the first some memory as
     * ABTI_stack_header and ABTI_thread. So, the effective stack area is
     * reduced as much as the size of ABTI_stack_header and ABTI_thread. */

    const size_t header_size = gp_ABTI_global->mem_sh_size;
    size_t stacksize, def_stacksize, actual_stacksize;
    ABTI_local *p_local = lp_ABTI_local;
    char *p_blk = NULL;
    ABTI_thread *p_thread;
    ABTI_stack_header *p_sh;
    void *p_stack;

    /* Get the stack size */
    def_stacksize = ABTI_global_get_thread_stacksize();
    if (attr == ABT_THREAD_ATTR_NULL) {
        stacksize = def_stacksize;

#ifndef ABT_CONFIG_DISABLE_EXT_THREAD
        /* If an external thread allocates a stack, we use ABTU_malloc. */
        if (p_local == NULL) {
            *p_stacksize = stacksize;
            return ABTI_mem_alloc_thread_with_stacksize(p_stacksize);
        }
#endif

    } else {
        ABTI_thread_attr *p_attr = ABTI_thread_attr_get_ptr(attr);

        if (p_attr->p_stack != NULL) {
            /* Since the stack is given by the user, we create ABTI_thread and
             * ABTI_stack_header explicitly with a single ABTU_malloc call. */
            p_blk = (char *)ABTU_CA_MALLOC(header_size);

            p_sh = (ABTI_stack_header *)(p_blk + sizeof(ABTI_thread));
            p_sh->p_next = ABTI_EXT_STACK;

            p_thread = (ABTI_thread *)p_blk;
            ABTI_thread_attr_copy(&p_thread->attr, p_attr);

            *p_stacksize = p_attr->stacksize;
            return p_thread;
        }

        stacksize = p_attr->stacksize;
        if (stacksize != def_stacksize) {
            /* Since the stack size requested is not the same as default one,
             * we use ABTU_malloc. */
            p_blk = (char *)ABTU_CA_MALLOC(stacksize);

            p_sh = (ABTI_stack_header *)(p_blk + sizeof(ABTI_thread));
            p_sh->p_next = ABTI_EXT_STACK;

            actual_stacksize = stacksize - header_size;
            p_thread = (ABTI_thread *)p_blk;
            ABTI_thread_attr_copy(&p_thread->attr, p_attr);
            p_thread->attr.stacksize = actual_stacksize;
            p_thread->attr.p_stack = (void *)(p_blk + header_size);

            *p_stacksize = actual_stacksize;
            return p_thread;
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
    actual_stacksize = stacksize - header_size;

    /* Get the ABTI_thread pointer and stack pointer */
    p_thread = (ABTI_thread *)p_blk;
    p_stack  = p_sh->p_stack;

    /* Set attributes */
    if (attr == ABT_THREAD_ATTR_NULL) {
        ABTI_thread_attr *p_myattr = &p_thread->attr;
        ABTI_thread_attr_init(p_myattr, p_stack, actual_stacksize, ABT_TRUE);
    } else {
        ABTI_thread_attr *p_attr = ABTI_thread_attr_get_ptr(attr);
        ABTI_thread_attr_copy(&p_thread->attr, p_attr);
        p_thread->attr.stacksize = actual_stacksize;
        p_thread->attr.p_stack = p_stack;
    }

    *p_stacksize = actual_stacksize;
    return p_thread;
}

static inline
ABTI_thread *ABTI_mem_alloc_main_thread(ABT_thread_attr attr)
{
    const size_t header_size = sizeof(ABTI_stack_header) + sizeof(ABTI_thread);
    char *p_blk;
    ABTI_stack_header *p_sh;
    ABTI_thread *p_thread;

    p_blk = (char *)ABTU_CA_MALLOC(header_size);

    p_sh = (ABTI_stack_header *)(p_blk + sizeof(ABTI_thread));
    p_sh->p_next = ABTI_EXT_STACK;

    p_thread = (ABTI_thread *)p_blk;

    /* Set attributes */
    /* TODO: Need to set the actual stack address and size for the main ULT */
    ABTI_thread_attr *p_attr = &p_thread->attr;
    ABTI_thread_attr_init(p_attr, NULL, 0, ABT_FALSE);

    return p_thread;
}

static inline
void ABTI_mem_free_thread(ABTI_thread *p_thread)
{
    ABTI_local *p_local;
    ABTI_stack_header *p_sh;

    p_sh = (ABTI_stack_header *)((char *)p_thread + sizeof(ABTI_thread));

    if (p_sh->p_next == ABTI_EXT_STACK) {
        ABTU_free((void *)p_thread);
        return;
    }

    p_local = lp_ABTI_local;
    if (p_local->num_stacks <= gp_ABTI_global->mem_max_stacks) {
        p_sh->p_next = p_local->p_mem_stack;
        p_local->p_mem_stack = p_sh;
        p_local->num_stacks++;
    } else {
        ABTI_mem_add_stack_to_global(p_sh);
    }
}

static inline
ABTI_task *ABTI_mem_alloc_task(void)
{
    ABTI_task *p_task = NULL;
    ABTI_local *p_local = lp_ABTI_local;
    const size_t blk_size = sizeof(ABTI_blk_header) + sizeof(ABTI_task);

#ifndef ABT_CONFIG_DISABLE_EXT_THREAD
    if (p_local == NULL) {
        /* For external threads */
        char *p_blk = (char *)ABTU_CA_MALLOC(blk_size);
        ABTI_blk_header *p_blk_header = (ABTI_blk_header *)p_blk;
        p_blk_header->p_ph = NULL;
        p_task = (ABTI_task *)(p_blk + sizeof(ABTI_blk_header));
        return p_task;
    }
#endif

    /* Find the page that has an empty block */
    ABTI_page_header *p_ph = p_local->p_mem_task_head;
    while (p_ph) {
        if (p_ph->p_head) break;
        if (p_ph->p_free) {
            ABTI_mem_take_free(p_ph);
            break;
        }

        p_ph = p_ph->p_next;
        if (p_ph == p_local->p_mem_task_head) {
            p_ph = NULL;
            break;
        }
    }

    /* If there is no page that has an empty block */
    if (p_ph == NULL) {
        /* Check pages in the global data */
        if (gp_ABTI_global->p_mem_task) {
            p_ph = ABTI_mem_take_global_page(p_local);
            if (p_ph == NULL) {
                p_ph = ABTI_mem_alloc_page(p_local, blk_size);
            }
        } else {
            /* Allocate a new page */
            p_ph = ABTI_mem_alloc_page(p_local, blk_size);
        }
    }

    ABTI_blk_header *p_head = p_ph->p_head;
    p_ph->p_head = p_head->p_next;
    p_ph->num_empty_blks--;

    p_task = (ABTI_task *)((char *)p_head + sizeof(ABTI_blk_header));

    return p_task;
}

static inline
void ABTI_mem_free_task(ABTI_task *p_task)
{
    ABTI_local *p_local;
    ABTI_blk_header *p_head;
    ABTI_page_header *p_ph;

    p_head = (ABTI_blk_header *)((char *)p_task - sizeof(ABTI_blk_header));
    p_ph = p_head->p_ph;

#ifndef ABT_CONFIG_DISABLE_EXT_THREAD
    if (p_ph == NULL) {
        /* This was allocated by an external thread. */
        ABTU_free(p_head);
        return;
    }
#endif

    p_local = lp_ABTI_local;
    if (p_ph->p_owner == p_local->p_xstream) {
        p_head->p_next = p_ph->p_head;
        p_ph->p_head = p_head;
        p_ph->num_empty_blks++;

        /* TODO: Need to decrease the number of pages */
        /* ABTI_mem_free_page(p_local, p_ph); */
    } else {
        /* Remote free */
        ABTI_mem_free_remote(p_ph, p_head);
    }
}

#else /* ABT_CONFIG_USE_MEM_POOL */

#define ABTI_mem_init(p)
#define ABTI_mem_init_local(p)
#define ABTI_mem_finalize(p)
#define ABTI_mem_finalize_local(p)

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
    ABTI_thread_attr_init(p_myattr, p_stack, actual_stacksize, ABT_TRUE);

    *p_stacksize = actual_stacksize;
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
    ABTI_thread_attr_init(p_attr, NULL, 0, ABT_FALSE);

    return p_thread;
}

static inline
void ABTI_mem_free_thread(ABTI_thread *p_thread)
{
    ABTU_free(p_thread);
}

static inline
ABTI_task *ABTI_mem_alloc_task(void)
{
    return (ABTI_task *)ABTU_CA_MALLOC(sizeof(ABTI_task));
}

static inline
void ABTI_mem_free_task(ABTI_task *p_task)
{
    ABTU_free(p_task);
}

#endif /* ABT_CONFIG_USE_MEM_POOL */

#endif /* ABTI_MEM_H_INCLUDED */

