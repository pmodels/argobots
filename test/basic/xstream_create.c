/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include "abt.h"
#include "abttest.h"

#define DEFAULT_NUM_XSTREAMS    4

int main(int argc, char *argv[])
{
    ABT_xstream *xstreams;
    int num_xstreams = DEFAULT_NUM_XSTREAMS;
    int i;
    int ret, tmp;

    /* Initialize */
    ABT_test_init(argc, argv);

    if (argc > 1) {
        num_xstreams = ABT_test_get_arg_val(ABT_TEST_ARG_N_ES);
    }

    ABT_test_printf(1, "# of ESs: %d\n", num_xstreams);

    xstreams = (ABT_xstream *)malloc(sizeof(ABT_xstream) * num_xstreams);
    assert(xstreams != NULL);

    /* Create Execution Streams */
    ret = ABT_xstream_self(&xstreams[0]);
    ABT_TEST_ERROR(ret, "ABT_xstream_self");
    for (i = 1; i < num_xstreams; i++) {
        ret = ABT_xstream_create(ABT_SCHED_NULL, &xstreams[i]);
        ABT_TEST_ERROR(ret, "ABT_xstream_create");
    }

    /* Get the number of Execution Streams */
    ret = ABT_xstream_get_num(&tmp);
    ABT_TEST_ERROR(ret, "ABT_xstream_get_num");
    assert(tmp == num_xstreams);

    /* Join Execution Streams */
    for (i = 1; i < num_xstreams; i++) {
        ret = ABT_xstream_join(xstreams[i]);
        ABT_TEST_ERROR(ret, "ABT_xstream_join");
    }

    /* Free Execution Streams */
    for (i = 1; i < num_xstreams; i++) {
        ret = ABT_xstream_free(&xstreams[i]);
        ABT_TEST_ERROR(ret, "ABT_xstream_free");
    }

    /* Finalize */
    ret = ABT_test_finalize(0);

    free(xstreams);

    return ret;
}
