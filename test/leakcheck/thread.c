/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include <stdlib.h>
#include <assert.h>
#include <abt.h>
#include "rtrace.h"
#include "util.h"

/* Check ABT_thread. */

#define MAX_THREADS 8
#define THREAD_STACK_SIZE (512 * 1024)

void thread_func(void *arg)
{
    if (arg) {
        int ret = ABT_self_yield();
        assert(ret == ABT_SUCCESS);
    }
}

int create_thread_default(int push_to_pool, ABT_thread *p_thread,
                          int must_succeed)
{
    int ret;
    ABT_xstream self_xstream;
    ret = ABT_self_get_xstream(&self_xstream);
    assert(ret == ABT_SUCCESS);
    if (p_thread)
        *p_thread = (ABT_thread)RAND_PTR;
    if (push_to_pool) {
        ABT_pool pool;
        ret = ABT_xstream_get_main_pools(self_xstream, 1, &pool);
        assert(ret == ABT_SUCCESS);
        ret = ABT_thread_create(pool, thread_func, (void *)&ret,
                                ABT_THREAD_ATTR_NULL, p_thread);
    } else {
        ret = ABT_thread_create_on_xstream(self_xstream, thread_func,
                                           (void *)&ret, ABT_THREAD_ATTR_NULL,
                                           p_thread);
    }
    assert(!must_succeed || ret == ABT_SUCCESS);
    if (ret != ABT_SUCCESS && p_thread) {
#ifdef ABT_ENABLE_VER_20_API
        assert(*p_thread == (ABT_thread)RAND_PTR);
        *p_thread = ABT_THREAD_NULL;
#else
        assert(*p_thread == ABT_THREAD_NULL);
#endif
    }
    return ret;
}

int create_thread_usersize(int push_to_pool, ABT_thread *p_thread,
                           int must_succeed)
{
    int ret;
    ABT_thread_attr attr = (ABT_thread_attr)RAND_PTR;
    ret = ABT_thread_attr_create(&attr);
    assert(!must_succeed || ret == ABT_SUCCESS);
    if (ret != ABT_SUCCESS) {
#ifdef ABT_ENABLE_VER_20_API
        assert(attr == (ABT_thread_attr)RAND_PTR);
#else
        assert(attr == ABT_THREAD_ATTR_NULL);
#endif
        return ret;
    }
    ret = ABT_thread_attr_set_stacksize(attr, THREAD_STACK_SIZE);
    assert(ret == ABT_SUCCESS);

    ABT_xstream self_xstream;
    ret = ABT_self_get_xstream(&self_xstream);
    assert(ret == ABT_SUCCESS);
    if (p_thread)
        *p_thread = (ABT_thread)RAND_PTR;
    if (push_to_pool) {
        ABT_pool pool;
        ret = ABT_xstream_get_main_pools(self_xstream, 1, &pool);
        assert(ret == ABT_SUCCESS);
        ret =
            ABT_thread_create(pool, thread_func, (void *)&ret, attr, p_thread);
    } else {
        ret = ABT_thread_create_on_xstream(self_xstream, thread_func,
                                           (void *)&ret, attr, p_thread);
    }
    assert(!must_succeed || ret == ABT_SUCCESS);
    if (ret != ABT_SUCCESS && p_thread) {
#ifdef ABT_ENABLE_VER_20_API
        assert(*p_thread == (ABT_thread)RAND_PTR);
        *p_thread = ABT_THREAD_NULL;
#else
        assert(*p_thread == ABT_THREAD_NULL);
#endif
    }
    ret = ABT_thread_attr_free(&attr);
    assert(ret == ABT_SUCCESS && attr == ABT_THREAD_ATTR_NULL);
    return ret;
}

int create_thread_nonyieldable(int push_to_pool, ABT_thread *p_thread,
                               int must_succeed)
{
    int ret;
    ABT_xstream self_xstream;
    ret = ABT_self_get_xstream(&self_xstream);
    assert(ret == ABT_SUCCESS);
    if (p_thread)
        *p_thread = (ABT_thread)RAND_PTR;
    if (push_to_pool) {
        ABT_pool pool;
        ret = ABT_xstream_get_main_pools(self_xstream, 1, &pool);
        assert(ret == ABT_SUCCESS);
        ret = ABT_task_create(pool, thread_func, NULL, p_thread);
    } else {
        ret = ABT_task_create_on_xstream(self_xstream, thread_func, NULL,
                                         p_thread);
    }
    assert(!must_succeed || ret == ABT_SUCCESS);
    if (ret != ABT_SUCCESS && p_thread) {
#ifdef ABT_ENABLE_VER_20_API
        assert(*p_thread == (ABT_thread)RAND_PTR);
        *p_thread = ABT_THREAD_NULL;
#else
        assert(*p_thread == ABT_TASK_NULL);
#endif
    }
    return ret;
}

void program(int push_to_pool, int named, int type, int must_succeed)
{
    int ret;
    rtrace_set_enabled(0);
    /* Checking ABT_init() should be done by other tests. */
    ret = ABT_init(0, 0);
    assert(ret == ABT_SUCCESS);
    rtrace_set_enabled(1);

    int i;

    ABT_thread threads[MAX_THREADS];
    for (i = 0; i < MAX_THREADS; i++) {
        threads[i] = ABT_THREAD_NULL;
    }

    for (i = 0; i < MAX_THREADS; i++) {
        if (type == 0) {
            ret =
                create_thread_default(push_to_pool, named ? &threads[i] : NULL,
                                      must_succeed);
        } else if (type == 1) {
            ret =
                create_thread_usersize(push_to_pool, named ? &threads[i] : NULL,
                                       must_succeed);
        } else if (type == 2) {
            ret = create_thread_nonyieldable(push_to_pool,
                                             named ? &threads[i] : NULL,
                                             must_succeed);
        }
        assert(!must_succeed || ret == ABT_SUCCESS);
        if (ret != ABT_SUCCESS) {
            threads[i] = ABT_THREAD_NULL;
            if (i < MAX_THREADS - 2) {
                i = MAX_THREADS - 2; /* Create at most one more. */
            }
        }
    }
    if (named) {
        for (i = 0; i < MAX_THREADS; i++) {
            if (threads[i] != ABT_THREAD_NULL) {
                ret = ABT_thread_free(&threads[i]);
                assert(ret == ABT_SUCCESS && threads[i] == ABT_THREAD_NULL);
            }
        }
    }

    ret = ABT_finalize();
    assert(ret == ABT_SUCCESS);
}

int main()
{
    setup_env();
    rtrace_init();

    /* Set a large stack size to use multiple buckets of the memory pool. */
    int ret;
    ret = setenv("ABT_THREAD_STACKSIZE", "512000", 1);
    assert(ret == 0);
    ret = setenv("ABT_MEM_MAX_NUM_STACKS", "4", 1);
    assert(ret == 0);
    ret = setenv("MEM_MAX_NUM_DESCS", "4", 1);
    assert(ret == 0);

    int push_to_pool, named, type;
    for (push_to_pool = 0; push_to_pool <= 1; push_to_pool++) {
        for (named = 0; named <= 1; named++) {
            for (type = 0; type < 3; type++) {
                do {
                    rtrace_start();
                    program(push_to_pool, named, type, 0);
                } while (!rtrace_stop());

                /* If no failure, it should succeed again. */
                program(push_to_pool, named, type, 1);
            }
        }
    }

    rtrace_finalize();
    return 0;
}
