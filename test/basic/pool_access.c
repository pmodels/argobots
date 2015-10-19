/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include "abt.h"
#include "abttest.h"


ABT_pool_access accesses[5] = {
  ABT_POOL_ACCESS_PRIV, ABT_POOL_ACCESS_SPSC, ABT_POOL_ACCESS_MPSC,
  ABT_POOL_ACCESS_SPMC, ABT_POOL_ACCESS_MPMC,
};

int add_to_another_ES(int accessIdx, int result)
{
    int ret;
    int s;
    ABT_pool_access access = accesses[accessIdx];

    ABT_test_printf(1, "add_to_another_ES: %d\n", accessIdx);

    ABT_pool pool;
    ret = ABT_pool_create_basic(ABT_POOL_FIFO, access, ABT_TRUE, &pool);
    ABT_TEST_ERROR(ret, "ABT_pool_create_basic");

    ABT_sched scheds[3];
    for (s = 0; s < 3; s++) {
        ret = ABT_sched_create_basic(ABT_SCHED_DEFAULT, 1, &pool,
                                     ABT_SCHED_CONFIG_NULL, &scheds[s]);
        ABT_TEST_ERROR(ret, "ABT_sched_create_basic");
    }

    /* Creation of two ESs */
    ABT_xstream xstream1, xstream2;
    ret = ABT_xstream_create(ABT_SCHED_NULL, &xstream1);
    ABT_TEST_ERROR(ret, "ABT_xstream_create");
    ret = ABT_xstream_create(ABT_SCHED_NULL, &xstream2);
    ABT_TEST_ERROR(ret, "ABT_xstream_create");
    /* Get the pools */
    ABT_pool pool1, pool2;
    ret = ABT_xstream_get_main_pools(xstream1, 1, &pool1);
    ABT_TEST_ERROR(ret, "ABT_xstream_get_main_pools");
    ret = ABT_xstream_get_main_pools(xstream2, 1, &pool2);
    ABT_TEST_ERROR(ret, "ABT_xstream_get_main_pools");

    /* Use the pool with two schedulers in the same ES */
    ret =  ABT_pool_add_sched(pool1, scheds[0]);
    ABT_TEST_ERROR(ret, "ABT_pool_add_sched");

    ret =  ABT_pool_add_sched(pool1, scheds[1]);
    ABT_TEST_ERROR(ret, "ABT_pool_add_sched");

    /* Use the pool with another scheduler in another ES */
    ret =  ABT_pool_add_sched(pool2, scheds[2]);

    /* Free scheds[2] if it was not added to pool2 */
    if (ret != ABT_SUCCESS) {
        ABT_TEST_ERROR(ABT_sched_free(&scheds[2]), "ABT_sched_free");
    }

    ABT_TEST_ERROR(ABT_xstream_join(xstream1), "ABT_xstream_join");
    ABT_TEST_ERROR(ABT_xstream_join(xstream2), "ABT_xstream_join");
    ABT_TEST_ERROR(ABT_xstream_free(&xstream1), "ABT_xstream_free");
    ABT_TEST_ERROR(ABT_xstream_free(&xstream2), "ABT_xstream_free");

    if ((ret != ABT_SUCCESS && result == ABT_SUCCESS) ||
        (ret == ABT_SUCCESS && result != ABT_SUCCESS))
        return ABT_ERR_INV_POOL_ACCESS;
    else
        return ABT_SUCCESS;
}

void task_func1(void *arg)
{
    int ret;
    void **args = (void **)arg;
    int result           = *(int *)      args[0];
    ABT_pool pool_main   = *(ABT_pool *) args[1];
    ABT_pool pool_dest   = *(ABT_pool *) args[2];
    ABT_sched sched_dest = *(ABT_sched *)args[3];
    ABT_sched sched      = *(ABT_sched *)args[4];

    ret = ABT_pool_add_sched(pool_main, sched_dest);
    ABT_TEST_ERROR(ret, "ABT_pool_add_sched");

    ret = ABT_pool_add_sched(pool_dest, sched);
    if (ret != ABT_SUCCESS) {
        ABT_TEST_ERROR(ABT_sched_free(&sched), "ABT_sched_free");
    }

    if ((ret != ABT_SUCCESS && result == ABT_SUCCESS) ||
        (ret == ABT_SUCCESS && result != ABT_SUCCESS))
        ret = ABT_ERR_INV_POOL_ACCESS;
    else
        ret = ABT_SUCCESS;

    ABT_TEST_ERROR(ret, "ABT_task_create");
}

