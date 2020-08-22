/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#ifndef ABTI_MEM_H_INCLUDED
#define ABTI_MEM_H_INCLUDED

/* Memory allocation */

/* Round desc_size up to the cacheline size.  The last four bytes will be
 * used to determine whether the descriptor is allocated externally (i.e.,
 * malloc()) or taken from a memory pool. */
#define ABTI_MEM_POOL_DESC_SIZE                                                \
    (((sizeof(ABTI_thread) + 4 + ABT_CONFIG_STATIC_CACHELINE_SIZE - 1) &       \
      (~(ABT_CONFIG_STATIC_CACHELINE_SIZE - 1))) -                             \
     4)

enum {
    ABTI_MEM_LP_MALLOC = 0,
    ABTI_MEM_LP_MMAP_RP,
    ABTI_MEM_LP_MMAP_HP_RP,
    ABTI_MEM_LP_MMAP_HP_THP,
    ABTI_MEM_LP_THP
};

void ABTI_mem_init(ABTI_global *p_global);
void ABTI_mem_init_local(ABTI_xstream *p_local_xstream);
void ABTI_mem_finalize(ABTI_global *p_global);
void ABTI_mem_finalize_local(ABTI_xstream *p_local_xstream);
int ABTI_mem_check_lp_alloc(int lp_alloc);

/* Inline functions */
#ifdef ABT_CONFIG_USE_MEM_POOL
static inline void
ABTI_mem_alloc_ythread_mempool_impl(ABTI_mem_pool_local_pool *p_mem_pool_stack,
                                    size_t stacksize, ABTI_ythread **pp_ythread,
                                    void **pp_stack)
{
    /* stacksize must be a multiple of ABT_CONFIG_STATIC_CACHELINE_SIZE. */
    ABTI_ASSERT((stacksize & (ABT_CONFIG_STATIC_CACHELINE_SIZE - 1)) == 0);
    char *p_ythread = (char *)ABTI_mem_pool_alloc(p_mem_pool_stack);
    *pp_stack = (void *)(((char *)p_ythread) - stacksize);
    *pp_ythread = (ABTI_ythread *)p_ythread;
}
#endif

static inline void ABTI_mem_alloc_ythread_malloc_impl(size_t stacksize,
                                                      ABTI_ythread **pp_ythread,
                                                      void **pp_stack)
{
    /* stacksize must be a multiple of ABT_CONFIG_STATIC_CACHELINE_SIZE. */
    size_t alloc_stacksize =
        (stacksize + ABT_CONFIG_STATIC_CACHELINE_SIZE - 1) &
        (~(ABT_CONFIG_STATIC_CACHELINE_SIZE - 1));
    char *p_stack = (char *)ABTU_malloc(alloc_stacksize + sizeof(ABTI_ythread));
    *pp_stack = (void *)p_stack;
    *pp_ythread = (ABTI_ythread *)(p_stack + alloc_stacksize);
}

static inline ABTI_ythread *ABTI_mem_alloc_ythread_default(ABTI_local *p_local)
{
    size_t stacksize = ABTI_global_get_thread_stacksize();
    ABTI_ythread *p_ythread;
    void *p_stack;
    /* If an external thread allocates a stack, we use ABTU_malloc. */
    ABTI_xstream *p_local_xstream = ABTI_local_get_xstream_or_null(p_local);
    if (ABTI_IS_EXT_THREAD_ENABLED && p_local_xstream == NULL) {
        ABTI_mem_alloc_ythread_malloc_impl(stacksize, &p_ythread, &p_stack);
        p_ythread->stacktype = ABTI_STACK_TYPE_MALLOC;
    } else {
#ifdef ABT_CONFIG_USE_MEM_POOL
        ABTI_mem_alloc_ythread_mempool_impl(&p_local_xstream->mem_pool_stack,
                                            stacksize, &p_ythread, &p_stack);
        p_ythread->stacktype = ABTI_STACK_TYPE_MEMPOOL;
#else
        ABTI_mem_alloc_ythread_malloc_impl(stacksize, &p_ythread, &p_stack);
        p_ythread->stacktype = ABTI_STACK_TYPE_MALLOC;
#endif
    }
    /* Initialize members of ABTI_thread_attr. */
    p_ythread->p_stack = p_stack;
    p_ythread->stacksize = stacksize;
    ABTI_VALGRIND_REGISTER_STACK(p_ythread->p_stack, p_ythread->stacksize);
    return p_ythread;
}

