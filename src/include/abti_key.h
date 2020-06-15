/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#ifndef ABTI_KEY_H_INCLUDED
#define ABTI_KEY_H_INCLUDED

/* Inlined functions for Work unit-specific data key */

static inline ABTI_key *ABTI_key_get_ptr(ABT_key key)
{
#ifndef ABT_CONFIG_DISABLE_ERROR_CHECK
    ABTI_key *p_key;
    if (key == ABT_KEY_NULL) {
        p_key = NULL;
    } else {
        p_key = (ABTI_key *)key;
    }
    return p_key;
#else
    return (ABTI_key *)key;
#endif
}

static inline ABT_key ABTI_key_get_handle(ABTI_key *p_key)
{
#ifndef ABT_CONFIG_DISABLE_ERROR_CHECK
    ABT_key h_key;
    if (p_key == NULL) {
        h_key = ABT_KEY_NULL;
    } else {
        h_key = (ABT_key)p_key;
    }
    return h_key;
#else
    return (ABT_key)p_key;
#endif
}

typedef struct ABTI_ktable_mem_header {
    struct ABTI_ktable_mem_header *p_next;
    ABT_bool is_from_mempool;
} ABTI_ktable_mem_header;

#define ABTI_KTABLE_DESC_SIZE                                                  \
    (ABTI_MEM_POOL_DESC_SIZE - sizeof(ABTI_ktable_mem_header))

static inline ABTI_ktable *ABTI_ktable_alloc(ABTI_xstream *p_local_xstream,
                                             int size)
{
    /* size must be a power of 2. */
    ABTI_ASSERT((size & (size - 1)) == 0);
    /* max alignment must be a power of 2. */
    ABTI_STATIC_ASSERT((ABTU_MAX_ALIGNMENT & (ABTU_MAX_ALIGNMENT - 1)) == 0);
    size_t ktable_size =
        (offsetof(ABTI_ktable, p_elems) + sizeof(ABTD_atomic_ptr) * size +
         ABTU_MAX_ALIGNMENT - 1) &
        (~(ABTU_MAX_ALIGNMENT - 1));
    ABTI_ktable *p_ktable;
    /* Since only one ES can access the memory pool on creation, this uses an
     * unsafe memory pool without taking a lock. */
    if (ABTU_likely(ktable_size <= ABTI_KTABLE_DESC_SIZE)) {
        /* Use memory pool. */
        void *p_mem = ABTI_mem_alloc_desc(p_local_xstream);
        ABTI_ktable_mem_header *p_header = (ABTI_ktable_mem_header *)p_mem;
        p_ktable =
            (ABTI_ktable *)(((char *)p_mem) + sizeof(ABTI_ktable_mem_header));
        p_header->p_next = NULL;
        p_header->is_from_mempool = ABT_TRUE;
        p_ktable->p_used_mem = p_mem;
        p_ktable->p_extra_mem = (void *)(((char *)p_ktable) + ktable_size);
        p_ktable->extra_mem_size = ABTI_KTABLE_DESC_SIZE - ktable_size;
    } else {
        /* Use malloc() */
        void *p_mem = ABTU_malloc(ktable_size + sizeof(ABTI_ktable_mem_header));
        ABTI_ktable_mem_header *p_header = (ABTI_ktable_mem_header *)p_mem;
        p_ktable =
            (ABTI_ktable *)(((char *)p_mem) + sizeof(ABTI_ktable_mem_header));
        p_header->p_next = NULL;
        p_header->is_from_mempool = ABT_FALSE;
        p_ktable->p_used_mem = p_mem;
        p_ktable->p_extra_mem = NULL;
        p_ktable->extra_mem_size = 0;
    }
    p_ktable->size = size;
    memset(p_ktable->p_elems, 0, sizeof(ABTD_atomic_ptr) * size);
    return p_ktable;
}

