/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"

/** @defgroup KEY Work-Unit Local Storage (TLS)
 * This group is for work-unit specific data, which can be described as
 * work-unit local storage (TLS).
 */

static inline void ABTI_ktable_set(ABTI_ktable *p_ktable, ABTI_key *p_key,
                                   void *value);
static inline void *ABTI_ktable_get(ABTI_ktable *p_ktable, ABTI_key *p_key);
void ABTI_ktable_delete(ABTI_ktable *p_ktable, ABTI_key *p_key);

static uint32_t g_key_id = 0;

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
    p_newkey->refcount = 1;
    p_newkey->freed = ABT_FALSE;

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

    uint32_t refcount;

    p_key->freed = ABT_TRUE;
    refcount = ABTD_atomic_fetch_sub_uint32(&p_key->refcount, 1);
    if (refcount == 1) {
        ABTU_free(p_key);
    }

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
    ABTI_thread *p_thread;
    ABTI_task *p_task;
    ABTI_ktable *p_ktable;

    ABTI_key *p_key = ABTI_key_get_ptr(key);
    ABTI_CHECK_NULL_KEY_PTR(p_key);

    /* We don't allow an external thread to call this routine. */
    ABTI_CHECK_INITIALIZED();
    ABTI_CHECK_TRUE(lp_ABTI_local != NULL, ABT_ERR_INV_XSTREAM);

    /* Obtain the key-value table pointer. */
    p_thread = ABTI_local_get_thread();
    if (p_thread) {
        if (p_thread->p_keytable == NULL) {
            int key_table_size = gp_ABTI_global->key_table_size;
            p_thread->p_keytable = ABTI_ktable_alloc(key_table_size);
        }
        p_ktable = p_thread->p_keytable;
    } else {
        p_task = ABTI_local_get_task();
        ABTI_CHECK_TRUE(p_task != NULL, ABT_ERR_INV_TASK);
        if (p_task->p_keytable == NULL) {
            int key_table_size = gp_ABTI_global->key_table_size;
            p_task->p_keytable = ABTI_ktable_alloc(key_table_size);
        }
        p_ktable = p_task->p_keytable;
    }

    /* Save the value in the key-value table */
    ABTI_ktable_set(p_ktable, p_key, value);

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
    ABTI_thread *p_thread;
    ABTI_task *p_task;
    ABTI_ktable *p_ktable = NULL;
    void *keyval = NULL;

    ABTI_key *p_key = ABTI_key_get_ptr(key);
    ABTI_CHECK_NULL_KEY_PTR(p_key);

    /* We don't allow an external thread to call this routine. */
    ABTI_CHECK_INITIALIZED();
    ABTI_CHECK_TRUE(lp_ABTI_local != NULL, ABT_ERR_INV_XSTREAM);

    /* Obtain the key-value table pointer */
    p_thread = ABTI_local_get_thread();
    if (p_thread) {
        p_ktable = p_thread->p_keytable;
        if (p_ktable) {
            /* Retrieve the value from the key-value table */
            keyval = ABTI_ktable_get(p_ktable, p_key);
        }
    } else {
        p_task = ABTI_local_get_task();
        ABTI_CHECK_TRUE(p_task != NULL, ABT_ERR_INV_TASK);
        p_ktable = p_task->p_keytable;
        if (p_ktable) {
            /* Retrieve the value from the key-value table */
            keyval = ABTI_ktable_get(p_ktable, p_key);
        }
    }

    *value = keyval;

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

ABTI_ktable *ABTI_ktable_alloc(int size)
{
    ABTI_ktable *p_ktable;

    p_ktable = (ABTI_ktable *)ABTU_malloc(sizeof(ABTI_ktable));
    p_ktable->size = size;
    p_ktable->num = 0;
    p_ktable->p_elems = (ABTI_ktelem **)ABTU_calloc(size, sizeof(ABTI_ktelem *));

    return p_ktable;
}

void ABTI_ktable_free(ABTI_ktable *p_ktable)
{
    ABTI_ktelem *p_elem, *p_next;
    ABTI_key *p_key;
    int i;
    uint32_t refcount;

    for (i = 0; i < p_ktable->size; i++) {
        p_elem = p_ktable->p_elems[i];
        while (p_elem) {
            /* Call the destructor if it exists and the value is not null. */
            p_key = p_elem->p_key;
            if (p_key->f_destructor && p_elem->value) {
                p_key->f_destructor(p_elem->value);
            }
            refcount = ABTD_atomic_fetch_sub_uint32(&p_key->refcount, 1);
            if (refcount == 1 && p_key->freed == ABT_TRUE) {
                ABTU_free(p_key);
            }

            p_next = p_elem->p_next;
            ABTU_free(p_elem);
            p_elem = p_next;
        }
    }

    ABTU_free(p_ktable->p_elems);
    ABTU_free(p_ktable);
}

static inline uint32_t ABTI_ktable_get_idx(ABTI_key *p_key, int size)
{
    return p_key->id % size;
}

static inline void ABTI_ktable_set(ABTI_ktable *p_ktable, ABTI_key *p_key,
                                   void *value)
{
    uint32_t idx;
    ABTI_ktelem *p_elem;

    /* Look for the same key */
    idx = ABTI_ktable_get_idx(p_key, p_ktable->size);
    p_elem = p_ktable->p_elems[idx];
    while (p_elem) {
        if (p_elem->p_key == p_key) {
            p_elem->value = value;
            return;
        }
        p_elem = p_elem->p_next;
    }

    /* The table does not have the same key */
    p_elem = (ABTI_ktelem *)ABTU_malloc(sizeof(ABTI_ktelem));
    p_elem->p_key = p_key;
    p_elem->value = value;
    p_elem->p_next = p_ktable->p_elems[idx];
    ABTD_atomic_fetch_add_uint32(&p_key->refcount, 1);
    p_ktable->p_elems[idx] = p_elem;

    p_ktable->num++;
}

static inline void *ABTI_ktable_get(ABTI_ktable *p_ktable, ABTI_key *p_key)
{
    uint32_t idx;
    ABTI_ktelem *p_elem;

    idx = ABTI_ktable_get_idx(p_key, p_ktable->size);
    p_elem = p_ktable->p_elems[idx];
    while (p_elem) {
        if (p_elem->p_key == p_key) {
            return p_elem->value;
        }
        p_elem = p_elem->p_next;
    }

    return NULL;
}

void ABTI_ktable_delete(ABTI_ktable *p_ktable, ABTI_key *p_key)
{
    uint32_t idx;
    ABTI_ktelem *p_prev = NULL;
    ABTI_ktelem *p_elem;

    idx = ABTI_ktable_get_idx(p_key, p_ktable->size);
    p_elem = p_ktable->p_elems[idx];
    while (p_elem) {
        if (p_elem->p_key == p_key) {
            if (p_prev) {
                p_prev->p_next = p_elem->p_next;
            } else {
                p_ktable->p_elems[idx] = p_elem->p_next;
            }
            p_ktable->num--;

            ABTU_free(p_elem);
            return;
        }

        p_prev = p_elem;
        p_elem = p_elem->p_next;
    }
}