#ifdef ABT_CONFIG_USE_MEM_POOL
static inline ABTI_ythread *
ABTI_mem_alloc_ythread_mempool(ABTI_local *p_local, ABTI_thread_attr *p_attr)
{
    size_t stacksize = ABTI_global_get_thread_stacksize();
    ABTI_ythread *p_ythread;
    void *p_stack;
    /* If an external thread allocates a stack, we use ABTU_malloc. */
    ABTI_xstream *p_local_xstream = ABTI_local_get_xstream_or_null(p_local);
    if (ABTI_IS_EXT_THREAD_ENABLED && p_local_xstream == NULL) {
        ABTI_mem_alloc_ythread_malloc_impl(stacksize, &p_ythread, &p_stack);
        p_ythread->stacktype = ABTI_STACK_TYPE_MALLOC;
    } else {
        ABTI_mem_alloc_ythread_mempool_impl(&p_local_xstream->mem_pool_stack,
                                            stacksize, &p_ythread, &p_stack);
        p_ythread->stacktype = ABTI_STACK_TYPE_MEMPOOL;
    }
    /* Copy members of p_attr. */
    p_ythread->p_stack = p_stack;
    p_ythread->stacksize = stacksize;
    ABTI_VALGRIND_REGISTER_STACK(p_ythread->p_stack, p_ythread->stacksize);
    return p_ythread;
}
#endif

static inline ABTI_ythread *
ABTI_mem_alloc_ythread_malloc(ABTI_thread_attr *p_attr)
{
    size_t stacksize = p_attr->stacksize;
    ABTI_ythread *p_ythread;
    void *p_stack;
    ABTI_mem_alloc_ythread_malloc_impl(stacksize, &p_ythread, &p_stack);
    /* Copy members of p_attr. */
    p_ythread->stacktype = ABTI_STACK_TYPE_MALLOC;
    p_ythread->stacksize = stacksize;
    p_ythread->p_stack = p_stack;
    ABTI_VALGRIND_REGISTER_STACK(p_ythread->p_stack, p_ythread->stacksize);
    return p_ythread;
}

static inline ABTI_ythread *
ABTI_mem_alloc_ythread_user(ABTI_thread_attr *p_attr)
{
    /* Do not allocate stack, but Valgrind registration is preferred. */
    ABTI_ythread *p_ythread = (ABTI_ythread *)ABTU_malloc(sizeof(ABTI_ythread));
    /* Copy members of p_attr. */
    p_ythread->stacktype = ABTI_STACK_TYPE_USER;
    p_ythread->stacksize = p_attr->stacksize;
    p_ythread->p_stack = p_attr->p_stack;
    ABTI_VALGRIND_REGISTER_STACK(p_ythread->p_stack, p_ythread->stacksize);
    return p_ythread;
}

static inline ABTI_ythread *
ABTI_mem_alloc_ythread_main(ABTI_thread_attr *p_attr)
{
    /* Stack of the currently running Pthreads is used. */
    ABTI_ythread *p_ythread = (ABTI_ythread *)ABTU_malloc(sizeof(ABTI_ythread));
    /* Copy members of p_attr. */
    p_ythread->stacktype = ABTI_STACK_TYPE_MAIN;
    p_ythread->stacksize = p_attr->stacksize;
    p_ythread->p_stack = p_attr->p_stack;
    return p_ythread;
}

