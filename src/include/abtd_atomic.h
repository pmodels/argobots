/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#ifndef ABTD_ATOMIC_H_INCLUDED
#define ABTD_ATOMIC_H_INCLUDED

#include <stdint.h>

typedef struct ABTD_atomic_uint8 {
    uint8_t val;
} ABTD_atomic_uint8;

typedef struct ABTD_atomic_int {
    int val;
} ABTD_atomic_int;

typedef struct ABTD_atomic_int32 {
    int32_t val;
} ABTD_atomic_int32;

typedef struct ABTD_atomic_uint32 {
    uint32_t val;
} ABTD_atomic_uint32;

typedef struct ABTD_atomic_int64 {
    int64_t val;
} ABTD_atomic_int64;

typedef struct ABTD_atomic_uint64 {
    uint64_t val;
} ABTD_atomic_uint64;

typedef struct ABTD_atomic_ptr {
    void *val;
} ABTD_atomic_ptr;

#define ABTD_ATOMIC_UINT8_STATIC_INITIALIZER(val)                              \
    {                                                                          \
        (val)                                                                  \
    }
#define ABTD_ATOMIC_INT_STATIC_INITIALIZER(val)                                \
    {                                                                          \
        (val)                                                                  \
    }
#define ABTD_ATOMIC_INT32_STATIC_INITIALIZER(val)                              \
    {                                                                          \
        (val)                                                                  \
    }
#define ABTD_ATOMIC_UINT32_STATIC_INITIALIZER(val)                             \
    {                                                                          \
        (val)                                                                  \
    }
#define ABTD_ATOMIC_INT64_STATIC_INITIALIZER(val)                              \
    {                                                                          \
        (val)                                                                  \
    }
#define ABTD_ATOMIC_UINT64_STATIC_INITIALIZER(val)                             \
    {                                                                          \
        (val)                                                                  \
    }
#define ABTD_ATOMIC_PTR_STATIC_INITIALIZER(val)                                \
    {                                                                          \
        (val)                                                                  \
    }

static inline int ABTDI_atomic_val_cas_int(ABTD_atomic_int *ptr, int oldv, int newv, int weak)
{
#ifdef ABT_CONFIG_HAVE_ATOMIC_BUILTIN
    int tmp_oldv = oldv;
    int ret = __atomic_compare_exchange_n(&ptr->val, &oldv, newv, weak,
                                          __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);
    return ret ? tmp_oldv : oldv;
#else
    return __sync_val_compare_and_swap(&ptr->val, oldv, newv);
#endif
}

static inline int32_t ABTDI_atomic_val_cas_int32(ABTD_atomic_int32 *ptr, int32_t oldv,
                                                 int32_t newv, int weak)
{
#ifdef ABT_CONFIG_HAVE_ATOMIC_BUILTIN
    int32_t tmp_oldv = oldv;
    int ret = __atomic_compare_exchange_n(&ptr->val, &oldv, newv, weak,
                                          __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);
    return ret ? tmp_oldv : oldv;
#else
    return __sync_val_compare_and_swap(&ptr->val, oldv, newv);
#endif
}

static inline uint32_t ABTDI_atomic_val_cas_uint32(ABTD_atomic_uint32 *ptr, uint32_t oldv,
                                                   uint32_t newv, int weak)
{
#ifdef ABT_CONFIG_HAVE_ATOMIC_BUILTIN
    uint32_t tmp_oldv = oldv;
    int ret = __atomic_compare_exchange_n(&ptr->val, &oldv, newv, weak,
                                          __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);
    return ret ? tmp_oldv : oldv;
#else
    return __sync_val_compare_and_swap(&ptr->val, oldv, newv);
#endif
}

static inline int64_t ABTDI_atomic_val_cas_int64(ABTD_atomic_int64 *ptr, int64_t oldv,
                                                 int64_t newv, int weak)
{
#ifdef ABT_CONFIG_HAVE_ATOMIC_BUILTIN
    int64_t tmp_oldv = oldv;
    int ret = __atomic_compare_exchange_n(&ptr->val, &oldv, newv, weak,
                                          __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);
    return ret ? tmp_oldv : oldv;
#else
    return __sync_val_compare_and_swap(&ptr->val, oldv, newv);
#endif
}

static inline uint64_t ABTDI_atomic_val_cas_uint64(ABTD_atomic_uint64 *ptr, uint64_t oldv,
                                                   uint64_t newv, int weak)
{
#ifdef ABT_CONFIG_HAVE_ATOMIC_BUILTIN
    uint64_t tmp_oldv = oldv;
    int ret = __atomic_compare_exchange_n(&ptr->val, &oldv, newv, weak,
                                          __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);
    return ret ? tmp_oldv : oldv;
#else
    return __sync_val_compare_and_swap(&ptr->val, oldv, newv);
#endif
}

