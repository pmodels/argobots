/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"

/** @defgroup INFO  Information
 * This group is for getting diverse runtime information of Argobots.  The
 * routines in this group are meant for debugging and diagnosing Argobots.
 */

/**
 * @ingroup INFO
 * @brief   Write the configuration information to the output stream.
 *
 * \c ABT_info_print_config() writes the configuration information to the given
 * output stream \c fp.
 *
 * @param[in] fp  output stream
 * @return Error code
 * @retval ABT_SUCCESS            on success
 * @retval ABT_ERR_UNINITIALIZED  Argobots has not been initialized
 */
int ABT_info_print_config(FILE *fp)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_CHECK_INITIALIZED();

    ABTI_global *p_global = gp_ABTI_global;

    fprintf(fp, "Argobots Configuration:\n");
    fprintf(fp, " - # of cores: %d\n", p_global->num_cores);
    fprintf(fp, " - cache line size: %u\n", ABT_CONFIG_STATIC_CACHELINE_SIZE);
    fprintf(fp, " - OS page size: %u\n", p_global->os_page_size);
    fprintf(fp, " - huge page size: %u\n", p_global->huge_page_size);
    fprintf(fp, " - max. # of ESs: %d\n", p_global->max_xstreams);
    fprintf(fp, " - cur. # of ESs: %d\n", p_global->num_xstreams);
    fprintf(fp, " - ES affinity: %s\n",
                (p_global->set_affinity == ABT_TRUE) ? "on" : "off");
    fprintf(fp, " - logging: %s\n",
                (p_global->use_logging == ABT_TRUE) ? "on" : "off");
    fprintf(fp, " - debug output: %s\n",
                (p_global->use_debug == ABT_TRUE) ? "on" : "off");
    fprintf(fp, " - key table entries: %d\n", p_global->key_table_size);
    fprintf(fp, " - ULT stack size: %u KB\n",
                (unsigned)(p_global->thread_stacksize / 1024));
    fprintf(fp, " - scheduler stack size: %u KB\n",
                (unsigned)(p_global->sched_stacksize / 1024));
    fprintf(fp, " - scheduler event check frequency: %u\n",
                p_global->sched_event_freq);

    fprintf(fp, " - timer function: "
#if defined(ABT_CONFIG_USE_CLOCK_GETTIME)
                "clock_gettime"
#elif defined(ABT_CONFIG_USE_MACH_ABSOLUTE_TIME)
                "mach_absolute_time"
#elif defined(ABT_CONFIG_USE_GETTIMEOFDAY)
                "gettimeofday"
#endif
                "\n");

#ifdef ABT_CONFIG_USE_MEM_POOL
    fprintf(fp, "Memory Pool:\n");
    fprintf(fp, " - page size for allocation: %u KB\n",
                p_global->mem_page_size / 1024);
    fprintf(fp, " - stack page size: %u KB\n", p_global->mem_sp_size / 1024);
    fprintf(fp, " - max. # of stacks per ES: %u\n", p_global->mem_max_stacks);
    switch (p_global->mem_lp_alloc) {
        case ABTI_MEM_LP_MALLOC:
            fprintf(fp, " - large page allocation: malloc\n");
            break;
        case ABTI_MEM_LP_MMAP_RP:
            fprintf(fp, " - large page allocation: mmap regular pages\n");
            break;
        case ABTI_MEM_LP_MMAP_HP_RP:
            fprintf(fp, " - large page allocation: mmap huge pages + "
                        "regular pages\n");
            break;
        case ABTI_MEM_LP_MMAP_HP_THP:
            fprintf(fp, " - large page allocation: mmap huge pages + THPs\n");
            break;
        case ABTI_MEM_LP_THP:
            fprintf(fp, " - large page allocation: THPs\n");
            break;
    }
#endif /* ABT_CONFIG_USE_MEM_POOL */

#if defined(ABT_CONFIG_HANDLE_POWER_EVENT) || defined(ABT_CONFIG_PUBLISH_INFO)
    fprintf(fp, "Event:\n");

#ifdef ABT_CONFIG_HANDLE_POWER_EVENT
    fprintf(fp, " - pm daemon connected: %s\n",
                (p_global->pm_connected == ABT_TRUE) ? "yes" : "no");
    fprintf(fp, " - pm hostname: %s\n", p_global->pm_host);
    fprintf(fp, " - pm port: %d\n", p_global->pm_port);
