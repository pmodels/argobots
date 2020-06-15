/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"

/** @defgroup KEY Work-Unit Local Storage (TLS)
 * This group is for work-unit specific data, which can be described as
 * work-unit local storage (TLS).
 */

typedef struct ABTI_ktable_mem_header {
    struct ABTI_ktable_mem_header *p_next;
    ABT_bool is_from_mempool;
} ABTI_ktable_mem_header;
#define ABTI_KTABLE_DESC_SIZE                                                  \
    (ABTI_MEM_POOL_DESC_SIZE - sizeof(ABTI_ktable_mem_header))

static inline void ABTI_ktable_set(ABTI_xstream *p_local_xstream,
                                   ABTI_ktable *p_ktable, ABTI_key *p_key,
                                   void *value);
static inline void *ABTI_ktable_get(ABTI_ktable *p_ktable, ABTI_key *p_key);

static ABTD_atomic_uint32 g_key_id = ABTD_ATOMIC_UINT32_STATIC_INITIALIZER(0);

/**
 * @ingroup KEY
 * @brief   Create an WU-specific data key.
 *
 * \c ABT_key_create() creates a new work unit (WU)-specific data key visible
 * to all WUs (ULTs or tasklets) in the process and returns its handle through
 * \c newkey.  Although the same key may be used by different WUs, the values
 * bound to the key by \c ABT_key_set() are maintained per WU and persist for
 * the life of the calling WU.
 *
 * Upon key creation, the value \c NULL shall be associated with the new key in
 * all active WUs.  Upon WU creation, the value \c NULL shall be associated
 * with all defined keys in the new WU.
 *
 * An optional destructor function, \c destructor, may be registered with each
 * key.  When a WU terminates, if a key has a non-NULL destructor pointer, and
 * the WU has a non-NULL value associated with that key, the value of the key
 * is set to \c NULL, and then \c destructor is called with the previously
 * associated value as its sole argument.  The order of destructor calls is
 * unspecified if more than one destructor exists for a WU when it exits.
 *
 * @param[in]  destructor  destructor function called when a WU exits
 * @param[out] newkey      handle to a newly created key
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_key_create(void (*destructor)(void *value), ABT_key *newkey)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_key *p_newkey;

    p_newkey = (ABTI_key *)ABTU_malloc(sizeof(ABTI_key));
    p_newkey->f_destructor = destructor;
    p_newkey->id = ABTD_atomic_fetch_add_uint32(&g_key_id, 1);
    /* Return value */
    *newkey = ABTI_key_get_handle(p_newkey);

    return abt_errno;
}

/**
 * @ingroup KEY
 * @brief   Free an WU-specific data key.
 *
 * \c ABT_key_free() deletes the WU-specific data key specified by \c key and
 * deallocates memory used for the key object.  It is the user's responsibility
 * to free memory for values associated with the deleted key.  This routine
 * does not call the destructor function registered by \c ABT_key_create().
 *
 * @param[in,out] key  handle to the target key
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_key_free(ABT_key *key)
{
    int abt_errno = ABT_SUCCESS;
    ABT_key h_key = *key;
    ABTI_key *p_key = ABTI_key_get_ptr(h_key);
    ABTI_CHECK_NULL_KEY_PTR(p_key);
    ABTU_free(p_key);

    /* Return value */
    *key = ABT_KEY_NULL;

fn_exit:
    return abt_errno;

fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