static inline void *ABTDI_atomic_val_cas_ptr(ABTD_atomic_ptr *ptr, void *oldv, void *newv,
                                             int weak)
{
#ifdef ABT_CONFIG_HAVE_ATOMIC_BUILTIN
    void *tmp_oldv = oldv;
    int ret = __atomic_compare_exchange_n(&ptr->val, &oldv, newv, weak,
                                          __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);
    return ret ? tmp_oldv : oldv;
#else
    return __sync_val_compare_and_swap(&ptr->val, oldv, newv);
#endif
}

static inline int ABTDI_atomic_bool_cas_int(ABTD_atomic_int *ptr, int oldv, int newv, int weak)
{
#ifdef ABT_CONFIG_HAVE_ATOMIC_BUILTIN
    return __atomic_compare_exchange_n(&ptr->val, &oldv, newv, weak, __ATOMIC_ACQ_REL,
                                       __ATOMIC_ACQUIRE);
#else
    return __sync_bool_compare_and_swap(&ptr->val, oldv, newv);
#endif
}

static inline int ABTDI_atomic_bool_cas_int32(ABTD_atomic_int32 *ptr, int32_t oldv,
                                              int32_t newv, int weak)
{
#ifdef ABT_CONFIG_HAVE_ATOMIC_BUILTIN
    return __atomic_compare_exchange_n(&ptr->val, &oldv, newv, weak, __ATOMIC_ACQ_REL,
                                       __ATOMIC_ACQUIRE);
#else
    return __sync_bool_compare_and_swap(&ptr->val, oldv, newv);
#endif
}

static inline int ABTDI_atomic_bool_cas_uint32(ABTD_atomic_uint32 *ptr, uint32_t oldv,
                                               uint32_t newv, int weak)
{
#ifdef ABT_CONFIG_HAVE_ATOMIC_BUILTIN
    return __atomic_compare_exchange_n(&ptr->val, &oldv, newv, weak, __ATOMIC_ACQ_REL,
                                       __ATOMIC_ACQUIRE);
#else
    return __sync_bool_compare_and_swap(&ptr->val, oldv, newv);
#endif
}

static inline int ABTDI_atomic_bool_cas_int64(ABTD_atomic_int64 *ptr, int64_t oldv,
                                              int64_t newv, int weak)
{
#ifdef ABT_CONFIG_HAVE_ATOMIC_BUILTIN
    return __atomic_compare_exchange_n(&ptr->val, &oldv, newv, weak, __ATOMIC_ACQ_REL,
                                       __ATOMIC_ACQUIRE);
#else
    return __sync_bool_compare_and_swap(&ptr->val, oldv, newv);
#endif
}

static inline int ABTDI_atomic_bool_cas_uint64(ABTD_atomic_uint64 *ptr, uint64_t oldv,
                                               uint64_t newv, int weak)
{
#ifdef ABT_CONFIG_HAVE_ATOMIC_BUILTIN
    return __atomic_compare_exchange_n(&ptr->val, &oldv, newv, weak, __ATOMIC_ACQ_REL,
                                       __ATOMIC_ACQUIRE);
#else
    return __sync_bool_compare_and_swap(&ptr->val, oldv, newv);
#endif
}

static inline int ABTDI_atomic_bool_cas_ptr(ABTD_atomic_ptr *ptr, void *oldv, void *newv,
                                            int weak)
{
#ifdef ABT_CONFIG_HAVE_ATOMIC_BUILTIN
    return __atomic_compare_exchange_n(&ptr->val, &oldv, newv, weak, __ATOMIC_ACQ_REL,
                                       __ATOMIC_ACQUIRE);
#else
    return __sync_bool_compare_and_swap(&ptr->val, oldv, newv);
#endif
}

static inline int ABTD_atomic_val_cas_weak_int(ABTD_atomic_int *ptr, int oldv, int newv)
{
    return ABTDI_atomic_val_cas_int(ptr, oldv, newv, 1);
}

static inline int32_t ABTD_atomic_val_cas_weak_int32(ABTD_atomic_int32 *ptr, int32_t oldv,
                                                     int32_t newv)
{
    return ABTDI_atomic_val_cas_int32(ptr, oldv, newv, 1);
}

static inline uint32_t
ABTD_atomic_val_cas_weak_uint32(ABTD_atomic_uint32 *ptr, uint32_t oldv, uint32_t newv)
{
    return ABTDI_atomic_val_cas_uint32(ptr, oldv, newv, 1);
}

static inline int64_t ABTD_atomic_val_cas_weak_int64(ABTD_atomic_int64 *ptr, int64_t oldv,
                                                     int64_t newv)
{
    return ABTDI_atomic_val_cas_int64(ptr, oldv, newv, 1);
}

static inline uint64_t
ABTD_atomic_val_cas_weak_uint64(ABTD_atomic_uint64 *ptr, uint64_t oldv, uint64_t newv)
{
    return ABTDI_atomic_val_cas_uint64(ptr, oldv, newv, 1);
}

static inline void *ABTD_atomic_val_cas_weak_ptr(ABTD_atomic_ptr *ptr, void *oldv,
                                                 void *newv)
{
    return ABTDI_atomic_val_cas_ptr(ptr, oldv, newv, 1);
}

