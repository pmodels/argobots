/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"
#include <unistd.h>

#define ABTD_THREAD_DEFAULT_STACKSIZE   16384

void ABTD_env_init(ABTI_global *p_global)
{
    char *env;

    /* Get the number of available cores in the system */
    p_global->num_cores = sysconf(_SC_NPROCESSORS_ONLN);

    /* By default, we use the CPU affinity */
    p_global->set_affinity = ABT_TRUE;
    env = getenv("ABT_ENV_SET_AFFINITY");
    if (env != NULL) {
        if (strcmp(env, "0") == 0 || strcmp(env, "NO") == 0 ||
            strcmp(env, "no") == 0 || strcmp(env, "No") == 0) {
            p_global->set_affinity = ABT_FALSE;
        }
    }

    /* Default stack size for ULT */
    env = getenv("ABT_ENV_THREAD_STACKSIZE");
    if (env != NULL) {
        p_global->default_stacksize = (size_t)atol(env);
        assert(p_global->default_stacksize >= 512);
    } else {
        p_global->default_stacksize = ABTD_THREAD_DEFAULT_STACKSIZE;
    }
}

