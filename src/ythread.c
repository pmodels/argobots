/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"

static inline ABT_bool ythread_callback_handle_request(ABTI_ythread *p_prev);

#ifdef ABT_CONFIG_ENABLE_STACK_UNWIND
#define UNW_LOCAL_ONLY
#include <libunwind.h>
struct unwind_stack_t {
    FILE *fp;
};
static void ythread_unwind_stack(void *arg);
#endif

/*****************************************************************************/
/* Private APIs                                                              */
/*****************************************************************************/

void ABTI_ythread_callback_yield(void *arg)
{
    // ABTI_event_ythread_yield(p_local_xstream, p_cur_ythread,
    //                      p_cur_ythread->thread.p_parent,
    //                      ABT_SYNC_EVENT_TYPE_USER, NULL);
    ABTI_ythread *p_prev = (ABTI_ythread *)arg;
    if (ythread_callback_handle_request(p_prev))
        return;
    /* Push this thread back to the pool. */
    ABTI_pool_add_thread(&p_prev->thread);
}

/* Before yield_to, p_prev->thread.p_pool's num_blocked must be incremented to
 * avoid making a pool empty. */
void ABTI_ythread_callback_yield_to(void *arg)
{
    // ABTI_event_ythread_yield(p_local_xstream, p_cur_ythread,
    //                      p_cur_ythread->thread.p_parent,
    //                      ABT_SYNC_EVENT_TYPE_USER, NULL);
    ABTI_ythread *p_prev = (ABTI_ythread *)arg;
    /* p_prev->thread.p_pool is loaded before ABTI_pool_add_thread() to keep
     * num_blocked consistent. Otherwise, other threads might pop p_prev
     * that has been pushed by ABTI_pool_add_thread() and change
     * p_prev->thread.p_pool by ABT_unit_set_associated_pool(). */
    ABTI_pool *p_pool = p_prev->thread.p_pool;
    if (!ythread_callback_handle_request(p_prev)) {
        /* Push this thread back to the pool. */
        ABTI_pool_add_thread(&p_prev->thread);
    }
    /* Decrease the number of blocked threads, which has been increased
     * by p_prev to avoid making a pool size 0. */
    ABTI_pool_dec_num_blocked(p_pool);
}

void ABTI_ythread_callback_suspend(void *arg)
{
    ABTI_ythread *p_prev = (ABTI_ythread *)arg;
    if (ythread_callback_handle_request(p_prev))
        return;
    /* Increase the number of blocked threads */
    ABTI_pool_inc_num_blocked(p_prev->thread.p_pool);
    /* Set this thread's state to BLOCKED. */
    ABTD_atomic_release_store_int(&p_prev->thread.state,
                                  ABT_THREAD_STATE_BLOCKED);
}

void ABTI_ythread_callback_terminate(void *arg)
{
    /* Terminate this thread. */
    ABTI_ythread *p_prev = (ABTI_ythread *)arg;
    ABTI_xstream_terminate_thread(ABTI_global_get_global(),
                                  ABTI_xstream_get_local(
                                      p_prev->thread.p_last_xstream),
                                  &p_prev->thread);
}

void ABTI_ythread_callback_suspend_unlock(void *arg)
{
    ABTI_ythread_callback_suspend_unlock_arg *p_arg =
        (ABTI_ythread_callback_suspend_unlock_arg *)arg;
    /* p_arg might point to the stack of the original ULT, so do not
     * access it after that ULT becomes resumable. */
    ABTI_ythread *p_prev = p_arg->p_prev;
    ABTD_spinlock *p_lock = p_arg->p_lock;
    if (ythread_callback_handle_request(p_prev))
        return;
    /* Increase the number of blocked threads */
    ABTI_pool_inc_num_blocked(p_prev->thread.p_pool);
    /* Set this thread's state to BLOCKED. */
    ABTD_atomic_release_store_int(&p_prev->thread.state,
                                  ABT_THREAD_STATE_BLOCKED);
    /* Release the lock. */
    ABTD_spinlock_release(p_lock);
}

void ABTI_ythread_callback_suspend_join(void *arg)
{
    ABTI_ythread_callback_suspend_join_arg *p_arg =
        (ABTI_ythread_callback_suspend_join_arg *)arg;
    /* p_arg might point to the stack of the original ULT, so do not
     * access it after that ULT becomes resumable. */
    ABTI_ythread *p_prev = p_arg->p_prev;
    ABTI_ythread *p_target = p_arg->p_target;
    if (ythread_callback_handle_request(p_prev))
        return;
    /* Increase the number of blocked threads */
    ABTI_pool_inc_num_blocked(p_prev->thread.p_pool);
    /* Set this thread's state to BLOCKED. */
    ABTD_atomic_release_store_int(&p_prev->thread.state,
                                  ABT_THREAD_STATE_BLOCKED);
    /* Set the link in the context of the target ULT. This p_link might be
     * read by p_target running on another ES in parallel, so release-store
     * is needed here. */
    ABTD_atomic_release_store_ythread_context_ptr(&p_target->ctx.p_link,
                                                  &p_prev->ctx);
}

