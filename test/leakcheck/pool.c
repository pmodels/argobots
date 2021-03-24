/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include <stdlib.h>
#include <assert.h>
#include <abt.h>
#include "rtrace.h"
#include "util.h"

/* Check ABT_pool. */

#define POOL_KIND_USER ((ABT_pool_kind)999)

ABT_unit unit_create_from_thread(ABT_thread thread)
{
    return (ABT_unit)thread;
}

void unit_free(ABT_unit *p_unit)
{
    (void)p_unit;
}

typedef struct {
    int num_units;
    ABT_unit units[16];
} pool_data_t;

int pool_init(ABT_pool pool, ABT_pool_config config)
{
    (void)config;
    pool_data_t *pool_data = (pool_data_t *)malloc(sizeof(pool_data_t));
    if (!pool_data) {
        return ABT_ERR_MEM;
    }
    pool_data->num_units = 0;
    int ret = ABT_pool_set_data(pool, pool_data);
    assert(ret == ABT_SUCCESS);
    return ABT_SUCCESS;
}

size_t pool_get_size(ABT_pool pool)
{
    pool_data_t *pool_data;
    int ret = ABT_pool_get_data(pool, (void **)&pool_data);
    assert(ret == ABT_SUCCESS);
    return pool_data->num_units;
}

void pool_push(ABT_pool pool, ABT_unit unit)
{
    /* Very simple: no lock, fixed size.  This implementation is for simplicity,
     * so don't use it in a real program unless you know what you are really
     * doing. */
    pool_data_t *pool_data;
    int ret = ABT_pool_get_data(pool, (void **)&pool_data);
    assert(ret == ABT_SUCCESS);
    pool_data->units[pool_data->num_units++] = unit;
}

ABT_unit pool_pop(ABT_pool pool)
{
    pool_data_t *pool_data;
    int ret = ABT_pool_get_data(pool, (void **)&pool_data);
    assert(ret == ABT_SUCCESS);
    if (pool_data->num_units == 0)
        return ABT_UNIT_NULL;
    return pool_data->units[--pool_data->num_units];
}

int pool_free(ABT_pool pool)
{
    pool_data_t *pool_data;
    int ret = ABT_pool_get_data(pool, (void **)&pool_data);
    assert(ret == ABT_SUCCESS);
    free(pool_data);
    return ABT_SUCCESS;
}

ABT_pool create_pool(int automatic, int must_succeed)
{
    if (automatic) {
        /* Currently there's no way to create an automatic pool using
         * ABT_pool_create(). */
        return ABT_POOL_NULL;
    }
    int ret;
    ABT_pool pool = (ABT_pool)RAND_PTR;

    ABT_pool_def pool_def;
    pool_def.access = ABT_POOL_ACCESS_MPMC;
    pool_def.u_get_type = NULL;
    pool_def.u_get_thread = NULL;
    pool_def.u_get_task = NULL;
    pool_def.u_is_in_pool = NULL;
    pool_def.u_create_from_thread = unit_create_from_thread;
    pool_def.u_create_from_task = NULL;
    pool_def.u_free = unit_free;
    pool_def.p_init = pool_init;
    pool_def.p_get_size = pool_get_size;
    pool_def.p_push = pool_push;
    pool_def.p_pop = pool_pop;
#ifdef ABT_ENABLE_VER_20_API
    pool_def.p_pop_wait = NULL;
#endif
    pool_def.p_pop_timedwait = NULL;
    pool_def.p_remove = NULL;
    pool_def.p_free = pool_free;
    pool_def.p_print_all = NULL;

    ret = ABT_pool_create(&pool_def, ABT_POOL_CONFIG_NULL, &pool);
    assert(!must_succeed || ret == ABT_SUCCESS);
    if (ret != ABT_SUCCESS) {
#ifdef ABT_ENABLE_VER_20_API
        assert(pool == (ABT_pool)RAND_PTR);
#else
        assert(pool == ABT_POOL_NULL);
#endif
        return ABT_POOL_NULL;
    }
    return pool;
}

ABT_pool create_pool_basic(ABT_pool_kind kind, int automatic, int must_succeed)
{
    int ret;
    ABT_pool pool = (ABT_pool)RAND_PTR;
    ret = ABT_pool_create_basic(kind, ABT_POOL_ACCESS_MPMC,
                                automatic ? ABT_TRUE : ABT_FALSE, &pool);
    assert(!must_succeed || ret == ABT_SUCCESS);
    if (ret != ABT_SUCCESS) {
#ifdef ABT_ENABLE_VER_20_API
        assert(pool == (ABT_pool)RAND_PTR);
#else
        assert(pool == ABT_POOL_NULL);
#endif
        return ABT_POOL_NULL;
    }
    return pool;
}