#endif /* ABT_CONFIG_HANDLE_POWER_EVENT */

#ifdef ABT_CONFIG_PUBLISH_INFO
    fprintf(fp, " - publishing needed: %s\n",
                (p_global->pub_needed == ABT_TRUE) ? "yes" : "no");
    fprintf(fp, " - publishing filename: %s\n", p_global->pub_filename);
    fprintf(fp, " - publishing interval: %lf sec.\n", p_global->pub_interval);
#endif /* ABT_CONFIG_PUBLISH_INFO */
#endif

    fflush(fp);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}


/**
 * @ingroup INFO
 * @brief   Write the information of all created ESs to the output stream.
 *
 * \c ABT_info_print_all_xstreams() writes the information of all ESs to the
 * given output stream \c fp.
 *
 * @param[in] fp  output stream
 * @return Error code
 * @retval ABT_SUCCESS            on success
 * @retval ABT_ERR_UNINITIALIZED  Argobots has not been initialized
 */
int ABT_info_print_all_xstreams(FILE *fp)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_CHECK_INITIALIZED();

    ABTI_global *p_global = gp_ABTI_global;
    int i;

    ABTI_spinlock_acquire(&p_global->xstreams_lock);

    fprintf(fp, "# of created ESs: %d\n", p_global->num_xstreams);
    for (i = 0; i < p_global->num_xstreams; i++) {
        ABTI_xstream *p_xstream = p_global->p_xstreams[i];
        if (p_xstream) {
            ABTI_xstream_print(p_xstream, fp, 0, ABT_FALSE);
        }
    }

    ABTI_spinlock_release(&p_global->xstreams_lock);

    fflush(fp);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}


/**
 * @ingroup INFO
 * @brief   Write the information of the target ES to the output stream.
 *
 * \c ABT_info_print_xstream() writes the information of the target ES
 * \c xstream to the given output stream \c fp.
 *
 * @param[in] fp       output stream
 * @param[in] xstream  handle to the target ES
 * @return Error code
 * @retval ABT_SUCCESS  on success
 */
int ABT_info_print_xstream(FILE *fp, ABT_xstream xstream)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_xstream *p_xstream = ABTI_xstream_get_ptr(xstream);
    ABTI_CHECK_NULL_XSTREAM_PTR(p_xstream);

    ABTI_xstream_print(p_xstream, fp, 0, ABT_FALSE);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}


/**
 * @ingroup INFO
 * @brief   Write the information of the target scheduler to the output stream.
 *
 * \c ABT_info_print_sched() writes the information of the target scheduler
 * \c sched to the given output stream \c fp.
 *
 * @param[in] fp     output stream
 * @param[in] sched  handle to the target scheduler
 * @return Error code
 * @retval ABT_SUCCESS  on success
 */
int ABT_info_print_sched(FILE *fp, ABT_sched sched)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_sched *p_sched = ABTI_sched_get_ptr(sched);
    ABTI_CHECK_NULL_SCHED_PTR(p_sched);

    ABTI_sched_print(p_sched, fp, 0, ABT_FALSE);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}


/**
 * @ingroup INFO
 * @brief   Write the information of the target pool to the output stream.
 *
 * \c ABT_info_print_pool() writes the information of the target pool
 * \c pool to the given output stream \c fp.
 *
 * @param[in] fp    output stream
 * @param[in] pool  handle to the target pool
 * @return Error code
 * @retval ABT_SUCCESS  on success
 */
int ABT_info_print_pool(FILE* fp, ABT_pool pool)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_pool *p_pool = ABTI_pool_get_ptr(pool);
    ABTI_CHECK_NULL_POOL_PTR(p_pool);

    ABTI_pool_print(p_pool, fp, 0);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}


/**
 * @ingroup INFO
 * @brief   Write the information of the target ULT to the output stream.
 *
 * \c ABT_info_print_thread() writes the information of the target ULT
 * \c thread to the given output stream \c fp.
 *
 * @param[in] fp      output stream
 * @param[in] thread  handle to the target ULT
 * @return Error code
 * @retval ABT_SUCCESS  on success
 */
int ABT_info_print_thread(FILE* fp, ABT_thread thread)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_thread *p_thread = ABTI_thread_get_ptr(thread);
    ABTI_CHECK_NULL_THREAD_PTR(p_thread);

    ABTI_thread_print(p_thread, fp, 0);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}