static inline int ABTD_atomic_val_cas_strong_int(ABTD_atomic_int *ptr, int oldv, int newv)
{
    return ABTDI_atomic_val_cas_int(ptr, oldv, newv, 0);
}

static inline int32_t
ABTD_atomic_val_cas_strong_int32(ABTD_atomic_int32 *ptr, int32_t oldv, int32_t newv)
{
    return ABTDI_atomic_val_cas_int32(ptr, oldv, newv, 0);
}

static inline uint32_t
ABTD_atomic_val_cas_strong_uint32(ABTD_atomic_uint32 *ptr, uint32_t oldv, uint32_t newv)
{
    return ABTDI_atomic_val_cas_uint32(ptr, oldv, newv, 0);
}

static inline int64_t
ABTD_atomic_val_cas_strong_int64(ABTD_atomic_int64 *ptr, int64_t oldv, int64_t newv)
{
    return ABTDI_atomic_val_cas_int64(ptr, oldv, newv, 0);
}

static inline uint64_t
ABTD_atomic_val_cas_strong_uint64(ABTD_atomic_uint64 *ptr, uint64_t oldv, uint64_t newv)
{
    return ABTDI_atomic_val_cas_uint64(ptr, oldv, newv, 0);
}

static inline void *ABTD_atomic_val_cas_strong_ptr(ABTD_atomic_ptr *ptr, void *oldv,
                                                   void *newv)
{
    return ABTDI_atomic_val_cas_ptr(ptr, oldv, newv, 0);
}

static inline int ABTD_atomic_bool_cas_weak_int(ABTD_atomic_int *ptr, int oldv, int newv)
{
    return ABTDI_atomic_bool_cas_int(ptr, oldv, newv, 1);
}

static inline int ABTD_atomic_bool_cas_weak_int32(ABTD_atomic_int32 *ptr, int32_t oldv,
                                                  int32_t newv)
{
    return ABTDI_atomic_bool_cas_int32(ptr, oldv, newv, 1);
}

static inline int ABTD_atomic_bool_cas_weak_uint32(ABTD_atomic_uint32 *ptr, uint32_t oldv,
                                                   uint32_t newv)
{
    return ABTDI_atomic_bool_cas_uint32(ptr, oldv, newv, 1);
}

static inline int ABTD_atomic_bool_cas_weak_int64(ABTD_atomic_int64 *ptr, int64_t oldv,
                                                  int64_t newv)
{
    return ABTDI_atomic_bool_cas_int64(ptr, oldv, newv, 1);
}

static inline int ABTD_atomic_bool_cas_weak_uint64(ABTD_atomic_uint64 *ptr, uint64_t oldv,
                                                   uint64_t newv)
{
    return ABTDI_atomic_bool_cas_uint64(ptr, oldv, newv, 1);
}

static inline int ABTD_atomic_bool_cas_weak_ptr(ABTD_atomic_ptr *ptr, void *oldv,
                                                void *newv)
{
    return ABTDI_atomic_bool_cas_ptr(ptr, oldv, newv, 1);
}

static inline int ABTD_atomic_bool_cas_strong_int(ABTD_atomic_int *ptr, int oldv, int newv)
{
    return ABTDI_atomic_bool_cas_int(ptr, oldv, newv, 0);
}

static inline int ABTD_atomic_bool_cas_strong_int32(ABTD_atomic_int32 *ptr, int32_t oldv,
                                                    int32_t newv)
{
    return ABTDI_atomic_bool_cas_int32(ptr, oldv, newv, 0);
}

static inline int
ABTD_atomic_bool_cas_strong_uint32(ABTD_atomic_uint32 *ptr, uint32_t oldv, uint32_t newv)
{
    return ABTDI_atomic_bool_cas_uint32(ptr, oldv, newv, 0);
}

static inline int ABTD_atomic_bool_cas_strong_int64(ABTD_atomic_int64 *ptr, int64_t oldv,
                                                    int64_t newv)
{
    return ABTDI_atomic_bool_cas_int64(ptr, oldv, newv, 0);
}

static inline int
ABTD_atomic_bool_cas_strong_uint64(ABTD_atomic_uint64 *ptr, uint64_t oldv, uint64_t newv)
{
    return ABTDI_atomic_bool_cas_uint64(ptr, oldv, newv, 0);
}

static inline int ABTD_atomic_bool_cas_strong_ptr(ABTD_atomic_ptr *ptr, void *oldv,
                                                  void *newv)
{
    return ABTDI_atomic_bool_cas_ptr(ptr, oldv, newv, 0);
}

static inline int ABTD_atomic_fetch_add_int(ABTD_atomic_int *ptr, int v)
{
#ifdef ABT_CONFIG_HAVE_ATOMIC_BUILTIN
    return __atomic_fetch_add(&ptr->val, v, __ATOMIC_ACQ_REL);
#else
    return __sync_fetch_and_add(&ptr->val, v);
#endif
}