void ABTI_ythread_callback_suspend_replace_sched(void *arg)
{
    ABTI_ythread_callback_suspend_replace_sched_arg *p_arg =
        (ABTI_ythread_callback_suspend_replace_sched_arg *)arg;
    /* p_arg might point to the stack of the original ULT, so do not
     * access it after that ULT becomes resumable. */
    ABTI_ythread *p_prev = p_arg->p_prev;
    ABTI_sched *p_main_sched = p_arg->p_main_sched;
    if (ythread_callback_handle_request(p_prev))
        return;
    /* Increase the number of blocked threads */
    ABTI_pool_inc_num_blocked(p_prev->thread.p_pool);
    /* Set this thread's state to BLOCKED. */
    ABTD_atomic_release_store_int(&p_prev->thread.state,
                                  ABT_THREAD_STATE_BLOCKED);
    /* Ask the current main scheduler to replace its scheduler */
    ABTI_sched_set_request(p_main_sched, ABTI_SCHED_REQ_REPLACE);
}

void ABTI_ythread_callback_orphan(void *arg)
{
    ABTI_ythread *p_prev = (ABTI_ythread *)arg;
    ABTI_thread_unset_associated_pool(ABTI_global_get_global(),
                                      &p_prev->thread);
}

void ABTI_ythread_set_ready(ABTI_local *p_local, ABTI_ythread *p_ythread)
{
    /* The ULT must be in BLOCKED state. */
    ABTI_ASSERT(ABTD_atomic_acquire_load_int(&p_ythread->thread.state) ==
                ABT_THREAD_STATE_BLOCKED);

    ABTI_event_ythread_resume(p_local, p_ythread,
                              ABTI_local_get_xstream_or_null(p_local)
                                  ? ABTI_local_get_xstream(p_local)->p_thread
                                  : NULL);
    /* p_ythread->thread.p_pool is loaded before ABTI_POOL_ADD_THREAD to keep
     * num_blocked consistent. Otherwise, other threads might pop p_ythread
     * that has been pushed in ABTI_POOL_ADD_THREAD and change
     * p_ythread->thread.p_pool by ABT_unit_set_associated_pool. */
    ABTI_pool *p_pool = p_ythread->thread.p_pool;

    /* Add the ULT to its associated pool */
    ABTI_pool_add_thread(&p_ythread->thread);

    /* Decrease the number of blocked threads */
    ABTI_pool_dec_num_blocked(p_pool);
}

ABTU_no_sanitize_address void ABTI_ythread_print_stack(ABTI_global *p_global,
                                                       ABTI_ythread *p_ythread,
                                                       FILE *p_os)
{
    ABTD_ythread_print_context(p_ythread, p_os, 0);
    fprintf(p_os,
            "stack     : %p\n"
            "stacksize : %" PRIu64 "\n",
            ABTD_ythread_context_get_stack(&p_ythread->ctx),
            (uint64_t)ABTD_ythread_context_get_stacksize(&p_ythread->ctx));

#ifdef ABT_CONFIG_ENABLE_STACK_UNWIND
    {
        /* Peeking a running context is specially forbidden.  Though it is
         * incomplete, let's quickly check if a thread is running. */
        ABT_thread_state state = (ABT_thread_state)ABTD_atomic_acquire_load_int(
            &p_ythread->thread.state);
        if (state == ABT_THREAD_STATE_READY ||
            state == ABT_THREAD_STATE_BLOCKED) {
            struct unwind_stack_t arg;
            arg.fp = p_os;
            ABT_bool succeeded =
                ABTI_ythread_context_peek(p_ythread, ythread_unwind_stack,
                                          &arg);
            if (!succeeded) {
                fprintf(p_os, "not executed yet.\n");
            }
        } else {
            fprintf(p_os, "failed to unwind a stack.\n");
        }
    }
#endif

    void *p_stack = ABTD_ythread_context_get_stack(&p_ythread->ctx);
    size_t i, j,
        stacksize = ABTD_ythread_context_get_stacksize(&p_ythread->ctx);
    if (stacksize == 0 || p_stack == NULL) {
        /* Some threads do not have p_stack (e.g., the main thread) */
        fprintf(p_os, "no stack\n");
        fflush(0);
        return;
    }

    if (p_global->print_raw_stack) {
        char buffer[32];
        const size_t value_width = 8;
        const int num_bytes = sizeof(buffer);

        for (i = 0; i < stacksize; i += num_bytes) {
            if (stacksize >= i + num_bytes) {
                memcpy(buffer, &((uint8_t *)p_stack)[i], num_bytes);
            } else {
                memset(buffer, 0, num_bytes);
                memcpy(buffer, &((uint8_t *)p_stack)[i], stacksize - i);
            }
            /* Print the stack address */
#if SIZEOF_VOID_P == 8
            fprintf(p_os, "%016" PRIxPTR ":",
                    (uintptr_t)(&((uint8_t *)p_stack)[i]));
#elif SIZEOF_VOID_P == 4
            fprintf(p_os, "%08" PRIxPTR ":",
                    (uintptr_t)(&((uint8_t *)p_stack)[i]));
#else
#error "unknown pointer size"
#endif
            /* Print the raw stack data */
            for (j = 0; j < num_bytes / value_width; j++) {
                if (value_width == 8) {
                    uint64_t val = ((uint64_t *)buffer)[j];
                    fprintf(p_os, " %016" PRIx64, val);
                } else if (value_width == 4) {
                    uint32_t val = ((uint32_t *)buffer)[j];
                    fprintf(p_os, " %08" PRIx32, val);
                } else if (value_width == 2) {
                    uint16_t val = ((uint16_t *)buffer)[j];
                    fprintf(p_os, " %04" PRIx16, val);
                } else {
                    uint8_t val = ((uint8_t *)buffer)[j];
                    fprintf(p_os, " %02" PRIx8, val);
                }
                if (j == (num_bytes / value_width) - 1)
                    fprintf(p_os, "\n");
            }
        }
    }
    fflush(p_os);
}

