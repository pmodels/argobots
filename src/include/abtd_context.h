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

void ABTD_thread_print_context(ABTI_thread *p_thread, FILE *p_os, int indent);

#endif /* ABTD_CONTEXT_H_INCLUDED */