/**
 * @ingroup KEY
 * @brief   Associate a value with the key.
 *
 * \c ABT_key_set() associates a value, \c value, with the target WU-specific
 * data key, \c key.  Different WUs may bind different values to the same key.
 *
 * @param[in] key    handle to the target key
 * @param[in] value  value for the key
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_key_set(ABT_key key, void *value)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_xstream *p_local_xstream = ABTI_local_get_xstream();

    ABTI_key *p_key = ABTI_key_get_ptr(key);
    ABTI_CHECK_NULL_KEY_PTR(p_key);

    /* We don't allow an external thread to call this routine. */
    ABTI_CHECK_INITIALIZED();
    ABTI_CHECK_TRUE(p_local_xstream != NULL, ABT_ERR_INV_XSTREAM);

    /* Obtain the key-value table pointer. */
    ABTI_unit *p_self = p_local_xstream->p_unit;
    ABTI_ASSERT(p_self);

    if (p_self->p_keytable == NULL) {
        int key_table_size = gp_ABTI_global->key_table_size;
        p_self->p_keytable = ABTI_ktable_alloc(p_local_xstream, key_table_size);
    }

    /* Save the value in the key-value table */
    ABTI_ktable_set(p_local_xstream, p_self->p_keytable, p_key, value);

fn_exit:
    return abt_errno;

fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

/**
 * @ingroup KEY
 * @brief   Get the value associated with the key.
 *
 * \c ABT_key_get() returns the value associated with the target WU-specific
 * data key, \c key, through \c value on behalf of the calling WU.  Different
 * WUs get different values for the target key via this routine if they have
 * set different values with \c ABT_key_set().  If a WU has never set a value
 * for the key, this routine returns \c NULL to \c value.
 *
 * @param[in] key    handle to the target key
 * @param[in] value  value for the key
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_key_get(ABT_key key, void **value)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_xstream *p_local_xstream = ABTI_local_get_xstream();
    void *keyval = NULL;

    ABTI_key *p_key = ABTI_key_get_ptr(key);
    ABTI_CHECK_NULL_KEY_PTR(p_key);

    /* We don't allow an external thread to call this routine. */
    ABTI_CHECK_INITIALIZED();
    ABTI_CHECK_TRUE(p_local_xstream != NULL, ABT_ERR_INV_XSTREAM);

    /* Obtain the key-value table pointer */
    ABTI_unit *p_self = p_local_xstream->p_unit;
    ABTI_ktable *p_ktable = p_self->p_keytable;
    if (p_ktable) {
        /* Retrieve the value from the key-value table */
        keyval = ABTI_ktable_get(p_ktable, p_key);
    }
    *value = keyval;

fn_exit:
    return abt_errno;

fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

ABTI_ktable *ABTI_ktable_alloc(ABTI_xstream *p_local_xstream, int size)
{
    /* size must be a power of 2. */
    ABTI_ASSERT((size & (size - 1)) == 0);
    /* max alignment must be a power of 2. */
    ABTI_STATIC_ASSERT((ABTU_MAX_ALIGNMENT & (ABTU_MAX_ALIGNMENT - 1)) == 0);
    size_t ktable_size =
        (offsetof(ABTI_ktable, p_elems) + sizeof(ABTI_ktelem *) * size +
         ABTU_MAX_ALIGNMENT - 1) &
        (~(ABTU_MAX_ALIGNMENT - 1));
    ABTI_ktable *p_ktable;
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
    memset(p_ktable->p_elems, 0, sizeof(ABTI_ktelem *) * size);
    return p_ktable;
}

void ABTI_ktable_free(ABTI_xstream *p_local_xstream, ABTI_ktable *p_ktable)
{
    ABTI_ktelem *p_elem;
    int i;

    for (i = 0; i < p_ktable->size; i++) {
        p_elem = p_ktable->p_elems[i];
        while (p_elem) {
            /* Call the destructor if it exists and the value is not null. */
            if (p_elem->f_destructor && p_elem->value) {
                p_elem->f_destructor(p_elem->value);
            }
            p_elem = p_elem->p_next;
        }
    }
    ABTI_ktable_mem_header *p_header =
        (ABTI_ktable_mem_header *)p_ktable->p_used_mem;
    while (p_header) {
        ABTI_ktable_mem_header *p_next = p_header->p_next;
        if (ABTU_likely(p_header->is_from_mempool)) {
            ABTI_mem_free_desc(p_local_xstream, (void *)p_header);
        } else {
            ABTU_free(p_header);
        }
        p_header = p_next;
    }
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
