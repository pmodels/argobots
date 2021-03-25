/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"
#include <unistd.h>
#include <strings.h>

#define ABTD_KEY_TABLE_DEFAULT_SIZE 4
#define ABTD_THREAD_DEFAULT_STACKSIZE 16384
#define ABTD_SCHED_DEFAULT_STACKSIZE (4 * 1024 * 1024)
#define ABTD_SCHED_EVENT_FREQ 50
#define ABTD_SCHED_SLEEP_NSEC 100

#define ABTD_HUGE_PAGE_SIZE (2 * 1024 * 1024)
#define ABTD_MEM_PAGE_SIZE (2 * 1024 * 1024)
#define ABTD_MEM_STACK_PAGE_SIZE (8 * 1024 * 1024)
#define ABTD_MEM_MAX_NUM_STACKS 1024
#define ABTD_MEM_MAX_TOTAL_STACK_SIZE (64 * 1024 * 1024)
#define ABTD_MEM_MAX_NUM_DESCS 4096

/* To avoid potential overflow, we intentionally use a smaller value than the
 * real limit. */
#define ABTD_ENV_INT_MAX ((int)(INT_MAX / 2))
#define ABTD_ENV_UINT32_MAX ((uint32_t)(UINT32_MAX / 2))
#define ABTD_ENV_UINT64_MAX ((uint64_t)(UINT64_MAX / 2))
#define ABTD_ENV_SIZE_MAX ((size_t)(SIZE_MAX / 2))

static uint32_t roundup_pow2_uint32(uint32_t val);
#ifdef ABT_CONFIG_USE_MEM_POOL
static size_t roundup_pow2_size(size_t val);
#endif
static const char *get_abt_env(const char *env_suffix);
static ABT_bool is_false(const char *str, ABT_bool include0);
static ABT_bool is_true(const char *str, ABT_bool include1);
static ABT_bool load_env_bool(const char *env_suffix, ABT_bool default_val);
static int load_env_int(const char *env_suffix, int default_val, int min_val,
                        int max_val);
static uint32_t load_env_uint32(const char *env_suffix, uint32_t default_val,
                                uint32_t min_val, uint32_t max_val);
static uint64_t load_env_uint64(const char *env_suffix, uint64_t default_val,
                                uint64_t min_val, uint64_t max_val);
static size_t load_env_size(const char *env_suffix, size_t default_val,
                            size_t min_val, size_t max_val);