static inline int32_t ABTD_atomic_fetch_add_int32(ABTD_atomic_int32 *ptr, int32_t v)
{
#ifdef ABT_CONFIG_HAVE_ATOMIC_BUILTIN
    return __atomic_fetch_add(&ptr->val, v, __ATOMIC_ACQ_REL);
#else
    return __sync_fetch_and_add(&ptr->val, v);
#endif
}

static inline uint32_t ABTD_atomic_fetch_add_uint32(ABTD_atomic_uint32 *ptr, uint32_t v)
{
#ifdef ABT_CONFIG_HAVE_ATOMIC_BUILTIN
    return __atomic_fetch_add(&ptr->val, v, __ATOMIC_ACQ_REL);
#else
    return __sync_fetch_and_add(&ptr->val, v);
#endif
}

static inline int64_t ABTD_atomic_fetch_add_int64(ABTD_atomic_int64 *ptr, int64_t v)
{
#ifdef ABT_CONFIG_HAVE_ATOMIC_BUILTIN
    return __atomic_fetch_add(&ptr->val, v, __ATOMIC_ACQ_REL);
#else
    return __sync_fetch_and_add(&ptr->val, v);
#endif
}

static inline uint64_t ABTD_atomic_fetch_add_uint64(ABTD_atomic_uint64 *ptr, uint64_t v)
{
#ifdef ABT_CONFIG_HAVE_ATOMIC_BUILTIN
    return __atomic_fetch_add(&ptr->val, v, __ATOMIC_ACQ_REL);
#else
    return __sync_fetch_and_add(&ptr->val, v);
#endif
}

static inline int ABTD_atomic_fetch_sub_int(ABTD_atomic_int *ptr, int v)
{
#ifdef ABT_CONFIG_HAVE_ATOMIC_BUILTIN
    return __atomic_fetch_sub(&ptr->val, v, __ATOMIC_ACQ_REL);
#else
    return __sync_fetch_and_sub(&ptr->val, v);
#endif
}

static inline int32_t ABTD_atomic_fetch_sub_int32(ABTD_atomic_int32 *ptr, int32_t v)
{
#ifdef ABT_CONFIG_HAVE_ATOMIC_BUILTIN
    return __atomic_fetch_sub(&ptr->val, v, __ATOMIC_ACQ_REL);
#else
    return __sync_fetch_and_sub(&ptr->val, v);
#endif
}

static inline uint32_t ABTD_atomic_fetch_sub_uint32(ABTD_atomic_uint32 *ptr, uint32_t v)
{
#ifdef ABT_CONFIG_HAVE_ATOMIC_BUILTIN
    return __atomic_fetch_sub(&ptr->val, v, __ATOMIC_ACQ_REL);
#else
    return __sync_fetch_and_sub(&ptr->val, v);
#endif
}

static inline int64_t ABTD_atomic_fetch_sub_int64(ABTD_atomic_int64 *ptr, int64_t v)
{
#ifdef ABT_CONFIG_HAVE_ATOMIC_BUILTIN
    return __atomic_fetch_sub(&ptr->val, v, __ATOMIC_ACQ_REL);
#else
    return __sync_fetch_and_sub(&ptr->val, v);
#endif
}

static inline uint64_t ABTD_atomic_fetch_sub_uint64(ABTD_atomic_uint64 *ptr, uint64_t v)
{
#ifdef ABT_CONFIG_HAVE_ATOMIC_BUILTIN
    return __atomic_fetch_sub(&ptr->val, v, __ATOMIC_ACQ_REL);
#else
    return __sync_fetch_and_sub(&ptr->val, v);
#endif
}

static inline int ABTD_atomic_fetch_and_int(ABTD_atomic_int *ptr, int v)
{
#ifdef ABT_CONFIG_HAVE_ATOMIC_BUILTIN
    return __atomic_fetch_and(&ptr->val, v, __ATOMIC_ACQ_REL);
#else
    return __sync_fetch_and_and(&ptr->val, v);
#endif
}

static inline int32_t ABTD_atomic_fetch_and_int32(ABTD_atomic_int32 *ptr, int32_t v)
{
#ifdef ABT_CONFIG_HAVE_ATOMIC_BUILTIN
    return __atomic_fetch_and(&ptr->val, v, __ATOMIC_ACQ_REL);
#else
    return __sync_fetch_and_and(&ptr->val, v);
#endif
}

static inline uint32_t ABTD_atomic_fetch_and_uint32(ABTD_atomic_uint32 *ptr, uint32_t v)
{
#ifdef ABT_CONFIG_HAVE_ATOMIC_BUILTIN
    return __atomic_fetch_and(&ptr->val, v, __ATOMIC_ACQ_REL);
#else
    return __sync_fetch_and_and(&ptr->val, v);
#endif
}

static inline int64_t ABTD_atomic_fetch_and_int64(ABTD_atomic_int64 *ptr, int64_t v)
{
#ifdef ABT_CONFIG_HAVE_ATOMIC_BUILTIN
    return __atomic_fetch_and(&ptr->val, v, __ATOMIC_ACQ_REL);
#else
    return __sync_fetch_and_and(&ptr->val, v);
#endif
}

