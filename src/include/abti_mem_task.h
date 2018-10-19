/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#ifndef ABTI_MEM_TASK_H_INCLUDED
#define ABTI_MEM_TASK_H_INCLUDED

#ifdef ABT_CONFIG_USE_MEM_POOL
typedef struct ABTI_blk_header  ABTI_blk_header;

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

ABTI_page_header *ABTI_mem_alloc_page(ABTI_local *p_local, size_t blk_size);
void ABTI_mem_free_page(ABTI_local *p_local, ABTI_page_header *p_ph);
void ABTI_mem_take_free(ABTI_page_header *p_ph);
void ABTI_mem_free_remote(ABTI_page_header *p_ph, ABTI_blk_header *p_bh);
ABTI_page_header *ABTI_mem_take_global_page(ABTI_local *p_local);

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
    } else if (!lp_ABTI_local) {
        /* This task has been allocated internally,
         * but now is being freed by an external thread. */
        ABTI_mem_free_remote(p_ph, p_head);
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

#endif /* ABTI_MEM_TASK_H_INCLUDED */

