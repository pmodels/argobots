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
    ABT_xstream xstream;

    /* Initialize */
    ABT_test_init(argc, argv);

    ABT_sched_create_basic(ABT_SCHED_POOL_FIFO, 0, NULL, 1, &s);

    ABT_xstream_self(&xstream);
    ABT_xstream_set_main_sched(xstream, s);

    /* Finalize */
    ret = ABT_test_finalize(0);

    return ret;
}

