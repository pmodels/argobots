/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"
#include <unistd.h>
#include <strings.h>

#define ABTD_KEY_TABLE_DEFAULT_SIZE     4
#define ABTD_THREAD_DEFAULT_STACKSIZE   16384
#define ABTD_SCHED_DEFAULT_STACKSIZE    (4*1024*1024)
#define ABTD_SCHED_EVENT_FREQ           50
#define ABTD_SCHED_SLEEP_NSEC           100

#define ABTD_OS_PAGE_SIZE               (4*1024)
#define ABTD_HUGE_PAGE_SIZE             (2*1024*1024)
#define ABTD_MEM_PAGE_SIZE              (2*1024*1024)
#define ABTD_MEM_STACK_PAGE_SIZE        (8*1024*1024)
#define ABTD_MEM_MAX_NUM_STACKS         65536


void ABTD_env_init(ABTI_global *p_global)
{
    char *env;

    /* Get the number of available cores in the system */
    p_global->num_cores = sysconf(_SC_NPROCESSORS_ONLN);

    /* By default, we use the CPU affinity */
    p_global->set_affinity = ABT_TRUE;
    env = getenv("ABT_SET_AFFINITY");
    if (env == NULL) env = getenv("ABT_ENV_SET_AFFINITY");
    if (env != NULL) {
        if (strcmp(env, "0") == 0 || strcasecmp(env, "n") == 0 ||
            strcasecmp(env, "no") == 0) {
            p_global->set_affinity = ABT_FALSE;
        }
    }
    if (p_global->set_affinity == ABT_TRUE) {
        ABTD_affinity_init();
    }

#ifdef ABT_CONFIG_USE_DEBUG_LOG_PRINT
    /* If the debug log printing is set in configure, logging is turned on by
     * default. */
    p_global->use_logging = ABT_TRUE;
    p_global->use_debug = ABT_TRUE;
#else
    /* Otherwise, logging is not turned on by default. */
    p_global->use_logging = ABT_FALSE;
    p_global->use_debug = ABT_FALSE;
#endif
    env = getenv("ABT_USE_LOG");
    if (env == NULL) env = getenv("ABT_ENV_USE_LOG");
    if (env != NULL) {
        if (strcmp(env, "0") == 0 || strcasecmp(env, "n") == 0 ||
            strcasecmp(env, "no") == 0) {
            p_global->use_logging = ABT_FALSE;
        } else {
            p_global->use_logging = ABT_TRUE;
        }
    }
    env = getenv("ABT_USE_DEBUG");
    if (env == NULL) env = getenv("ABT_ENV_USE_DEBUG");
    if (env != NULL) {
        if (strcmp(env, "0") == 0 || strcasecmp(env, "n") == 0 ||
            strcasecmp(env, "no") == 0) {
            p_global->use_debug = ABT_FALSE;
        } else {
            p_global->use_debug = ABT_TRUE;
        }
    }

    /* Maximum size of the internal ES array */
    env = getenv("ABT_MAX_NUM_XSTREAMS");
    if (env == NULL) env = getenv("ABT_ENV_MAX_NUM_XSTREAMS");
    if (env != NULL) {
        p_global->max_xstreams = atoi(env);
    } else {
        p_global->max_xstreams = p_global->num_cores;
    }

    /* Default key table size */
    env = getenv("ABT_KEY_TABLE_SIZE");
    if (env == NULL) env = getenv("ABT_ENV_KEY_TABLE_SIZE");
    if (env != NULL) {
        p_global->key_table_size = (int)atoi(env);
    } else {
        p_global->key_table_size = ABTD_KEY_TABLE_DEFAULT_SIZE;
    }

    /* Default stack size for ULT */
    env = getenv("ABT_THREAD_STACKSIZE");
    if (env == NULL) env = getenv("ABT_ENV_THREAD_STACKSIZE");
    if (env != NULL) {
        p_global->thread_stacksize = (size_t)atol(env);
        ABTI_ASSERT(p_global->thread_stacksize >= 512);
    } else {
        p_global->thread_stacksize = ABTD_THREAD_DEFAULT_STACKSIZE;
    }

    /* Default stack size for scheduler */
    env = getenv("ABT_SCHED_STACKSIZE");
    if (env == NULL) env = getenv("ABT_ENV_SCHED_STACKSIZE");
    if (env != NULL) {
        p_global->sched_stacksize = (size_t)atol(env);
        ABTI_ASSERT(p_global->sched_stacksize >= 512);
    } else {
        p_global->sched_stacksize = ABTD_SCHED_DEFAULT_STACKSIZE;
    }

    /* Default frequency for event checking by the scheduler */
    env = getenv("ABT_SCHED_EVENT_FREQ");
    if (env == NULL) env = getenv("ABT_ENV_SCHED_EVENT_FREQ");
    if (env != NULL) {
        p_global->sched_event_freq = (uint32_t)atol(env);
        ABTI_ASSERT(p_global->sched_event_freq >= 1);
    } else {
        p_global->sched_event_freq = ABTD_SCHED_EVENT_FREQ;
    }

    /* Default nanoseconds for scheduler sleep */
    env = getenv("ABT_SCHED_SLEEP_NSEC");
    if (env == NULL) env = getenv("ABT_ENV_SCHED_SLEEP_NSEC");
    if (env != NULL) {
        p_global->sched_sleep_nsec = atol(env);
        ABTI_ASSERT(p_global->sched_sleep_nsec >= 0);
    } else {
        p_global->sched_sleep_nsec = ABTD_SCHED_SLEEP_NSEC;
    }

    /* Mutex attributes */
    env = getenv("ABT_MUTEX_MAX_HANDOVERS");
    if (env == NULL) env = getenv("ABT_ENV_MUTEX_MAX_HANDOVERS");
    if (env != NULL) {
        p_global->mutex_max_handovers = (uint32_t)atoi(env);
        ABTI_ASSERT(p_global->mutex_max_handovers >= 1);
    } else {
        p_global->mutex_max_handovers = 64;
    }

    env = getenv("ABT_MUTEX_MAX_WAKEUPS");
    if (env == NULL) env = getenv("ABT_ENV_MUTEX_MAX_WAKEUPS");
    if (env != NULL) {
        p_global->mutex_max_wakeups = (uint32_t)atoi(env);
        ABTI_ASSERT(p_global->mutex_max_wakeups >= 1);
    } else {
        p_global->mutex_max_wakeups = 1;
    }

    /* OS page size */
    env = getenv("ABT_OS_PAGE_SIZE");
    if (env == NULL) env = getenv("ABT_ENV_OS_PAGE_SIZE");
    if (env != NULL) {
        p_global->os_page_size = (uint32_t)atol(env);
    } else {
        p_global->os_page_size = ABTD_OS_PAGE_SIZE;
    }

    /* Huge page size */
    env = getenv("ABT_HUGE_PAGE_SIZE");
    if (env == NULL) env = getenv("ABT_ENV_HUGE_PAGE_SIZE");
    if (env != NULL) {
        p_global->huge_page_size = (uint32_t)atol(env);
    } else {
        p_global->huge_page_size = ABTD_HUGE_PAGE_SIZE;
    }

#ifdef ABT_CONFIG_USE_MEM_POOL
    /* Page size for memory allocation */
    env = getenv("ABT_MEM_PAGE_SIZE");
    if (env == NULL) env = getenv("ABT_ENV_MEM_PAGE_SIZE");
    if (env != NULL) {
        p_global->mem_page_size = (uint32_t)atol(env);
    } else {
        p_global->mem_page_size = ABTD_MEM_PAGE_SIZE;
    }

    /* Stack page size for memory allocation */
    env = getenv("ABT_MEM_STACK_PAGE_SIZE");
    if (env == NULL) env = getenv("ABT_ENV_MEM_STACK_PAGE_SIZE");
    if (env != NULL) {
        p_global->mem_sp_size = (size_t)atol(env);
    } else {
        p_global->mem_sp_size = ABTD_MEM_STACK_PAGE_SIZE;
    }

    /* Maximum number of stacks that each ES can keep during execution */
    env = getenv("ABT_MEM_MAX_NUM_STACKS");
    if (env == NULL) env = getenv("ABT_ENV_MEM_MAX_NUM_STACKS");
    if (env != NULL) {
        p_global->mem_max_stacks = (uint32_t)atol(env);
    } else {
        p_global->mem_max_stacks = ABTD_MEM_MAX_NUM_STACKS;
    }

    /* How to allocate large pages.  The default is to use mmap() for huge
     * pages and then to fall back to allocate regular pages using mmap() when
     * huge pages are run out of. */
    env = getenv("ABT_MEM_LP_ALLOC");
    if (env == NULL) env = getenv("ABT_ENV_MEM_LP_ALLOC");
#if defined(HAVE_MAP_ANONYMOUS) || defined(HAVE_MAP_ANON)
    int lp_alloc = ABTI_MEM_LP_MMAP_HP_RP;
#else
    int lp_alloc = ABTI_MEM_LP_MALLOC;
#endif
    if (env != NULL) {
        if (strcasecmp(env, "malloc") == 0) {
            lp_alloc = ABTI_MEM_LP_MALLOC;
#if defined(HAVE_MAP_ANONYMOUS) || defined(HAVE_MAP_ANON)
        } else if (strcasecmp(env, "mmap_rp") == 0) {
            lp_alloc = ABTI_MEM_LP_MMAP_RP;
        } else if (strcasecmp(env, "mmap_hp_rp") == 0) {
            lp_alloc = ABTI_MEM_LP_MMAP_HP_RP;
        } else if (strcasecmp(env, "mmap_hp_thp") == 0) {
            lp_alloc = ABTI_MEM_LP_MMAP_HP_THP;
#endif
        } else if (strcasecmp(env, "thp") == 0) {
            lp_alloc = ABTI_MEM_LP_THP;
        }
    }

    /* Check if the requested allocation method is really possible. */
    if (lp_alloc != ABTI_MEM_LP_MALLOC) {
        p_global->mem_lp_alloc = ABTI_mem_check_lp_alloc(lp_alloc);
    } else {
        p_global->mem_lp_alloc = lp_alloc;
    }
#endif

#ifdef ABT_CONFIG_HANDLE_POWER_EVENT
    /* Hostname for power management daemon */
    env = getenv("ABT_POWER_EVENT_HOSTNAME");
    if (env == NULL) env = getenv("ABT_ENV_POWER_EVENT_HOSTNAME");
    p_global->pm_host = (env != NULL) ? env : "localhost";

    /* Port number for power management daemon */
    env = getenv("ABT_POWER_EVENT_PORT");
    if (env == NULL) env = getenv("ABT_ENV_POWER_EVENT_PORT");
    p_global->pm_port = (env != NULL) ? atoi(env) : 60439;
#endif

#ifdef ABT_CONFIG_PUBLISH_INFO
    /* Do we need to publish exec. information? */
    env = getenv("ABT_PUBLISH_INFO");
    if (env == NULL) env = getenv("ABT_ENV_PUBLISH_INFO");
    if (env != NULL) {
        if (strcmp(env, "0") == 0 || strcasecmp(env, "n") == 0 ||
            strcasecmp(env, "no") == 0) {
            p_global->pub_needed = ABT_FALSE;
        } else {
            p_global->pub_needed = ABT_TRUE;
        }
    } else {
        p_global->pub_needed = ABT_TRUE;
    }

    /* Filename for exec. information publishing */
    env = getenv("ABT_PUBLISH_FILENAME");
    if (env == NULL) env = getenv("ABT_ENV_PUBLISH_FILENAME");
    p_global->pub_filename = env ? env : ABT_CONFIG_DEFAULT_PUB_FILENAME;

    /* Time interval for exec. information publishing */
    env = getenv("ABT_PUBLISH_INTERVAL");
    if (env == NULL) env = getenv("ABT_ENV_PUBLISH_INTERVAL");
    p_global->pub_interval = env ? atof(env) : 1.0;
#endif

    /* Whether to print the configuration on ABT_init() */
    env = getenv("ABT_PRINT_CONFIG");
    if (env == NULL) env = getenv("ABT_ENV_PRINT_CONFIG");
    if (env != NULL) {
        if (strcmp(env, "1") == 0 || strcasecmp(env, "yes") == 0 ||
            strcasecmp(env, "y") == 0) {
            p_global->print_config = ABT_TRUE;
        } else {
            p_global->print_config = ABT_FALSE;
        }
    } else {
        p_global->print_config = ABT_FALSE;
    }

    /* Init timer */
    ABTD_time_init();
}