void program(ABT_pool_kind kind, int automatic, int type, int must_succeed)
{
    int ret;
    rtrace_set_enabled(0);
    /* Checking ABT_init() should be done by other tests. */
    ret = ABT_init(0, 0);
    assert(ret == ABT_SUCCESS);
    rtrace_set_enabled(1);

    ABT_pool pool;
    if (kind == POOL_KIND_USER) {
        pool = create_pool(automatic, must_succeed);
    } else {
        pool = create_pool_basic(kind, automatic, must_succeed);
    }

    if (pool == ABT_POOL_NULL) {
        /* Allocation failed, so do nothing. */
    } else if (type == 0) {
        /* Just free.  We must free an automatic one too. */
        ret = ABT_pool_free(&pool);
        assert(ret == ABT_SUCCESS && pool == ABT_POOL_NULL);
    } else if (type == 1) {
        /* Use it for ABT_sched_create_basic(). */
        ABT_sched sched = (ABT_sched)RAND_PTR;
        ret = ABT_sched_create_basic(ABT_SCHED_DEFAULT, 1, &pool,
                                     ABT_SCHED_CONFIG_NULL, &sched);
        assert(!must_succeed || ret == ABT_SUCCESS);
        if (ret != ABT_SUCCESS) {
#ifdef ABT_ENABLE_VER_20_API
            assert(sched == (ABT_sched)RAND_PTR);
#else
            assert(sched == ABT_SCHED_NULL);
#endif
            /* Maybe the second time will succeed. */
            ret = ABT_sched_create_basic(ABT_SCHED_DEFAULT, 1, &pool,
                                         ABT_SCHED_CONFIG_NULL, &sched);
            if (ret != ABT_SUCCESS) {
                /* Second time failed.  Give up. */
                ret = ABT_pool_free(&pool);
                assert(ret == ABT_SUCCESS && pool == ABT_POOL_NULL);
                sched = ABT_SCHED_NULL;
            }
        }
        if (sched != ABT_SCHED_NULL) {
            ret = ABT_sched_free(&sched);
            assert(ret == ABT_SUCCESS && sched == ABT_SCHED_NULL);
            if (!automatic) {
                ret = ABT_pool_free(&pool);
                assert(ret == ABT_SUCCESS && pool == ABT_POOL_NULL);
            }
        }
    } else if (type == 2) {
        /* Use it for ABT_xstream_create_basic(). */
        ABT_xstream xstream = (ABT_xstream)RAND_PTR;
        ret = ABT_xstream_create_basic(ABT_SCHED_DEFAULT, 1, &pool,
                                       ABT_SCHED_CONFIG_NULL, &xstream);
        assert(!must_succeed || ret == ABT_SUCCESS);
        if (ret != ABT_SUCCESS) {
#ifdef ABT_ENABLE_VER_20_API
            assert(xstream == (ABT_xstream)RAND_PTR);
#else
            assert(xstream == ABT_XSTREAM_NULL);
#endif
            /* Maybe the second time will succeed. */
            ret = ABT_xstream_create_basic(ABT_SCHED_DEFAULT, 1, &pool,
                                           ABT_SCHED_CONFIG_NULL, &xstream);
            if (ret != ABT_SUCCESS) {
                /* Second time failed.  Give up. */
                ret = ABT_pool_free(&pool);
                assert(ret == ABT_SUCCESS && pool == ABT_POOL_NULL);
                xstream = ABT_XSTREAM_NULL;
            }
        }
        if (xstream != ABT_XSTREAM_NULL) {
            ret = ABT_xstream_free(&xstream);
            assert(ret == ABT_SUCCESS && xstream == ABT_XSTREAM_NULL);
            if (!automatic) {
                ret = ABT_pool_free(&pool);
                assert(ret == ABT_SUCCESS && pool == ABT_POOL_NULL);
            }
        }
    } else if (type == 3) {
        /* Use it for ABT_xstream_set_main_sched_basic() */
        ABT_xstream self_xstream;
        ret = ABT_self_get_xstream(&self_xstream);
        assert(ret == ABT_SUCCESS);
        ret = ABT_xstream_set_main_sched_basic(self_xstream, ABT_SCHED_DEFAULT,
                                               1, &pool);
        assert(!must_succeed || ret == ABT_SUCCESS);
        if (ret != ABT_SUCCESS) {
            ret = ABT_pool_free(&pool);
            assert(ret == ABT_SUCCESS && pool == ABT_POOL_NULL);
        }
    }
    ret = ABT_finalize();
    assert(ret == ABT_SUCCESS);
}

int main()
{
    setup_env();
    rtrace_init();

    int i, automatic, type;
    ABT_pool_kind kinds[] = { ABT_POOL_FIFO, POOL_KIND_USER };
    /* Checking all takes too much time. */
    for (i = 0; i < (int)(sizeof(kinds) / sizeof(kinds[0])); i++) {
        for (automatic = 0; automatic <= 1; automatic++) {
            for (type = 0; type < 4; type++) {
                do {
                    rtrace_start();
                    program(kinds[i], automatic, type, 0);
                } while (!rtrace_stop());

                /* If no failure, it should succeed again. */
                program(kinds[i], automatic, type, 1);
            }
        }
    }

    ABT_pool_kind extra_kinds[] = { ABT_POOL_FIFO_WAIT };
    for (i = 0; i < (int)(sizeof(extra_kinds) / sizeof(extra_kinds[0])); i++) {
        for (automatic = 0; automatic <= 1; automatic++) {
            for (type = 0; type < 1; type++) {
                do {
                    rtrace_start();
                    program(extra_kinds[i], automatic, type, 0);
                } while (!rtrace_stop());

                /* If no failure, it should succeed again. */
                program(extra_kinds[i], automatic, type, 1);
            }
        }
    }

    rtrace_finalize();
    return 0;
}