static inline void ABTI_mem_free_ythread(ABTI_local *p_local,
                                         ABTI_ythread *p_ythread)
{
    /* Return stack. */
#ifdef ABT_CONFIG_USE_MEM_POOL
    if (p_ythread->stacktype == ABTI_STACK_TYPE_MEMPOOL) {
        ABTI_VALGRIND_UNREGISTER_STACK(p_ythread->p_stack);
        ABTI_xstream *p_local_xstream = ABTI_local_get_xstream_or_null(p_local);
        /* Came from a memory pool. */
#ifndef ABT_CONFIG_DISABLE_EXT_THREAD
        if (p_local_xstream == NULL) {
            /* Return a stack to the global pool. */
            ABTI_spinlock_acquire(&gp_ABTI_global->mem_pool_stack_lock);
            ABTI_mem_pool_free(&gp_ABTI_global->mem_pool_stack_ext, p_ythread);
            ABTI_spinlock_release(&gp_ABTI_global->mem_pool_stack_lock);
            return;
        }
#endif
        ABTI_mem_pool_free(&p_local_xstream->mem_pool_stack, p_ythread);
    } else
#endif
        if (p_ythread->stacktype == ABTI_STACK_TYPE_MALLOC) {
        ABTI_VALGRIND_UNREGISTER_STACK(p_ythread->p_stack);
        /* p_ythread is allocated together with the stack. */
        ABTU_free(p_ythread->p_stack);
    } else {
        if (p_ythread->stacktype == ABTI_STACK_TYPE_USER) {
            ABTI_VALGRIND_UNREGISTER_STACK(p_ythread->p_stack);
        }
        ABTU_free(p_ythread);
    }
}

static inline void *ABTI_mem_alloc_desc(ABTI_local *p_local)
{
#ifndef ABT_CONFIG_USE_MEM_POOL
    return ABTU_malloc(ABTI_MEM_POOL_DESC_SIZE);
#else
    void *p_desc;

    ABTI_xstream *p_local_xstream = ABTI_local_get_xstream_or_null(p_local);
    if (ABTI_IS_EXT_THREAD_ENABLED && p_local_xstream == NULL) {
        /* For external threads */
        p_desc = ABTU_malloc(ABTI_MEM_POOL_DESC_SIZE);
        *(uint32_t *)(((char *)p_desc) + ABTI_MEM_POOL_DESC_SIZE) = 1;
        return p_desc;
    }

    /* Find the page that has an empty block */
    p_desc = ABTI_mem_pool_alloc(&p_local_xstream->mem_pool_desc);
    /* To distinguish it from a malloc'ed case, assign non-NULL value. */
    *(uint32_t *)(((char *)p_desc) + ABTI_MEM_POOL_DESC_SIZE) = 0;
    return p_desc;
#endif
}

static inline void ABTI_mem_free_desc(ABTI_local *p_local, void *p_desc)
{
#ifndef ABT_CONFIG_USE_MEM_POOL
    ABTU_free(p_desc);
#else
    ABTI_xstream *p_local_xstream = ABTI_local_get_xstream_or_null(p_local);
#ifndef ABT_CONFIG_DISABLE_EXT_THREAD
    if (*(uint32_t *)(((char *)p_desc) + ABTI_MEM_POOL_DESC_SIZE)) {
        /* This was allocated by an external thread. */
        ABTU_free(p_desc);
        return;
    } else if (!p_local_xstream) {
        /* Return a stack and a descriptor to their global pools. */
        ABTI_spinlock_acquire(&gp_ABTI_global->mem_pool_desc_lock);
        ABTI_mem_pool_free(&gp_ABTI_global->mem_pool_desc_ext, p_desc);
        ABTI_spinlock_release(&gp_ABTI_global->mem_pool_desc_lock);
        return;
    }
#endif
    ABTI_mem_pool_free(&p_local_xstream->mem_pool_desc, p_desc);
#endif
}

static inline ABTI_thread *ABTI_mem_alloc_task(ABTI_local *p_local)
{
    return (ABTI_thread *)ABTI_mem_alloc_desc(p_local);
}

static inline void ABTI_mem_free_task(ABTI_local *p_local, ABTI_thread *p_task)
{
    ABTI_mem_free_desc(p_local, (void *)p_task);
}

#endif /* ABTI_MEM_H_INCLUDED */