/*****************************************************************************/
/* Internal static functions                                                 */
/*****************************************************************************/

/* Return ABT_TRUE if p_prev should terminate. */
static inline ABT_bool ythread_callback_handle_request(ABTI_ythread *p_prev)
{
#if defined(ABT_CONFIG_DISABLE_THREAD_CANCEL) &&                               \
    defined(ABT_CONFIG_DISABLE_MIGRATION)
    return ABT_FALSE;
#else
    /* At least either cancellation or migration is enabled. */
    const uint32_t request =
        ABTD_atomic_acquire_load_uint32(&p_prev->thread.request);

    /* Check cancellation request. */
#ifndef ABT_CONFIG_DISABLE_THREAD_CANCEL
    if (ABTU_unlikely(request & ABTI_THREAD_REQ_CANCEL)) {
        ABTD_ythread_cancel(p_prev->thread.p_last_xstream, p_prev);
        ABTI_xstream_terminate_thread(ABTI_global_get_global(),
                                      ABTI_xstream_get_local(
                                          p_prev->thread.p_last_xstream),
                                      &p_prev->thread);
        return ABT_TRUE;
    }
#endif /* !ABT_CONFIG_DISABLE_THREAD_CANCEL */

    /* Check migration request. */
#ifndef ABT_CONFIG_DISABLE_MIGRATION
    if (ABTU_unlikely(request & ABTI_THREAD_REQ_MIGRATE)) {
        /* This is the case when the ULT requests migration of itself. */
        ABTD_atomic_release_store_int(&p_prev->thread.state,
                                      ABT_THREAD_STATE_READY);
        int abt_errno =
            ABTI_xstream_migrate_thread(ABTI_global_get_global(),
                                        ABTI_xstream_get_local(
                                            p_prev->thread.p_last_xstream),
                                        &p_prev->thread);
        if (abt_errno != ABT_SUCCESS) {
            /* Migration failed.  Let's push it back to its associated pool. */
            ABTI_pool_add_thread(&p_prev->thread);
        }
        return ABT_FALSE;
    }
#endif /* !ABT_CONFIG_DISABLE_MIGRATION */
#endif
    /* This thread does not terminate. */
    return ABT_FALSE;
}

#ifdef ABT_CONFIG_ENABLE_STACK_UNWIND
ABTU_no_sanitize_address static int ythread_unwind_stack_impl(FILE *fp)
{
    unw_cursor_t cursor;
    unw_context_t uc;
    unw_word_t ip, sp;
    int ret, level = -1;

    ret = unw_getcontext(&uc);
    if (ret != 0)
        return ABT_ERR_OTHER;

    ret = unw_init_local(&cursor, &uc);
    if (ret != 0)
        return ABT_ERR_OTHER;

    while (unw_step(&cursor) > 0 && level < 50) {
        level++;

        ret = unw_get_reg(&cursor, UNW_REG_IP, &ip);
        if (ret != 0)
            return ABT_ERR_OTHER;

        ret = unw_get_reg(&cursor, UNW_REG_SP, &sp);
        if (ret != 0)
            return ABT_ERR_OTHER;

        char proc_name[256];
        unw_word_t offset;
        ret = unw_get_proc_name(&cursor, proc_name, 256, &offset);
        if (ret != 0)
            return ABT_ERR_OTHER;

        /* Print function stack. */
        fprintf(fp, "#%d %p in %s () <+%d> (%s = %p)\n", level,
                (void *)((uintptr_t)ip), proc_name, (int)offset,
                unw_regname(UNW_REG_SP), (void *)((uintptr_t)sp));
    }
    return ABT_SUCCESS;
}

static void ythread_unwind_stack(void *arg)
{
    struct unwind_stack_t *p_arg = (struct unwind_stack_t *)arg;
    if (ythread_unwind_stack_impl(p_arg->fp) != ABT_SUCCESS) {
        fprintf(p_arg->fp, "libunwind error\n");
    }
}

#endif /* ABT_CONFIG_ENABLE_STACK_UNWIND */
