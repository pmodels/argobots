/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#ifndef UTIL_H_INCLUDED
#define UTIL_H_INCLUDED

#include <abt.h>
#include <assert.h>
#include <stdlib.h>

#define RAND_PTR ((void *)(intptr_t)0x12345678)
static void setup_env(void)
{
    /* The following speeds up ABT_init(). */
    int ret;
    ret = setenv("ABT_MEM_MAX_NUM_DESCS", "4", 1);
    assert(ret == 0);
    ret = setenv("ABT_MEM_MAX_NUM_STACKS", "4", 1);
    assert(ret == 0);
}
#endif /* UTIL_H_INCLUDED */
