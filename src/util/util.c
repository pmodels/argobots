/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"

#if defined(__ibmxl__) || defined(__xlc__)

ABTU_ret_err int ABTU_memalign(size_t alignment, size_t size, void **p_ptr)
{
    void *ptr;
    int ret = posix_memalign(&ptr, alignment, size);
    if (ABTI_IS_ERROR_CHECK_ENABLED && ret != 0) {
        return ABT_ERR_MEM;
    }
    *p_ptr = ptr;
    return ABT_SUCCESS;
}

void ABTU_free(void *ptr)
{
    free(ptr);
}

#ifdef ABT_CONFIG_USE_ALIGNED_ALLOC

ABTU_ret_err int ABTU_malloc(size_t size, void **p_ptr)
{
    /* Round up to the smallest multiple of ABT_CONFIG_STATIC_CACHELINE_SIZE
     * which is greater than or equal to size in order to avoid any
     * false-sharing. */
    size = (size + ABT_CONFIG_STATIC_CACHELINE_SIZE - 1) &
           (~(ABT_CONFIG_STATIC_CACHELINE_SIZE - 1));
    return ABTU_memalign(ABT_CONFIG_STATIC_CACHELINE_SIZE, size, p_ptr);
}

ABTU_ret_err int ABTU_calloc(size_t num, size_t size, void **p_ptr)
{
    void *ptr;
    int ret = ABTU_malloc(num * size, &ptr);
    if (ABTI_IS_ERROR_CHECK_ENABLED && ret != ABT_SUCCESS) {
        return ABT_ERR_MEM;
    }
    memset(ptr, 0, num * size);
    *p_ptr = ptr;
    return ABT_SUCCESS;
}

ABTU_ret_err int ABTU_realloc(size_t old_size, size_t new_size, void **p_ptr)
{
    void *new_ptr, *old_ptr = *p_ptr;
    int ret = ABTU_malloc(new_size, &new_ptr);
    if (ABTI_IS_ERROR_CHECK_ENABLED && ret != ABT_SUCCESS) {
        return ABT_ERR_MEM;
    }
    memcpy(new_ptr, old_ptr, (old_size < new_size) ? old_size : new_size);
    ABTU_free(old_ptr);
    *p_ptr = new_ptr;
    return ABT_SUCCESS;
}

#else /* ABT_CONFIG_USE_ALIGNED_ALLOC */

ABTU_ret_err int ABTU_malloc(size_t size, void **p_ptr)
{
    void *ptr = malloc(size);
    if (ABTI_IS_ERROR_CHECK_ENABLED && ptr == NULL) {
        return ABT_ERR_MEM;
    }
    *p_ptr = ptr;
    return ABT_SUCCESS;
}

ABTU_ret_err int ABTU_calloc(size_t num, size_t size, void **p_ptr)
{
    void *ptr = calloc(num, size);
    if (ABTI_IS_ERROR_CHECK_ENABLED && ptr == NULL) {
        return ABT_ERR_MEM;
    }
    *p_ptr = ptr;
    return ABT_SUCCESS;
}

ABTU_ret_err int ABTU_realloc(size_t old_size, size_t new_size, void **p_ptr)
{
    (void)old_size;
    void *ptr = realloc(*p_ptr, new_size);
    if (ABTI_IS_ERROR_CHECK_ENABLED && ptr == NULL) {
        return ABT_ERR_MEM;
    }
    *p_ptr = ptr;
    return ABT_SUCCESS;
}
#endif

#endif /* !(defined(__ibmxl__) || defined(__xlc__)) */