static inline uint64_t ABTD_atomic_fetch_and_uint64(ABTD_atomic_uint64 *ptr, uint64_t v)
{
#ifdef ABT_CONFIG_HAVE_ATOMIC_BUILTIN
    return __atomic_fetch_and(&ptr->val, v, __ATOMIC_ACQ_REL);
#else
    return __sync_fetch_and_and(&ptr->val, v);
#endif
}

static inline int ABTD_atomic_fetch_or_int(ABTD_atomic_int *ptr, int v)
{
#ifdef ABT_CONFIG_HAVE_ATOMIC_BUILTIN
    return __atomic_fetch_or(&ptr->val, v, __ATOMIC_ACQ_REL);
#else
    return __sync_fetch_and_or(&ptr->val, v);
#endif
}

static inline int32_t ABTD_atomic_fetch_or_int32(ABTD_atomic_int32 *ptr, int32_t v)
{
#ifdef ABT_CONFIG_HAVE_ATOMIC_BUILTIN
    return __atomic_fetch_or(&ptr->val, v, __ATOMIC_ACQ_REL);
#else
    return __sync_fetch_and_or(&ptr->val, v);
#endif
}

static inline uint32_t ABTD_atomic_fetch_or_uint32(ABTD_atomic_uint32 *ptr, uint32_t v)
{
#ifdef ABT_CONFIG_HAVE_ATOMIC_BUILTIN
    return __atomic_fetch_or(&ptr->val, v, __ATOMIC_ACQ_REL);
#else
    return __sync_fetch_and_or(&ptr->val, v);
#endif
}

static inline int64_t ABTD_atomic_fetch_or_int64(ABTD_atomic_int64 *ptr, int64_t v)
{
#ifdef ABT_CONFIG_HAVE_ATOMIC_BUILTIN
    return __atomic_fetch_or(&ptr->val, v, __ATOMIC_ACQ_REL);
#else
    return __sync_fetch_and_or(&ptr->val, v);
#endif
}

static inline uint64_t ABTD_atomic_fetch_or_uint64(ABTD_atomic_uint64 *ptr, uint64_t v)
{
#ifdef ABT_CONFIG_HAVE_ATOMIC_BUILTIN
    return __atomic_fetch_or(&ptr->val, v, __ATOMIC_ACQ_REL);
#else
    return __sync_fetch_and_or(&ptr->val, v);
#endif
}

static inline int ABTD_atomic_fetch_xor_int(ABTD_atomic_int *ptr, int v)
{
#ifdef ABT_CONFIG_HAVE_ATOMIC_BUILTIN
    return __atomic_fetch_xor(&ptr->val, v, __ATOMIC_ACQ_REL);
#else
    return __sync_fetch_and_xor(&ptr->val, v);
#endif
}

static inline int32_t ABTD_atomic_fetch_xor_int32(ABTD_atomic_int32 *ptr, int32_t v)
{
#ifdef ABT_CONFIG_HAVE_ATOMIC_BUILTIN
    return __atomic_fetch_xor(&ptr->val, v, __ATOMIC_ACQ_REL);
#else
    return __sync_fetch_and_xor(&ptr->val, v);
#endif
}

static inline uint32_t ABTD_atomic_fetch_xor_uint32(ABTD_atomic_uint32 *ptr, uint32_t v)
{
#ifdef ABT_CONFIG_HAVE_ATOMIC_BUILTIN
    return __atomic_fetch_xor(&ptr->val, v, __ATOMIC_ACQ_REL);
#else
    return __sync_fetch_and_xor(&ptr->val, v);
#endif
}

static inline int64_t ABTD_atomic_fetch_xor_int64(ABTD_atomic_int64 *ptr, int64_t v)
{
#ifdef ABT_CONFIG_HAVE_ATOMIC_BUILTIN
    return __atomic_fetch_xor(&ptr->val, v, __ATOMIC_ACQ_REL);
#else
    return __sync_fetch_and_xor(&ptr->val, v);
#endif
}

static inline uint64_t ABTD_atomic_fetch_xor_uint64(ABTD_atomic_uint64 *ptr, uint64_t v)
{
#ifdef ABT_CONFIG_HAVE_ATOMIC_BUILTIN
    return __atomic_fetch_xor(&ptr->val, v, __ATOMIC_ACQ_REL);
#else
    return __sync_fetch_and_xor(&ptr->val, v);
#endif
}

static inline uint16_t ABTD_atomic_test_and_set_uint8(ABTD_atomic_uint8 *ptr)
{
    /* return 0 if this test_and_set succeeds to set a value. */
#ifdef ABT_CONFIG_HAVE_ATOMIC_BUILTIN
    return __atomic_test_and_set(&ptr->val, __ATOMIC_ACQUIRE);
#else
    return __sync_lock_test_and_set(&ptr->val, 1);
#endif
}

