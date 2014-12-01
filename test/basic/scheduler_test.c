/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abt.h"
#include "abttest.h"

int main(int argc, char *argv[])
{
    int ret;
    ABT_sched s;

    /* Initialize */
    ABT_test_init(argc, argv);

    ABT_sched_create_basic(ABT_SCHED_FIFO, &s);
    ABT_sched_free(&s);

    /* Finalize */
    ret = ABT_test_finalize(0);

    return ret;
}