void ABTD_env_init(ABTI_global *p_global)
{
    const char *env;

    /* Get the number of available cores in the system */
    p_global->num_cores = sysconf(_SC_NPROCESSORS_ONLN);

    /* ABT_SET_AFFINITY, ABT_ENV_SET_AFFINITY */
    env = get_abt_env("SET_AFFINITY");
    if (env != NULL && is_false(env, ABT_FALSE)) {
        p_global->set_affinity = ABT_FALSE;
    } else {
        /* By default, we use the CPU affinity */
        p_global->set_affinity = ABT_TRUE;
        ABTD_affinity_init(p_global, env);
    }

#ifdef ABT_CONFIG_USE_DEBUG_LOG_PRINT
    /* If the debug log printing is set in configure, logging is turned on by
     * default. */
    const ABT_bool default_use_logging = ABT_TRUE;
    const ABT_bool default_use_debug = ABT_TRUE;
#else
    /* Otherwise, logging is not turned on by default. */
    const ABT_bool default_use_logging = ABT_FALSE;
    const ABT_bool default_use_debug = ABT_FALSE;
#endif
    /* ABT_USE_LOG, ABT_ENV_USE_LOG */
    p_global->use_logging = load_env_bool("USE_LOG", default_use_logging);

    /* ABT_USE_DEBUG, ABT_ENV_USE_DEBUG */
    p_global->use_debug = load_env_bool("USE_DEBUG", default_use_debug);

    /* ABT_MAX_NUM_XSTREAMS, ABT_ENV_MAX_NUM_XSTREAMS
     * Maximum size of the internal ES array */
    p_global->max_xstreams =
        load_env_int("MAX_NUM_XSTREAMS", p_global->num_cores, 1,
                     ABTD_ENV_INT_MAX);

    /* ABT_KEY_TABLE_SIZE, ABT_ENV_KEY_TABLE_SIZE
     * Default key table size */
    p_global->key_table_size = roundup_pow2_uint32(
        load_env_uint32("KEY_TABLE_SIZE", ABTD_KEY_TABLE_DEFAULT_SIZE, 1,
                        ABTD_ENV_UINT32_MAX));

    /* ABT_THREAD_STACKSIZE, ABT_ENV_THREAD_STACKSIZE
     * Default stack size for ULT */
    p_global->thread_stacksize =
        ABTU_roundup_size(load_env_size("THREAD_STACKSIZE",
                                        ABTD_THREAD_DEFAULT_STACKSIZE, 512,
                                        ABTD_ENV_SIZE_MAX),
                          ABT_CONFIG_STATIC_CACHELINE_SIZE);

    /* ABT_SCHED_STACKSIZE, ABT_ENV_SCHED_STACKSIZE
     * Default stack size for scheduler */
    p_global->sched_stacksize =
        ABTU_roundup_size(load_env_size("SCHED_STACKSIZE",
                                        ABTD_SCHED_DEFAULT_STACKSIZE, 512,
                                        ABTD_ENV_SIZE_MAX),
                          ABT_CONFIG_STATIC_CACHELINE_SIZE);

    /* ABT_SCHED_EVENT_FREQ, ABT_ENV_SCHED_EVENT_FREQ
     * Default frequency for event checking by the scheduler */
    p_global->sched_event_freq =
        load_env_uint32("SCHED_EVENT_FREQ", ABTD_SCHED_EVENT_FREQ, 1,
                        ABTD_ENV_UINT32_MAX);

    /* ABT_SCHED_SLEEP_NSEC, ABT_ENV_SCHED_SLEEP_NSEC
     * Default nanoseconds for scheduler sleep */
    p_global->sched_sleep_nsec =
        load_env_uint64("SCHED_SLEEP_NSEC", ABTD_SCHED_SLEEP_NSEC, 0,
                        ABTD_ENV_UINT64_MAX);

    /* ABT_MUTEX_MAX_HANDOVERS, ABT_ENV_MUTEX_MAX_HANDOVERS
     * Default maximum number of mutex handover */
    p_global->mutex_max_handovers =
        load_env_uint32("MUTEX_MAX_HANDOVERS", 64, 1, ABTD_ENV_UINT32_MAX);

    /* ABT_MUTEX_MAX_WAKEUPS, ABT_ENV_MUTEX_MAX_WAKEUPS
     * Default maximum number of mutex wakeup operations */
    p_global->mutex_max_wakeups =
        load_env_uint32("MUTEX_MAX_WAKEUPS", 1, 1, ABTD_ENV_UINT32_MAX);

    /* ABT_HUGE_PAGE_SIZE, ABT_ENV_HUGE_PAGE_SIZE
     * Huge page size */
    size_t default_huge_page_size = (ABT_CONFIG_SYS_HUGE_PAGE_SIZE != 0)
                                        ? ABT_CONFIG_SYS_HUGE_PAGE_SIZE
                                        : ABTD_HUGE_PAGE_SIZE;
    p_global->huge_page_size =
        load_env_size("HUGE_PAGE_SIZE", default_huge_page_size, 4096,
                      ABTD_ENV_SIZE_MAX);

#ifdef ABT_CONFIG_USE_MEM_POOL
    /* ABT_MEM_PAGE_SIZE, ABT_ENV_MEM_PAGE_SIZE
     * Page size for memory allocation.  It must be 2^N. */
    p_global->mem_page_size = roundup_pow2_size(
        ABTU_roundup_size(load_env_size("MEM_PAGE_SIZE", ABTD_MEM_PAGE_SIZE,
                                        4096, ABTD_ENV_SIZE_MAX),
                          ABT_CONFIG_STATIC_CACHELINE_SIZE));

    /* ABT_MEM_STACK_PAGE_SIZE, ABT_ENV_MEM_STACK_PAGE_SIZE
     * Stack page size for memory allocation */
    p_global->mem_sp_size =
        ABTU_roundup_size(load_env_size("MEM_STACK_PAGE_SIZE",
                                        ABTD_MEM_STACK_PAGE_SIZE,
                                        p_global->thread_stacksize * 4,
                                        ABTD_ENV_SIZE_MAX),
                          ABT_CONFIG_STATIC_CACHELINE_SIZE);

    /* ABT_MEM_MAX_NUM_STACKS, ABT_ENV_MEM_MAX_NUM_STACKS
     * Maximum number of stacks that each ES can keep during execution. */
    /* If each execution stream caches too many stacks in total, let's reduce
     * the max # of stacks. */
    const uint32_t default_mem_max_stacks =
        ABTU_min_uint32(ABTD_MEM_MAX_TOTAL_STACK_SIZE /
                            p_global->thread_stacksize,
                        ABTD_MEM_MAX_NUM_STACKS);
    /* The value must be a multiple of ABT_MEM_POOL_MAX_LOCAL_BUCKETS. */
    p_global->mem_max_stacks =
        ABTU_roundup_uint32(load_env_uint32("MEM_MAX_NUM_STACKS",
                                            default_mem_max_stacks,
                                            ABT_MEM_POOL_MAX_LOCAL_BUCKETS,
                                            ABTD_ENV_UINT32_MAX),
                            ABT_MEM_POOL_MAX_LOCAL_BUCKETS);

    /* ABT_MEM_MAX_NUM_DESCS, ABT_ENV_MEM_MAX_NUM_DESCS
     * Maximum number of descriptors that each ES can keep during execution */
    /* The value must be a multiple of ABT_MEM_POOL_MAX_LOCAL_BUCKETS. */
    p_global->mem_max_descs =
        ABTU_roundup_uint32(load_env_uint32("MEM_MAX_NUM_DESCS",
                                            ABTD_MEM_MAX_NUM_DESCS,
                                            ABT_MEM_POOL_MAX_LOCAL_BUCKETS,
                                            ABTD_ENV_UINT32_MAX),
                            ABT_MEM_POOL_MAX_LOCAL_BUCKETS);

    /* ABT_MEM_LP_ALLOC, ABT_ENV_MEM_LP_ALLOC
     * How to allocate large pages.  The default is to use mmap() for huge
     * pages and then to fall back to allocate regular pages using mmap() when
     * huge pages are run out of. */
    int lp_alloc;
#if defined(HAVE_MAP_ANONYMOUS) || defined(HAVE_MAP_ANON)
    /*
     * To use hugepage, mmap() needs a correct size of hugepage; otherwise,
     * an error happens on "munmap()" (not mmap()).
     */
    if (get_abt_env("HUGE_PAGE_SIZE")) {
        /* If the explicitly user explicitly sets the huge page size via the
         * environmental variable, we respect that value.  It is the user's
         * responsibility to set a correct huge page size. */
        lp_alloc = ABTI_MEM_LP_MMAP_HP_RP;
    } else {
        /* Let's use huge page when both of the following conditions are met:
         * 1. Huge page is actually usable (ABT_CONFIG_USE_HUGE_PAGE_DEFAULT).
         * 2. The huge page size is not too large (e.g., some systems use 512 MB
         *    huge page, which is too big for the default setting). */
#ifdef ABT_CONFIG_USE_HUGE_PAGE_DEFAULT
        if (4096 <= ABT_CONFIG_SYS_HUGE_PAGE_SIZE &&
            ABT_CONFIG_SYS_HUGE_PAGE_SIZE <= 8 * 1024 * 1024) {
            lp_alloc = ABTI_MEM_LP_MMAP_HP_RP;
        } else {
            lp_alloc = ABTI_MEM_LP_MMAP_RP;
        }
#else
        /* Huge page allocation failed at configuration time.  Don't use it.*/
        lp_alloc = ABTI_MEM_LP_MMAP_RP;
#endif
    }
#else
    /* We cannot use mmap().  Let's use a normal malloc(). */
    lp_alloc = ABTI_MEM_LP_MALLOC;
#endif
    env = get_abt_env("MEM_LP_ALLOC");
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
        p_global->mem_lp_alloc = ABTI_mem_check_lp_alloc(p_global, lp_alloc);
    } else {
        p_global->mem_lp_alloc = lp_alloc;
    }