/**
 * @ingroup INFO
 * @brief   Write the information of the target ULT attribute to the output
 * stream.
 *
 * \c ABT_info_print_thread_attr() writes the information of the target ULT
 * attribute \c attr to the given output stream \c fp.
 *
 * @param[in] fp    output stream
 * @param[in] attr  handle to the target ULT attribute
 * @return Error code
 * @retval ABT_SUCCESS  on success
 */
int ABT_info_print_thread_attr(FILE* fp, ABT_thread_attr attr)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_thread_attr *p_attr = ABTI_thread_attr_get_ptr(attr);
    ABTI_CHECK_NULL_THREAD_ATTR_PTR(p_attr);

    ABTI_thread_attr_print(p_attr, fp, 0);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}


/**
 * @ingroup INFO
 * @brief   Write the information of the target tasklet to the output stream.
 *
 * \c ABT_info_print_task() writes the information of the target tasklet
 * \c task to the given output stream \c fp.
 *
 * @param[in] fp    output stream
 * @param[in] task  handle to the target tasklet
 * @return Error code
 * @retval ABT_SUCCESS  on success
 */
int ABT_info_print_task(FILE* fp, ABT_task task)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_task *p_task = ABTI_task_get_ptr(task);
    ABTI_CHECK_NULL_TASK_PTR(p_task);

    ABTI_task_print(p_task, fp, 0);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

/**
 * @ingroup INFO
 * @brief   Dump the stack of the target thread to the output stream.
 *
 * \c ABT_info_print_thread_stack() dumps the call stack of \c thread
 * to the given output stream \c fp.
 *
 * @param[in] fp      output stream
 * @param[in] thread  handle to the target thread
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_info_print_thread_stack(FILE *fp, ABT_thread thread)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_thread *p_thread = ABTI_thread_get_ptr(thread);
    ABTI_CHECK_NULL_THREAD_PTR(p_thread);

    abt_errno = ABTI_thread_print_stack(p_thread, fp);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

struct ABTI_info_print_unit_arg_t {
    FILE *fp;
    ABT_pool pool;
};

static void ABTI_info_print_unit(void *arg, ABT_unit unit)
{
    /* This function may not have any side effect on unit because it is passed
     * to p_print_all. */
    struct ABTI_info_print_unit_arg_t *p_arg;
    p_arg = (struct ABTI_info_print_unit_arg_t *)arg;
    FILE *fp = p_arg->fp;
    ABT_pool pool = p_arg->pool;
    ABTI_pool *p_pool = ABTI_pool_get_ptr(pool);
    ABT_unit_type type = p_pool->u_get_type(unit);

    if (type == ABT_UNIT_TYPE_THREAD) {
        fprintf(fp, "=== ULT (%p) ===\n", (void *)unit);
        ABT_thread thread = p_pool->u_get_thread(unit);
        ABTI_thread *p_thread = ABTI_thread_get_ptr(thread);
        ABT_thread_id thread_id = ABTI_thread_get_id(p_thread);
        fprintf(fp, "id        : %" PRIu64 "\n"
                    "ctx       : %p\n",
                    (uint64_t)thread_id,
                    &p_thread->ctx);
        ABTD_thread_print_context(p_thread, fp, 2);
        fprintf(fp, "stack     : %p\n"
                    "stacksize : %" PRIu64 "\n",
                    p_thread->attr.p_stack,
                    (uint64_t)p_thread->attr.stacksize);
        int abt_errno = ABT_info_print_thread_stack(fp, thread);
        if (abt_errno != ABT_SUCCESS)
            fprintf(fp, "Failed to print stack.\n");
    } else if (type == ABT_UNIT_TYPE_TASK) {
        fprintf(fp, "=== tasklet (%p) ===\n", (void *)unit);
    } else {
        fprintf(fp, "=== unknown (%p) ===\n", (void *)unit);
    }
}

