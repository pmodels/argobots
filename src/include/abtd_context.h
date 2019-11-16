/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#ifndef ABTD_CONTEXT_H_INCLUDED
#define ABTD_CONTEXT_H_INCLUDED

#include "abt_config.h"

typedef void *  fcontext_t;

typedef struct ABTD_thread_context {
    fcontext_t             fctx;    /* actual context */
    void (*f_thread)(void *);       /* ULT function */
    void *                 p_arg;   /* ULT function argument */
    struct ABTD_thread_context *p_link;  /* pointer to scheduler context */
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

#include "abtd_fcontext.h"

#endif /* ABTD_CONTEXT_H_INCLUDED */
