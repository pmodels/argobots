/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#ifndef ABTU_H_INCLUDED
#define ABTU_H_INCLUDED

#include <stdlib.h>
#include <string.h>

/* Utility Functions */
#define ABTU_Malloc(a)          malloc((size_t)(a))
#define ABTU_Callos(a,b)        calloc((size_t)(a),(size_t)b)
#define ABTU_Free(a)            free((void *)(a))
#define ABTU_Realloc(a,b)       realloc((void *)(a),(size_t)(b))

#define ABTU_Strcpy(d,s)        strcpy(d,s)
#define ABTU_Strncpy(d,s,n)     strncpy(d,s,n)

#endif /* ABTU_H_INCLUDED */