static inline void ABTD_atomic_relaxed_clear_uint8(ABTD_atomic_uint8 *ptr)
{
#ifdef ABT_CONFIG_HAVE_ATOMIC_BUILTIN
    __atomic_clear(&ptr->val, __ATOMIC_RELAXED);
#else
    *(volatile uint8_t *)&ptr->val = 0;
#endif
}

static inline void ABTD_atomic_release_clear_uint8(ABTD_atomic_uint8 *ptr)
{
#ifdef ABT_CONFIG_HAVE_ATOMIC_BUILTIN
    __atomic_clear(&ptr->val, __ATOMIC_RELEASE);
#else
    __sync_lock_release(&ptr->val);
#endif
}

static inline uint16_t ABTD_atomic_relaxed_load_uint8(const ABTD_atomic_uint8 *ptr)
{
#ifdef ABT_CONFIG_HAVE_ATOMIC_BUILTIN
    return __atomic_load_n(&ptr->val, __ATOMIC_RELAXED);
#else
    return *(volatile uint8_t *)&ptr->val;
#endif
}

static inline int ABTD_atomic_relaxed_load_int(const ABTD_atomic_int *ptr)
{
#ifdef ABT_CONFIG_HAVE_ATOMIC_BUILTIN
    return __atomic_load_n(&ptr->val, __ATOMIC_RELAXED);
#else
    return *(volatile int *)&ptr->val;
#endif
}

static inline int32_t ABTD_atomic_relaxed_load_int32(const ABTD_atomic_int32 *ptr)
{
#ifdef ABT_CONFIG_HAVE_ATOMIC_BUILTIN
    return __atomic_load_n(&ptr->val, __ATOMIC_RELAXED);
#else
    return *(volatile int32_t *)&ptr->val;
#endif
}

static inline uint32_t ABTD_atomic_relaxed_load_uint32(const ABTD_atomic_uint32 *ptr)
{
#ifdef ABT_CONFIG_HAVE_ATOMIC_BUILTIN
    return __atomic_load_n(&ptr->val, __ATOMIC_RELAXED);
#else
    return *(volatile uint32_t *)&ptr->val;
#endif
}

static inline int64_t ABTD_atomic_relaxed_load_int64(const ABTD_atomic_int64 *ptr)
{
#ifdef ABT_CONFIG_HAVE_ATOMIC_BUILTIN
    return __atomic_load_n(&ptr->val, __ATOMIC_RELAXED);
#else
    return *(volatile int64_t *)&ptr->val;
#endif
}

static inline uint64_t ABTD_atomic_relaxed_load_uint64(const ABTD_atomic_uint64 *ptr)
{
    /* return 0 if this test_and_set succeeds to set a value. */
#ifdef ABT_CONFIG_HAVE_ATOMIC_BUILTIN
    return __atomic_load_n(&ptr->val, __ATOMIC_RELAXED);
#else
    return *(volatile uint64_t *)&ptr->val;
#endif
}

static inline void *ABTD_atomic_relaxed_load_ptr(const ABTD_atomic_ptr *ptr)
{
    /* return 0 if this test_and_set succeeds to set a value. */
#ifdef ABT_CONFIG_HAVE_ATOMIC_BUILTIN
    return __atomic_load_n(&ptr->val, __ATOMIC_RELAXED);
#else
    return *(void *volatile *)&ptr->val;
#endif
}

static inline uint16_t ABTD_atomic_acquire_load_uint8(const ABTD_atomic_uint8 *ptr)
{
#ifdef ABT_CONFIG_HAVE_ATOMIC_BUILTIN
    return __atomic_load_n(&ptr->val, __ATOMIC_ACQUIRE);
#else
    __sync_synchronize();
    uint8_t val = *(volatile uint8_t *)&ptr->val;
    __sync_synchronize();
    return val;
#endif
}

static inline int ABTD_atomic_acquire_load_int(const ABTD_atomic_int *ptr)
{
#ifdef ABT_CONFIG_HAVE_ATOMIC_BUILTIN
    return __atomic_load_n(&ptr->val, __ATOMIC_ACQUIRE);
#else
    __sync_synchronize();
    int val = *(volatile int *)&ptr->val;
    __sync_synchronize();
    return val;
#endif
}

static inline int32_t ABTD_atomic_acquire_load_int32(const ABTD_atomic_int32 *ptr)
{
#ifdef ABT_CONFIG_HAVE_ATOMIC_BUILTIN
    return __atomic_load_n(&ptr->val, __ATOMIC_ACQUIRE);
#else
    __sync_synchronize();
    int32_t val = *(volatile int32_t *)&ptr->val;
    __sync_synchronize();
    return val;
#endif
}

static inline uint32_t ABTD_atomic_acquire_load_uint32(const ABTD_atomic_uint32 *ptr)
{
#ifdef ABT_CONFIG_HAVE_ATOMIC_BUILTIN
    return __atomic_load_n(&ptr->val, __ATOMIC_ACQUIRE);
#else
    __sync_synchronize();
    uint32_t val = *(volatile uint32_t *)&ptr->val;
    __sync_synchronize();
    return val;
#endif
}

