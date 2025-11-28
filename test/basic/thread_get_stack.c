/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include "abt.h"
#include "abttest.h"

#define DEFAULT_NUM_THREADS 4

void thread_func(void *arg)
{
    int ret;
    void *stackaddr1 = NULL, *stackaddr2 = NULL;
    size_t stacksize1 = 0, stacksize2 = 0;
    ABT_thread self;
    ABT_thread_attr attr;

    /* Get self */
    ret = ABT_thread_self(&self);
    ATS_ERROR(ret, "ABT_thread_self");

    /* Method 1: New direct API */
    ret = ABT_thread_get_stack(self, &stackaddr1, &stacksize1);
    ATS_ERROR(ret, "ABT_thread_get_stack");

    /* Method 2: Old method via attributes (for comparison) */
    ret = ABT_thread_get_attr(self, &attr);
    ATS_ERROR(ret, "ABT_thread_get_attr");

    ret = ABT_thread_attr_get_stack(attr, &stackaddr2, &stacksize2);
    ATS_ERROR(ret, "ABT_thread_attr_get_stack");

    ret = ABT_thread_attr_free(&attr);
    ATS_ERROR(ret, "ABT_thread_attr_free");

    /* Verify both methods return the same values */
    if (stackaddr1 != stackaddr2) {
        ATS_ERROR(ABT_ERR_OTHER, "Stack addresses don't match");
    }
    if (stacksize1 != stacksize2) {
        ATS_ERROR(ABT_ERR_OTHER, "Stack sizes don't match");
    }

    ATS_printf(2, "Thread: stackaddr=%p, stacksize=%zu\n", stackaddr1,
               stacksize1);

    /* Test partial queries */
    void *stackaddr_only = NULL;
    ret = ABT_thread_get_stack(self, &stackaddr_only, NULL);
    ATS_ERROR(ret, "ABT_thread_get_stack (addr only)");
    if (stackaddr_only != stackaddr1) {
        ATS_ERROR(ABT_ERR_OTHER, "Stackaddr-only query mismatch");
    }

    size_t stacksize_only = 0;
    ret = ABT_thread_get_stack(self, NULL, &stacksize_only);
    ATS_ERROR(ret, "ABT_thread_get_stack (size only)");
    if (stacksize_only != stacksize1) {
        ATS_ERROR(ABT_ERR_OTHER, "Stacksize-only query mismatch");
    }
}

int main(int argc, char *argv[])
{
    int ret, i;
    int num_threads = DEFAULT_NUM_THREADS;
    ABT_xstream xstream;
    ABT_pool pool;
    ABT_thread *threads;

    /* Initialize */
    ATS_read_args(argc, argv);
    if (argc > 1) {
        num_threads = ATS_get_arg_val(ATS_ARG_N_ULT);
    }
    ATS_init(argc, argv, 1);

    ATS_printf(1, "# of ULTs: %d\n", num_threads);

    threads = (ABT_thread *)malloc(num_threads * sizeof(ABT_thread));

    /* Get the primary execution stream */
    ret = ABT_xstream_self(&xstream);
    ATS_ERROR(ret, "ABT_xstream_self");

    /* Get the pool */
    ret = ABT_xstream_get_main_pools(xstream, 1, &pool);
    ATS_ERROR(ret, "ABT_xstream_get_main_pools");

    /* Create ULTs */
    for (i = 0; i < num_threads; i++) {
        ret = ABT_thread_create(pool, thread_func, NULL, ABT_THREAD_ATTR_NULL,
                                &threads[i]);
        ATS_ERROR(ret, "ABT_thread_create");
    }

    /* Join and free ULTs */
    for (i = 0; i < num_threads; i++) {
        ret = ABT_thread_free(&threads[i]);
        ATS_ERROR(ret, "ABT_thread_free");
    }

    /* Test with external thread (should return NULL/0) */
    void *ext_stackaddr = (void *)0xdeadbeef;
    size_t ext_stacksize = 12345;
    ABT_thread ext_thread;

    ret = ABT_thread_self(&ext_thread);
    ATS_ERROR(ret, "ABT_thread_self");

    ret = ABT_thread_get_stack(ext_thread, &ext_stackaddr, &ext_stacksize);
    ATS_ERROR(ret, "ABT_thread_get_stack (external)");

    ATS_printf(2, "External thread: stackaddr=%p, stacksize=%zu\n",
               ext_stackaddr, ext_stacksize);

    /* Note: External thread may have a stack or not depending on version */
    /* Just verify the call succeeds */

    /* Finalize */
    ret = ATS_finalize(0);

    free(threads);

    return ret;
}
