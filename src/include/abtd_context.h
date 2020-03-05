/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#ifndef ABTD_CONTEXT_H_INCLUDED
#define ABTD_CONTEXT_H_INCLUDED

#include "abt_config.h"

#ifndef ABT_CONFIG_USE_FCONTEXT
#define _XOPEN_SOURCE
#include <ucontext.h>
#endif

typedef struct ABTD_thread_context {
    void *p_ctx;                        /* actual context of fcontext, or a
                                         * pointer to uctx */
    void (*f_thread)(void *);           /* ULT function */
    void *p_arg;                        /* ULT function argument */
    struct ABTD_thread_context *p_link; /* pointer to scheduler context */
#ifndef ABT_CONFIG_USE_FCONTEXT
    ucontext_t uctx;               /* ucontext entity pointed by p_ctx */
    void (*f_uctx_thread)(void *); /* root function called by ucontext */
    void *p_uctx_arg;              /* argument for root function */
#endif
} ABTD_thread_context;

static void ABTD_thread_context_make(ABTD_thread_context *p_ctx, void *sp,
                                     size_t size, void (*thread_func)(void *));
static void ABTD_thread_context_jump(ABTD_thread_context *p_old,
                                     ABTD_thread_context *p_new, void *arg);
static void ABTD_thread_context_take(ABTD_thread_context *p_old,
                                     ABTD_thread_context *p_new, void *arg);
#if ABT_CONFIG_THREAD_TYPE == ABT_THREAD_TYPE_DYNAMIC_PROMOTION
static void ABTD_thread_context_init_and_call(ABTD_thread_context *p_ctx,
                                              void *sp,
                                              void (*thread_func)(void *),
                                              void *arg);
#endif

void ABTD_thread_print_context(ABTI_thread *p_thread, FILE *p_os, int indent);

#ifdef ABT_CONFIG_USE_FCONTEXT
#include "abtd_fcontext.h"
#else
#include "abtd_ucontext.h"
#endif

#endif /* ABTD_CONTEXT_H_INCLUDED */