int add_to_another_access(int accessIdx, int *results)
{
    int ret;
    int p;
    ABT_pool_access access = accesses[accessIdx];

    for (p = 0; p < 5; p++) {
        ABT_test_printf(1, "add_to_another_access: %d-%d\n", accessIdx, p);

        /* Creation of the ES */
        ABT_xstream xstream;
        ret = ABT_xstream_create_basic(ABT_SCHED_DEFAULT, 0, NULL,
                                       ABT_SCHED_CONFIG_NULL, &xstream);
        ABT_TEST_ERROR(ret, "ABT_xstream_create_basic");
        /* Get the pool */
        ABT_pool pool_main;
        ret = ABT_xstream_get_main_pools(xstream, 1, &pool_main);
        ABT_TEST_ERROR(ret, "ABT_xstream_get_main_pools");

        /* Test */
        ABT_pool pool_dest;
        ret = ABT_pool_create_basic(ABT_POOL_FIFO, accesses[p], ABT_TRUE, &pool_dest);
        ABT_TEST_ERROR(ret, "ABT_pool_create_basic");
        ABT_sched sched_dest;
        ret = ABT_sched_create_basic(ABT_SCHED_DEFAULT, 1, &pool_dest,
                                     ABT_SCHED_CONFIG_NULL, &sched_dest);
        ABT_TEST_ERROR(ret, "ABT_sched_create_basic");

        ABT_sched_config config;
        ret = ABT_sched_config_create(&config,
                                      ABT_sched_config_access, access,
                                      ABT_sched_config_var_end);
        ABT_TEST_ERROR(ret, "ABT_sched_config_create");
        ABT_sched sched;
        ret = ABT_sched_create_basic(ABT_SCHED_DEFAULT, 0, NULL, config, &sched);
        ABT_TEST_ERROR(ret, "ABT_sched_create_basic");
        ret = ABT_sched_config_free(&config);
        ABT_TEST_ERROR(ret, "ABT_sched_config_free");
        /* We need to use a task for the test to be in the same ES */
        void *args[5] = { &results[p], &pool_main, &pool_dest, &sched_dest,
                          &sched };
        ret = ABT_task_create(pool_main, task_func1, args, NULL);
        ABT_TEST_ERROR(ret, "ABT_task_create");

        ABT_TEST_ERROR(ABT_xstream_join(xstream), "ABT_xstream_join");
        ABT_TEST_ERROR(ABT_xstream_free(&xstream), "ABT_xstream_free");
    }
    return ABT_SUCCESS;
}

void task_func2(void *arg)
{
    if (arg != NULL) {
        int ret;
        int result = *(int *)arg;

        ABT_xstream xstream;
        ret = ABT_xstream_self(&xstream);
        ABT_TEST_ERROR(ret, "ABT_xstream_self");
        /* Get the pool */
        ABT_pool pool;
        ret = ABT_xstream_get_main_pools(xstream, 1, &pool);
        ABT_TEST_ERROR(ret, "ABT_xstream_get_main_pools");

        ret = ABT_task_create(pool, task_func2, NULL, NULL);
        if ((ret != ABT_SUCCESS && result == ABT_SUCCESS) ||
            (ret == ABT_SUCCESS && result != ABT_SUCCESS))
            ret =  ABT_ERR_INV_POOL_ACCESS;
        else
            ret = ABT_SUCCESS;
        ABT_TEST_ERROR(ret, "ABT_task_create");
    }
}