/**
 * @ingroup INFO
 * @brief   Dump stack information of all the threads in the target pool.
 *
 * \c ABT_info_print_thread_stacks_in_pool() dumps call stacks of all threads
 * stored in \c pool.  This function returns \c ABT_ERR_POOL if \c pool does not
 * support \c p_print_all.
 *
 * @param[in] fp    output stream
 * @param[in] pool  handle to the target pool
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_info_print_thread_stacks_in_pool(FILE *fp, ABT_pool pool)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_pool *p_pool = ABTI_pool_get_ptr(pool);
    ABTI_CHECK_NULL_POOL_PTR(p_pool);

    if (!p_pool->p_print_all) {
        abt_errno = ABT_ERR_POOL;
        goto fn_fail;
    }
    fprintf(fp, "== pool (%p) ==\n", p_pool);
    struct ABTI_info_print_unit_arg_t arg;
    arg.fp = fp;
    arg.pool = pool;
    p_pool->p_print_all(pool, &arg, ABTI_info_print_unit);

  fn_exit:
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
}

struct ABTI_info_pool_set_t {
    ABT_pool *pools;
    size_t num;
    size_t len;
};

static inline
void ABTI_info_initialize_pool_set(struct ABTI_info_pool_set_t *p_set)
{
    size_t default_len = 16;
    p_set->pools = (ABT_pool *)ABTU_malloc(sizeof(ABT_pool) * default_len);
    p_set->num = 0;
    p_set->len = default_len;
}

static inline
void ABTI_info_finalize_pool_set(struct ABTI_info_pool_set_t *p_set)
{
    ABTU_free(p_set->pools);
}

static inline
void ABTI_info_add_pool_set(ABT_pool pool, struct ABTI_info_pool_set_t *p_set)
{
    size_t i;
    for (i = 0; i < p_set->num; i++) {
        if (p_set->pools[i] == pool)
            return;
    }
    /* Add pool to p_set. */
    if (p_set->num == p_set->len) {
        size_t new_len = p_set->len * 2;
        p_set->pools = (ABT_pool *)ABTU_realloc(p_set->pools,
                                                sizeof(ABT_pool) * new_len);
        p_set->len = new_len;
    }
    p_set->pools[p_set->num++] = pool;
}

#define PRINT_STACK_FLAG_UNSET      0
#define PRINT_STACK_FLAG_INITIALIZE 1
#define PRINT_STACK_FLAG_WAIT       2
#define PRINT_STACK_FLAG_FINALIZE   3

static uint32_t print_stack_flag = PRINT_STACK_FLAG_UNSET;
static FILE *print_stack_fp = NULL;
static double print_stack_timeout = 0.0;
static void (*print_cb_func)(ABT_bool, void *) = NULL;
static void *print_arg = NULL;
static uint32_t print_stack_barrier = 0;

/**
 * @ingroup INFO
 * @brief   Dump stacks of threads in pools existing in Argobots.
 *
 * \c ABT_info_trigger_print_all_thread_stacks() tries to dump call stacks of
 * all threads stored in pools in the Argobots runtime. This function itself does
 * not print stacks; it immediately returns after updating a flag. Stacks are
 * printed when all execution streams stop in \c ABT_xstream_check_events().
 *
 * If some execution streams do not stop within a certain time period, one of
 * the stopped execution streams starts to print stack information. In this
 * case, this function might not work correctly and at worst causes a crash.
 * This function does not work at all if no execution stream executes
 * \c ABT_xstream_check_events().
 *
 * \c cb_func is called after completing stack dump unless it is NULL. The first
 * argument is set to \c ABT_TRUE if not all the execution streams stop within
 * \c timeout. Otherwise, \c ABT_FALSE is set. The second argument is user-defined
 * data \c arg. Since \c cb_func is not called by a thread or an execution
 * stream, \c ABT_self_...() functions in \c cb_func return undefined values.
 * Neither signal-safety nor thread-safety is required for \c cb_func.
 *
 * In Argobots, \c ABT_info_trigger_print_all_thread_stacks is exceptionally
 * signal-safe; it can be safely called in a signal handler.
 *
 * The following threads are not captured in this function:
 * - threads that are suspending (e.g., by \c ABT_thread_suspend())
 * - threads in pools that are not associated with main schedulers
 *
 * @param[in] fp       output stream
 * @param[in] timeout  timeout (second). Disabled if the value is negative.
 * @param[in] cb_func  call-back function
 * @param[in] arg      an argument passed to \c cb_func
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_info_trigger_print_all_thread_stacks(FILE *fp, double timeout,
                                             void (*cb_func)(ABT_bool, void *),
                                             void *arg)
{
    /* This function is signal-safe, so it may not call other functions unless
     * you really know what the called functions do. */
    if (ABTD_atomic_load_uint32(&print_stack_flag) == PRINT_STACK_FLAG_UNSET) {
        if (ABTD_atomic_bool_cas_strong_uint32(&print_stack_flag,
                                               PRINT_STACK_FLAG_UNSET,
                                               PRINT_STACK_FLAG_INITIALIZE)) {
            /* Save fp and timeout. */
            print_stack_fp = fp;
            print_stack_timeout = timeout;
            print_cb_func = cb_func;
            print_arg = arg;
            /* Here print_stack_barrier must be 0. */
            ABTI_ASSERT(ABTD_atomic_load_uint32(&print_stack_barrier) == 0);
            ABTD_atomic_store_uint32(&print_stack_flag, PRINT_STACK_FLAG_WAIT);
        }
    }
    return ABT_SUCCESS;
}

