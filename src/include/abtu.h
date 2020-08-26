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

#ifdef HAVE___BUILTIN_EXPECT
#define ABTU_likely(cond) __builtin_expect(!!(cond), 1)
#define ABTU_unlikely(cond) __builtin_expect(!!(cond), 0)
#else
#define ABTU_likely(cond) (cond)
#define ABTU_unlikely(cond) (cond)
#endif

#ifdef HAVE___BUILTIN_UNREACHABLE
#define ABTU_unreachable() __builtin_unreachable()
#else
#define ABTU_unreachable()
#endif

#ifdef HAVE_FUNC_ATTRIBUTE_NORETURN
#define ABTU_noreturn __attribute__((noreturn))
#else
#define ABTU_noreturn
#endif

#ifdef ABT_CONFIG_HAVE_ALIGNOF_GCC
#define ABTU_alignof(type) (__alignof__(type))
#elif defined(ABT_CONFIG_HAVE_ALIGNOF_C11)
#define ABTU_alignof(type) (alignof(type))
#else
#define ABTU_alignof(type) 16 /* 16 bytes would be a good guess. */
#endif
#define ABTU_MAX_ALIGNMENT                                                     \
    (ABTU_alignof(long double) > ABTU_alignof(long long)                       \
         ? ABTU_alignof(long double)                                           \
         : ABTU_alignof(long long))

/*
 * An attribute to hint an alignment of a member variable.
 * Usage:
 * struct X {
 *   void *obj_1;
 *   ABTU_align_member_var(64)
 *   void *obj_2;
 * };
 */
#ifndef __SUNPRO_C
#define ABTU_align_member_var(size) __attribute__((aligned(size)))
#else
/* Sun Studio does not support it. */
#define ABTU_align_member_var(size)
#endif

/*
 * An attribute to suppress address sanitizer warning.
 */
#if defined(__GNUC__) && defined(__SANITIZE_ADDRESS__)
/*
 * Older GCC cannot combine no_sanitize_address + always_inline (e.g., builtin
 * memcpy on some platforms), which causes *a compilation error*.  Let's accept
 * false-positive warning if used GCC is old.  This issue seems fixed between
 * GCC 7.4.0 and GCC 8.3.0 as far as I checked.
 * https://gcc.gnu.org/bugzilla/show_bug.cgi?id=59600
 */
#if __GNUC__ >= 8
#define ABTU_no_sanitize_address __attribute__((no_sanitize_address))
#endif
#elif __clang__
#if defined(__has_feature)
#if __has_feature(address_sanitizer)
#if __clang_major__ >= 4 || (__clang_major__ >= 3 && __clang_minor__ >= 7)
/* >= Clang 3.7.0 */
#define ABTU_no_sanitize_address __attribute__((no_sanitize("address")))
#elif (__clang_major__ >= 3 && __clang_minor__ >= 3)
/* >= Clang 3.3.0 */
#define ABTU_no_sanitize_address __attribute__((no_sanitize_address))
#elif (__clang_major__ >= 3 && __clang_minor__ >= 1)
/* >= Clang 3.1.0 */
#define ABTU_no_sanitize_address __attribute__((no_address_safety_analysis))
#else /* Too old clang. */
#define ABTU_no_sanitize_address
#endif
#endif /* __has_feature(address_sanitizer) */
#endif /* defined(__has_feature) */
#endif

#ifndef ABTU_no_sanitize_address
/* We do not support other address sanitizers. */
#define ABTU_no_sanitize_address
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

/* The caller should free the memory returned. */
char *ABTU_get_indent_str(int indent);

#endif /* ABTU_H_INCLUDED */