int push_from_another_es(int accessIdx, int *results)
{
    int ret;
    ABT_pool_access access = accesses[accessIdx];

    ABT_test_printf(1, "push_from_another_es: %d\n", accessIdx);

    /* Creation of the ES */
    ABT_sched_config config;
    ret = ABT_sched_config_create(&config,
                                  ABT_sched_config_access, access,
                                  ABT_sched_config_var_end);
    ABT_TEST_ERROR(ret, "ABT_sched_config_create");
    ABT_sched sched;
    ret = ABT_sched_create_basic(ABT_SCHED_DEFAULT, 0, NULL, config, &sched);
    ABT_TEST_ERROR(ret, "ABT_sched_create_basic");
    ret = ABT_sched_config_free(&config);
    ABT_TEST_ERROR(ret, "ABT_sched_config_free");
    ABT_xstream xstream;
    ret = ABT_xstream_create(sched, &xstream);
    ABT_TEST_ERROR(ret, "ABT_xstream_create");
    /* Get the pool */
    ABT_pool pool;
    ret = ABT_xstream_get_main_pools(xstream, 1, &pool);
    ABT_TEST_ERROR(ret, "ABT_xstream_get_main_pools");

    /* Tests */
    ret = ABT_task_create(pool, task_func2, &results[1], NULL);

    if ((ret != ABT_SUCCESS && results[0] == ABT_SUCCESS) ||
        (ret == ABT_SUCCESS && results[0] != ABT_SUCCESS))
        ret = ABT_ERR_INV_POOL_ACCESS;
    else
        ret = ABT_SUCCESS;

    ABT_TEST_ERROR(ABT_xstream_join(xstream), "ABT_xstream_join");
    ABT_TEST_ERROR(ABT_xstream_free(&xstream), "ABT_xstream_free");

    return ret;
}

int main(int argc, char *argv[])
{
    int i;
    int ret;
    int error = ABT_ERR_INV_POOL_ACCESS;
    int success = ABT_SUCCESS;
    int ret_add_to_another_ES[5];
    int *ret_add_to_another_access[5];
    int *ret_push_from_another_pool[5];

    /* Initialize */
    ABT_test_init(argc, argv);

    /* ABT_POOL_ACCESS_PRIV */
    ret_add_to_another_ES[0] = error;
    int temp00[5] = {success, success, success, error, error};
    ret_add_to_another_access[0] = temp00;
    int temp01[2] = {error, error};
    ret_push_from_another_pool[0] = temp01;

    /* ABT_POOL_ACCESS_SPSC */
    ret_add_to_another_ES[1] = error;
    int temp10[5] = {success, success, success, error, error};
    ret_add_to_another_access[1] = temp10;
    int temp11[2] = {success, error};
    ret_push_from_another_pool[1] = temp11;

    /* ABT_POOL_ACCESS_MPSC */
    ret_add_to_another_ES[2] = error;
    int temp20[5] = {success, success, success, error, error};
    ret_add_to_another_access[2] = temp20;
    int temp21[2] = {success, success};
    ret_push_from_another_pool[2] = temp21;

    /* ABT_POOL_ACCESS_SPMC */
    ret_add_to_another_ES[3] = success;
    int temp30[5] = {success, success, success, success, success};
    ret_add_to_another_access[3] = temp30;
    int temp31[2] = {success, error};
    ret_push_from_another_pool[3] = temp31;

    /* ABT_POOL_ACCESS_MPMC */
    ret_add_to_another_ES[4] = success;
    int temp40[5] = {success, success, success, success, success};
    ret_add_to_another_access[4] = temp40;
    int temp41[2] = {success, success};
    ret_push_from_another_pool[4] = temp41;

    for (i = 0; i < 5; i++) {
        ret = add_to_another_ES(i, ret_add_to_another_ES[i]);
        ABT_TEST_ERROR(ret, "add_to_another_ES");
        ret = add_to_another_access(i, ret_add_to_another_access[i]);
        ABT_TEST_ERROR(ret, "add_to_another_access");
        ret = push_from_another_es(i, ret_push_from_another_pool[i]);
        ABT_TEST_ERROR(ret, "push_from_another_es");
    }

    /* Finalize */
    ret = ABT_test_finalize(0);
    return ret;
}