static inline int64_t ABTD_atomic_acquire_load_int64(const ABTD_atomic_int64 *ptr)
{
#ifdef ABT_CONFIG_HAVE_ATOMIC_BUILTIN
    return __atomic_load_n(&ptr->val, __ATOMIC_ACQUIRE);
#else
    __sync_synchronize();
    int64_t val = *(volatile int64_t *)&ptr->val;
    __sync_synchronize();
    return val;
#endif
}

static inline uint64_t ABTD_atomic_acquire_load_uint64(const ABTD_atomic_uint64 *ptr)
{
    /* return 0 if this test_and_set succeeds to set a value. */
#ifdef ABT_CONFIG_HAVE_ATOMIC_BUILTIN
    return __atomic_load_n(&ptr->val, __ATOMIC_ACQUIRE);
#else
    __sync_synchronize();
    uint64_t val = *(volatile uint64_t *)&ptr->val;
    __sync_synchronize();
    return val;
#endif
}

static inline void *ABTD_atomic_acquire_load_ptr(const ABTD_atomic_ptr *ptr)
{
    /* return 0 if this test_and_set succeeds to set a value. */
#ifdef ABT_CONFIG_HAVE_ATOMIC_BUILTIN
    return __atomic_load_n(&ptr->val, __ATOMIC_ACQUIRE);
#else
    __sync_synchronize();
    void *val = *(void *volatile *)&ptr->val;
    __sync_synchronize();
    return val;
#endif
}

static inline void ABTD_atomic_relaxed_store_int(ABTD_atomic_int *ptr, int val)
{
#ifdef ABT_CONFIG_HAVE_ATOMIC_BUILTIN
    __atomic_store_n(&ptr->val, val, __ATOMIC_RELAXED);
#else
    *(volatile int *)&ptr->val = val;
#endif
}

static inline void ABTD_atomic_relaxed_store_int32(ABTD_atomic_int32 *ptr, int32_t val)
{
#ifdef ABT_CONFIG_HAVE_ATOMIC_BUILTIN
    __atomic_store_n(&ptr->val, val, __ATOMIC_RELAXED);
#else
    *(volatile int32_t *)&ptr->val = val;
#endif
}

static inline void ABTD_atomic_relaxed_store_uint32(ABTD_atomic_uint32 *ptr, uint32_t val)
{
#ifdef ABT_CONFIG_HAVE_ATOMIC_BUILTIN
    __atomic_store_n(&ptr->val, val, __ATOMIC_RELAXED);
#else
    *(volatile uint32_t *)&ptr->val = val;
#endif
}

static inline void ABTD_atomic_relaxed_store_int64(ABTD_atomic_int64 *ptr, int64_t val)
{
#ifdef ABT_CONFIG_HAVE_ATOMIC_BUILTIN
    __atomic_store_n(&ptr->val, val, __ATOMIC_RELAXED);
#else
    *(volatile int64_t *)&ptr->val = val;
#endif
}

static inline void ABTD_atomic_relaxed_store_uint64(ABTD_atomic_uint64 *ptr, uint64_t val)
{
#ifdef ABT_CONFIG_HAVE_ATOMIC_BUILTIN
    __atomic_store_n(&ptr->val, val, __ATOMIC_RELAXED);
#else
    *(volatile uint64_t *)&ptr->val = val;
#endif
}

static inline void ABTD_atomic_relaxed_store_ptr(ABTD_atomic_ptr *ptr, void *val)
{
#ifdef ABT_CONFIG_HAVE_ATOMIC_BUILTIN
    __atomic_store_n(&ptr->val, val, __ATOMIC_RELAXED);
#else
    *(void *volatile *)&ptr->val = val;
#endif
}

static inline void ABTD_atomic_release_store_int(ABTD_atomic_int *ptr, int val)
{
#ifdef ABT_CONFIG_HAVE_ATOMIC_BUILTIN
    __atomic_store_n(&ptr->val, val, __ATOMIC_RELEASE);
#else
    __sync_synchronize();
    *(volatile int *)&ptr->val = val;
    __sync_synchronize();
#endif
}

static inline void ABTD_atomic_release_store_int32(ABTD_atomic_int32 *ptr, int32_t val)
{
#ifdef ABT_CONFIG_HAVE_ATOMIC_BUILTIN
    __atomic_store_n(&ptr->val, val, __ATOMIC_RELEASE);
#else
    __sync_synchronize();
    *(volatile int32_t *)&ptr->val = val;
    __sync_synchronize();
#endif
}

static inline void ABTD_atomic_release_store_uint32(ABTD_atomic_uint32 *ptr, uint32_t val)
{
#ifdef ABT_CONFIG_HAVE_ATOMIC_BUILTIN
    __atomic_store_n(&ptr->val, val, __ATOMIC_RELEASE);
#else
    __sync_synchronize();
    *(volatile uint32_t *)&ptr->val = val;
    __sync_synchronize();
#endif
}