static inline void *ABTI_ktable_alloc_elem(ABTI_xstream *p_local_xstream,
                                           ABTI_ktable *p_ktable, size_t size)
{
    ABTI_ASSERT((size & (ABTU_MAX_ALIGNMENT - 1)) == 0);
    size_t extra_mem_size = p_ktable->extra_mem_size;
    if (size <= extra_mem_size) {
        /* Use the extra memory. */
        void *p_ret = p_ktable->p_extra_mem;
        p_ktable->p_extra_mem = (void *)(((char *)p_ret) + size);
        p_ktable->extra_mem_size = extra_mem_size - size;
        return p_ret;
    } else if (ABTU_likely(size <= ABTI_KTABLE_DESC_SIZE)) {
        /* Use memory pool. */
        void *p_mem = ABTI_mem_alloc_desc(p_local_xstream);
        ABTI_ktable_mem_header *p_header = (ABTI_ktable_mem_header *)p_mem;
        p_header->p_next = (ABTI_ktable_mem_header *)p_ktable->p_used_mem;
        p_header->is_from_mempool = ABT_TRUE;
        p_ktable->p_used_mem = (void *)p_header;
        p_mem = (void *)(((char *)p_mem) + sizeof(ABTI_ktable_mem_header));
        p_ktable->p_extra_mem = (void *)(((char *)p_mem) + size);
        p_ktable->extra_mem_size = ABTI_KTABLE_DESC_SIZE - size;
        return p_mem;
    } else {
        /* Use malloc() */
        void *p_mem = ABTU_malloc(size + sizeof(ABTI_ktable_mem_header));
        ABTI_ktable_mem_header *p_header = (ABTI_ktable_mem_header *)p_mem;
        p_header->p_next = (ABTI_ktable_mem_header *)p_ktable->p_used_mem;
        p_header->is_from_mempool = ABT_FALSE;
        p_ktable->p_used_mem = (void *)p_header;
        p_mem = (void *)(((char *)p_mem) + sizeof(ABTI_ktable_mem_header));
        return p_mem;
    }
}

static inline uint32_t ABTI_ktable_get_idx(ABTI_key *p_key, int size)
{
    return p_key->id & (size - 1);
}

static inline void ABTI_ktable_set(ABTI_xstream *p_local_xstream,
                                   ABTI_ktable *p_ktable, ABTI_key *p_key,
                                   void *value)
{
    uint32_t idx;
    ABTI_ktelem *p_elem;

    /* Look for the same key */
    idx = ABTI_ktable_get_idx(p_key, p_ktable->size);
    p_elem = p_ktable->p_elems[idx];
    uint32_t key_id = p_key->id;
    while (p_elem) {
        if (p_elem->key_id == key_id) {
            p_elem->value = value;
            return;
        }
        p_elem = p_elem->p_next;
    }

    /* The table does not have the same key */
    ABTI_STATIC_ASSERT((ABTU_MAX_ALIGNMENT & (ABTU_MAX_ALIGNMENT - 1)) == 0);
    size_t ktelem_size = (sizeof(ABTI_ktelem) + ABTU_MAX_ALIGNMENT - 1) &
                         (~(ABTU_MAX_ALIGNMENT - 1));
    p_elem = (ABTI_ktelem *)ABTI_ktable_alloc_elem(p_local_xstream, p_ktable,
                                                   ktelem_size);
    p_elem->f_destructor = p_key->f_destructor;
    p_elem->key_id = p_key->id;
    p_elem->value = value;
    p_elem->p_next = p_ktable->p_elems[idx];
    p_ktable->p_elems[idx] = p_elem;
}

static inline void *ABTI_ktable_get(ABTI_ktable *p_ktable, ABTI_key *p_key)
{
    uint32_t idx;
    ABTI_ktelem *p_elem;

    idx = ABTI_ktable_get_idx(p_key, p_ktable->size);
    p_elem = p_ktable->p_elems[idx];
    uint32_t key_id = p_key->id;
    while (p_elem) {
        if (p_elem->key_id == key_id) {
            return p_elem->value;
        }
        p_elem = p_elem->p_next;
    }

    return NULL;
}

#endif /* ABTI_KEY_H_INCLUDED */
