/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#ifndef ABTD_ATOMIC_H_INCLUDED
#define ABTD_ATOMIC_H_INCLUDED

#include <stdint.h>

static inline
int32_t ABTD_atomic_cas_int32(int32_t *ptr, int32_t oldv, int32_t newv)
{
    return __sync_val_compare_and_swap(ptr, oldv, newv);
}

static inline
uint32_t ABTD_atomic_cas_uint32(uint32_t *ptr, uint32_t oldv, uint32_t newv)
{
    return __sync_val_compare_and_swap(ptr, oldv, newv);
}

static inline
int64_t ABTD_atomic_cas_int64(int64_t *ptr, int64_t oldv, int64_t newv)
{
    return __sync_val_compare_and_swap(ptr, oldv, newv);
}

static inline
uint64_t ABTD_atomic_cas_uint64(uint64_t *ptr, uint64_t oldv, uint64_t newv)
{
    return __sync_val_compare_and_swap(ptr, oldv, newv);
}

static inline
int32_t ABTD_atomic_fetch_add_int32(int32_t *ptr, int32_t v)
{
    return __sync_fetch_and_add(ptr, v);
}

static inline
uint32_t ABTD_atomic_fetch_add_uint32(uint32_t *ptr, uint32_t v)
{
    return __sync_fetch_and_add(ptr, v);
}

static inline
int64_t ABTD_atomic_fetch_add_int64(int64_t *ptr, int64_t v)
{
    return __sync_fetch_and_add(ptr, v);
}

static inline
uint64_t ABTD_atomic_fetch_add_uint64(uint64_t *ptr, uint64_t v)
{
    return __sync_fetch_and_add(ptr, v);
}

static inline
double ABTD_atomic_fetch_add_double(double *ptr, double v)
{
    union value {
        double d_val;
        uint64_t u_val;
    } oldv, newv;

    do {
        oldv.d_val = *ptr;
        newv.d_val = oldv.d_val + v;
    } while (ABTD_atomic_cas_uint64((uint64_t *)ptr, oldv.u_val, newv.u_val)
             != oldv.u_val);

    return oldv.d_val;
}

static inline
int32_t ABTD_atomic_fetch_sub_int32(int32_t *ptr, int32_t v)
{
    return __sync_fetch_and_sub(ptr, v);
}

static inline
uint32_t ABTD_atomic_fetch_sub_uint32(uint32_t *ptr, uint32_t v)
{
    return __sync_fetch_and_sub(ptr, v);
}

static inline
int64_t ABTD_atomic_fetch_sub_int64(int64_t *ptr, int64_t v)
{
    return __sync_fetch_and_sub(ptr, v);
}

static inline
uint64_t ABTD_atomic_fetch_sub_uint64(uint64_t *ptr, uint64_t v)
{
    return __sync_fetch_and_sub(ptr, v);
}

static inline
double ABTD_atomic_fetch_sub_double(double *ptr, double v)
{
    return ABTD_atomic_fetch_add_double(ptr, -v);
}

static inline
int32_t ABTD_atomic_fetch_and_int32(int32_t *ptr, int32_t v)
{
    return __sync_fetch_and_and(ptr, v);
}

static inline
uint32_t ABTD_atomic_fetch_and_uint32(uint32_t *ptr, uint32_t v)
{
    return __sync_fetch_and_and(ptr, v);
}

static inline
int64_t ABTD_atomic_fetch_and_int64(int64_t *ptr, int64_t v)
{
    return __sync_fetch_and_and(ptr, v);
}

static inline
uint64_t ABTD_atomic_fetch_and_uint64(uint64_t *ptr, uint64_t v)
{
    return __sync_fetch_and_and(ptr, v);
}

static inline
int32_t ABTD_atomic_fetch_or_int32(int32_t *ptr, int32_t v)
{
    return __sync_fetch_and_or(ptr, v);
}

static inline
uint32_t ABTD_atomic_fetch_or_uint32(uint32_t *ptr, uint32_t v)
{
    return __sync_fetch_and_or(ptr, v);
}

static inline
int64_t ABTD_atomic_fetch_or_int64(int64_t *ptr, int64_t v)
{
    return __sync_fetch_and_or(ptr, v);
}

static inline
uint64_t ABTD_atomic_fetch_or_uint64(uint64_t *ptr, uint64_t v)
{
    return __sync_fetch_and_or(ptr, v);
}

static inline
int32_t ABTD_atomic_fetch_xor_int32(int32_t *ptr, int32_t v)
{
    return __sync_fetch_and_xor(ptr, v);
}

static inline
uint32_t ABTD_atomic_fetch_xor_uint32(uint32_t *ptr, uint32_t v)
{
    return __sync_fetch_and_xor(ptr, v);
}

static inline
int64_t ABTD_atomic_fetch_xor_int64(int64_t *ptr, int64_t v)
{
    return __sync_fetch_and_xor(ptr, v);
}

static inline
uint64_t ABTD_atomic_fetch_xor_uint64(uint64_t *ptr, uint64_t v)
{
    return __sync_fetch_and_xor(ptr, v);
}

#ifdef ABT_CONFIG_HAVE_ATOMIC_EXCHANGE
static inline
int32_t ABTD_atomic_exchange_int32(int32_t *ptr, int32_t v)
{
    return __atomic_exchange_n(ptr, v, __ATOMIC_SEQ_CST);
}

static inline
uint32_t ABTD_atomic_exchange_uint32(uint32_t *ptr, uint32_t v)
{
    return __atomic_exchange_n(ptr, v, __ATOMIC_SEQ_CST);
}

static inline
int64_t ABTD_atomic_exchange_int64(int64_t *ptr, int64_t v)
{
    return __atomic_exchange_n(ptr, v, __ATOMIC_SEQ_CST);
}

static inline
uint64_t ABTD_atomic_exchange_uint64(uint64_t *ptr, uint64_t v)
{
    return __atomic_exchange_n(ptr, v, __ATOMIC_SEQ_CST);
}
#endif

static inline
void ABTD_atomic_mem_barrier(void)
{
    __sync_synchronize();
}

static inline
void ABTD_compiler_barrier(void)
{
    __asm__ __volatile__ ( "" ::: "memory" );
}

static inline
void ABTD_atomic_pause(void)
{
    __asm__ __volatile__ ( "pause" ::: "memory" );
}

#endif /* ABTD_ATOMIC_H_INCLUDED */
