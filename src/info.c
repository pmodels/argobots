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
