/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#ifndef ABTU_H_INCLUDED
#define ABTU_H_INCLUDED

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "abt_config.h"

/* Utility feature */
#ifdef ABT_CONFIG_HAVE___BUILTIN_EXPECT
#define ABTU_likely(cond) __builtin_expect(!!(cond), 1)
#define ABTU_unlikely(cond) __builtin_expect(!!(cond), 0)
#else
#define ABTU_likely(cond) (cond)
#define ABTU_unlikely(cond) (cond)
#endif

/* Utility Functions */

static inline void *ABTU_memalign(size_t alignment, size_t size)
{
    void *p_ptr;
    int ret = posix_memalign(&p_ptr, alignment, size);
    assert(ret == 0);
    return p_ptr;
}
static inline void ABTU_free(void *ptr)
{
    free(ptr);
}

#ifdef ABT_CONFIG_USE_ALIGNED_ALLOC

static inline void *ABTU_malloc(size_t size)
{
    /* Round up to the smallest multiple of ABT_CONFIG_STATIC_CACHELINE_SIZE
     * which is greater than or equal to size in order to avoid any
     * false-sharing. */
    size = (size + ABT_CONFIG_STATIC_CACHELINE_SIZE - 1) &
           (~(ABT_CONFIG_STATIC_CACHELINE_SIZE - 1));
    return ABTU_memalign(ABT_CONFIG_STATIC_CACHELINE_SIZE, size);
}

static inline void *ABTU_calloc(size_t num, size_t size)
{
    void *ptr = ABTU_malloc(num * size);
    memset(ptr, 0, num * size);
    return ptr;
}

static inline void *ABTU_realloc(void *ptr, size_t old_size, size_t new_size)
{
    void *new_ptr = ABTU_malloc(new_size);
    memcpy(new_ptr, ptr, (old_size < new_size) ? old_size : new_size);
    ABTU_free(ptr);
    return new_ptr;
}

#else /* ABT_CONFIG_USE_ALIGNED_ALLOC */

static inline void *ABTU_malloc(size_t size)
{
    return malloc(size);
}

static inline void *ABTU_calloc(size_t num, size_t size)
{
    return calloc(num, size);
}

static inline void *ABTU_realloc(void *ptr, size_t old_size, size_t new_size)
{
    (void)old_size;
    return realloc(ptr, new_size);
}

#endif /* !ABT_CONFIG_USE_ALIGNED_ALLOC */

typedef enum ABTU_MEM_LARGEPAGE_TYPE {
    ABTU_MEM_LARGEPAGE_MALLOC,   /* ABTU_malloc(). */
    ABTU_MEM_LARGEPAGE_MEMALIGN, /* memalign() */
    ABTU_MEM_LARGEPAGE_MMAP,     /* normal private memory obtained by mmap() */
    ABTU_MEM_LARGEPAGE_MMAP_HUGEPAGE, /* hugepage obtained by mmap() */
} ABTU_MEM_LARGEPAGE_TYPE;

/* Returns 1 if a given large page type is supported. */
int ABTU_is_supported_largepage_type(size_t size, size_t alignment_hint,
                                     ABTU_MEM_LARGEPAGE_TYPE requested);
void *ABTU_alloc_largepage(size_t size, size_t alignment_hint,
                           const ABTU_MEM_LARGEPAGE_TYPE *requested_types,
                           int num_requested_types,
                           ABTU_MEM_LARGEPAGE_TYPE *p_actual);
void ABTU_free_largepage(void *ptr, size_t size, ABTU_MEM_LARGEPAGE_TYPE type);

#define ABTU_strcpy(d, s) strcpy(d, s)
#define ABTU_strncpy(d, s, n) strncpy(d, s, n)

/* The caller should free the memory returned. */
char *ABTU_get_indent_str(int indent);

int ABTU_get_int_len(size_t num);

#endif /* ABTU_H_INCLUDED */
