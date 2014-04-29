/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#ifndef ABTMEM_H_INCLUDED
#define ABTMEM_H_INCLUDED

#include <stdlib.h>

#define ABTU_Malloc(a)      malloc((size_t)(a))
#define ABTU_Callos(a,b)    calloc((size_t)(a),(size_t)b)
#define ABTU_Free(a)        free((void *)(a))
#define ABTU_Realloc(a,b)   realloc((void *)(a),(size_t)(b))

#endif /* ABTMEM_H_INCLUDED */