#endif

    /* ABT_PRINT_CONFIG, ABT_ENV_PRINT_CONFIG
     * Whether to print the configuration on ABT_init() */
    p_global->print_config = load_env_bool("PRINT_CONFIG", ABT_FALSE);

    /* Init timer */
    ABTD_time_init();
}

/*****************************************************************************/
/* Internal static functions                                                 */
/*****************************************************************************/

static uint32_t roundup_pow2_uint32(uint32_t val)
{
    /* 3 -> 4
     * 4 -> 4
     * 5 -> 8 */
    if (val == 0)
        return 0;
    uint32_t i;
    for (i = 0; i < sizeof(uint32_t) * 8 - 1; i++) {
        if ((val - 1) >> i == 0)
            break;
    }
    return ((uint32_t)1) << i;
}

#ifdef ABT_CONFIG_USE_MEM_POOL
static size_t roundup_pow2_size(size_t val)
{
    if (val == 0)
        return 0;
    size_t i;
    for (i = 0; i < sizeof(size_t) * 8 - 1; i++) {
        if ((val - 1) >> i == 0)
            break;
    }
    return ((size_t)1) << i;
}
#endif

static const char *get_abt_env(const char *env_suffix)
{
    /* Valid prefix is ABT_ and ABT_ENV_. ABT_ is prioritized. */
    char buffer[128];
    const char *prefixes[] = { "ABT_", "ABT_ENV_" };
    uint32_t i;
    for (i = 0; i < sizeof(prefixes) / sizeof(prefixes[0]); i++) {
        int prefix_size = strlen(prefixes[i]);
        int env_suffix_size = strlen(env_suffix);
        if (prefix_size + env_suffix_size + 1 <= (int)sizeof(buffer)) {
            memcpy(buffer, prefixes[i], prefix_size);
            memcpy(buffer + prefix_size, env_suffix, env_suffix_size);
            buffer[prefix_size + env_suffix_size] = '\0';
            const char *env = getenv(buffer);
            if (env)
                return env;
        }
    }
    return NULL;
}