static inline void ABTD_atomic_release_store_int64(ABTD_atomic_int64 *ptr, int64_t val)
{
#ifdef ABT_CONFIG_HAVE_ATOMIC_BUILTIN
    __atomic_store_n(&ptr->val, val, __ATOMIC_RELEASE);
#else
    __sync_synchronize();
    *(volatile int64_t *)&ptr->val = val;
    __sync_synchronize();
#endif
}

static inline void ABTD_atomic_release_store_uint64(ABTD_atomic_uint64 *ptr, uint64_t val)
{
#ifdef ABT_CONFIG_HAVE_ATOMIC_BUILTIN
    __atomic_store_n(&ptr->val, val, __ATOMIC_RELEASE);
#else
    __sync_synchronize();
    *(volatile uint64_t *)&ptr->val = val;
    __sync_synchronize();
#endif
}

static inline void ABTD_atomic_release_store_ptr(ABTD_atomic_ptr *ptr, void *val)
{
#ifdef ABT_CONFIG_HAVE_ATOMIC_BUILTIN
    __atomic_store_n(&ptr->val, val, __ATOMIC_RELEASE);
#else
    __sync_synchronize();
    *(void *volatile *)&ptr->val = val;
    __sync_synchronize();
#endif
}

static inline int ABTD_atomic_exchange_int(ABTD_atomic_int *ptr, int v)
{
#ifdef ABT_CONFIG_HAVE_ATOMIC_BUILTIN
    return __atomic_exchange_n(&ptr->val, v, __ATOMIC_ACQ_REL);
#else
    int val;
    do {
        val = ABTD_atomic_acquire_load_int(ptr);
    } while (!ABTD_atomic_bool_cas_weak_int(ptr, val, v));
    return val;
#endif
}

static inline int32_t ABTD_atomic_exchange_int32(ABTD_atomic_int32 *ptr, int32_t v)
{
#ifdef ABT_CONFIG_HAVE_ATOMIC_BUILTIN
    return __atomic_exchange_n(&ptr->val, v, __ATOMIC_ACQ_REL);
#else
    int32_t val;
    do {
        val = ABTD_atomic_acquire_load_int32(ptr);
    } while (!ABTD_atomic_bool_cas_weak_int32(ptr, val, v));
    return val;
#endif
}

static inline uint32_t ABTD_atomic_exchange_uint32(ABTD_atomic_uint32 *ptr, uint32_t v)
{
#ifdef ABT_CONFIG_HAVE_ATOMIC_BUILTIN
    return __atomic_exchange_n(&ptr->val, v, __ATOMIC_ACQ_REL);
#else
    uint32_t val;
    do {
        val = ABTD_atomic_acquire_load_uint32(ptr);
    } while (!ABTD_atomic_bool_cas_weak_uint32(ptr, val, v));
    return val;
#endif
}

static inline int64_t ABTD_atomic_exchange_int64(ABTD_atomic_int64 *ptr, int64_t v)
{
#ifdef ABT_CONFIG_HAVE_ATOMIC_BUILTIN
    return __atomic_exchange_n(&ptr->val, v, __ATOMIC_ACQ_REL);
#else
    int64_t val;
    do {
        val = ABTD_atomic_acquire_load_int64(ptr);
    } while (!ABTD_atomic_bool_cas_weak_int64(ptr, val, v));
    return val;
#endif
}

static inline uint64_t ABTD_atomic_exchange_uint64(ABTD_atomic_uint64 *ptr, uint64_t v)
{
#ifdef ABT_CONFIG_HAVE_ATOMIC_BUILTIN
    return __atomic_exchange_n(&ptr->val, v, __ATOMIC_ACQ_REL);
#else
    uint64_t val;
    do {
        val = ABTD_atomic_acquire_load_uint64(ptr);
    } while (!ABTD_atomic_bool_cas_weak_uint64(ptr, val, v));
    return val;
#endif
}

static inline void *ABTD_atomic_exchange_ptr(ABTD_atomic_ptr *ptr, void *v)
{
#ifdef ABT_CONFIG_HAVE_ATOMIC_BUILTIN
    return __atomic_exchange_n(&ptr->val, v, __ATOMIC_ACQ_REL);
#else
    void *val;
    do {
        val = ABTD_atomic_acquire_load_ptr(ptr);
    } while (!ABTD_atomic_bool_cas_weak_ptr(ptr, val, v));
    return val;
#endif
}

static inline void ABTD_atomic_mem_barrier(void)
{
#ifdef ABT_CONFIG_HAVE_ATOMIC_BUILTIN
    __atomic_thread_fence(__ATOMIC_ACQ_REL);
#else
    __sync_synchronize();
#endif
}

static inline void ABTD_compiler_barrier(void)
{
    __asm__ __volatile__("" ::: "memory");
}

static inline void ABTD_atomic_pause(void)
{
#ifdef __x86_64__
    __asm__ __volatile__("pause" ::: "memory");
#endif
}

#endif /* ABTD_ATOMIC_H_INCLUDED */
