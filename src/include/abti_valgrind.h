/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#ifndef ABTI_VALGRIND_H_INCLUDED
#define ABTI_VALGRIND_H_INCLUDED

#ifdef HAVE_VALGRIND_SUPPORT
#include <valgrind/valgrind.h>
#define ABTI_VALGRIND_STACK_REGISTER(p,s)             \
    VALGRIND_STACK_REGISTER(p, ((char *)p)+s)
#else
#define ABTI_VALGRIND_STACK_REGISTER(p,s)
#endif

#endif /* ABTI_VALGRIND_H_INCLUDED */