static ABT_bool is_false(const char *str, ABT_bool include0)
{
    if (include0 && strcmp(str, "0") == 0) {
        return ABT_TRUE;
    } else if (strcasecmp(str, "n") == 0 || strcasecmp(str, "no") == 0 ||
               strcasecmp(str, "false") == 0 || strcasecmp(str, "off") == 0) {
        return ABT_TRUE;
    }
    return ABT_FALSE;
}

static ABT_bool is_true(const char *str, ABT_bool include1)
{
    if (include1 && strcmp(str, "1") == 0) {
        return ABT_TRUE;
    } else if (strcasecmp(str, "y") == 0 || strcasecmp(str, "yes") == 0 ||
               strcasecmp(str, "true") == 0 || strcasecmp(str, "on") == 0) {
        return ABT_TRUE;
    }
    return ABT_FALSE;
}

static ABT_bool load_env_bool(const char *env_suffix, ABT_bool default_val)
{
    const char *env = get_abt_env(env_suffix);
    if (!env) {
        return default_val;
    } else {
        if (default_val) {
            /* If env is not "false", return true */
            return is_false(env, ABT_TRUE) ? ABT_FALSE : ABT_TRUE;
        } else {
            /* If env is not "true", return false */
            return is_true(env, ABT_TRUE) ? ABT_TRUE : ABT_FALSE;
        }
    }
}

static int load_env_int(const char *env_suffix, int default_val, int min_val,
                        int max_val)
{
    const char *env = get_abt_env(env_suffix);
    if (!env) {
        return ABTU_max_int(min_val, ABTU_min_int(max_val, default_val));
    } else {
        int val;
        int abt_errno = ABTU_atoi(env, &val, NULL);
        if (abt_errno != ABT_SUCCESS) {
            return ABTU_max_int(min_val, ABTU_min_int(max_val, default_val));
        } else {
            return ABTU_max_int(min_val, ABTU_min_int(max_val, val));
        }
    }
}

static uint32_t load_env_uint32(const char *env_suffix, uint32_t default_val,
                                uint32_t min_val, uint32_t max_val)
{
    const char *env = get_abt_env(env_suffix);
    if (!env) {
        return ABTU_max_uint32(min_val, ABTU_min_uint32(max_val, default_val));
    } else {
        uint32_t val;
        int abt_errno = ABTU_atoui32(env, &val, NULL);
        if (abt_errno != ABT_SUCCESS) {
            return ABTU_max_uint32(min_val,
                                   ABTU_min_uint32(max_val, default_val));
        } else {
            return ABTU_max_uint32(min_val, ABTU_min_uint32(max_val, val));
        }
    }
}

static uint64_t load_env_uint64(const char *env_suffix, uint64_t default_val,
                                uint64_t min_val, uint64_t max_val)
{
    const char *env = get_abt_env(env_suffix);
    if (!env) {
        return ABTU_max_uint64(min_val, ABTU_min_uint64(max_val, default_val));
    } else {
        uint64_t val;
        int abt_errno = ABTU_atoui64(env, &val, NULL);
        if (abt_errno != ABT_SUCCESS) {
            return ABTU_max_uint64(min_val,
                                   ABTU_min_uint64(max_val, default_val));
        } else {
            return ABTU_max_uint64(min_val, ABTU_min_uint64(max_val, val));
        }
    }
}

static size_t load_env_size(const char *env_suffix, size_t default_val,
                            size_t min_val, size_t max_val)
{
    const char *env = get_abt_env(env_suffix);
    if (!env) {
        return ABTU_max_size(min_val, ABTU_min_size(max_val, default_val));
    } else {
        size_t val;
        int abt_errno = ABTU_atosz(env, &val, NULL);
        if (abt_errno != ABT_SUCCESS) {
            return ABTU_max_size(min_val, ABTU_min_size(max_val, default_val));
        } else {
            return ABTU_max_size(min_val, ABTU_min_size(max_val, val));
        }
    }
}