void ABTI_info_check_print_all_thread_stacks(void)
{
    if (ABTD_atomic_load_uint32(&print_stack_flag) != PRINT_STACK_FLAG_WAIT)
        return;

    /* Wait for the other execution streams using a barrier mechanism. */
    uint32_t self_value = ABTD_atomic_fetch_add_uint32(&print_stack_barrier, 1);
    if (self_value == 0) {
        /* This ES becomes a master. */
        double start_time = ABT_get_wtime();
        ABT_bool force_print = ABT_FALSE;
        while (ABTD_atomic_load_uint32(&print_stack_barrier)
               < ABTD_atomic_load_int32(&gp_ABTI_global->num_xstreams)) {
            ABTD_atomic_pause();
            if (print_stack_timeout >= 0.0
                && (ABT_get_wtime() - start_time) >= print_stack_timeout) {
                force_print = ABT_TRUE;
                break;
            }
        }
        /* All the available ESs are (supposed to be) stopped. We *assume* that
         * no ES is calling and will call Argobots functions except this
         * function while printing stack information. */
        int i, j;
        struct ABTI_info_pool_set_t pool_set;
        ABTI_info_initialize_pool_set(&pool_set);
        FILE *fp = print_stack_fp;
        if (force_print) {
            fprintf(fp, "ABT_info_trigger_print_all_thread_stacks: "
                        "timeout (only %d ESs stop)\n",
                        (int)print_stack_barrier);
        }
        for (i = 0; i < gp_ABTI_global->num_xstreams; i++) {
            ABTI_xstream *p_xstream = gp_ABTI_global->p_xstreams[i];
            ABTI_sched *p_main_sched = p_xstream->p_main_sched;
            fprintf(fp, "= xstream[%d] (%p) =\n", i, p_xstream);
            fprintf(fp, "main_sched : %p\n", p_main_sched);
            if (!p_main_sched)
                continue;
            for (j = 0; j < p_main_sched->num_pools; j++) {
                ABT_pool pool = p_main_sched->pools[j];
                ABTI_ASSERT(pool != ABT_POOL_NULL);
                fprintf(fp, "  pools[%d] : %p\n", j, pool);
                ABTI_info_add_pool_set(pool, &pool_set);
            }
        }
        for (i = 0; i < pool_set.num; i++) {
            ABT_pool pool = pool_set.pools[i];
            int abt_errno = ABT_info_print_thread_stacks_in_pool(fp, pool);
            if (abt_errno != ABT_SUCCESS)
                fprintf(fp, "  Failed to print (errno = %d).\n", abt_errno);
        }
        if (print_cb_func)
            print_cb_func(force_print, print_arg);
        /* Update print_stack_flag to 3. */
        ABTD_atomic_store_uint32(&print_stack_flag, PRINT_STACK_FLAG_FINALIZE);
    } else {
        /* Wait for the master's work. */
        while (ABTD_atomic_load_uint32(&print_stack_flag)
               != PRINT_STACK_FLAG_FINALIZE)
            ABTD_atomic_pause();
    }
    ABTI_ASSERT(ABTD_atomic_load_uint32(&print_stack_flag)
                == PRINT_STACK_FLAG_FINALIZE);

    /* Decrement the barrier value. */
    uint32_t dec_value = ABTD_atomic_fetch_sub_uint32(&print_stack_barrier, 1);
    if (dec_value == 0) {
        /* The last execution stream resets the flag. */
        ABTD_atomic_store_uint32(&print_stack_flag, PRINT_STACK_FLAG_UNSET);
    }
}
