/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdarg.h>
#include "abti.h"

#ifdef ABT_CONFIG_USE_DEBUG_LOG

void ABTI_log_debug(FILE *fh, const char *format, ...)
{
    if (gp_ABTI_global->use_logging == ABT_FALSE)
        return;
    ABTI_xstream *p_local_xstream = ABTI_local_get_xstream_uninlined();

    ABTI_thread *p_thread = NULL;
    ABTI_task *p_task = NULL;
    char *prefix_fmt = NULL, *prefix = NULL;
    char *newfmt;
    uint64_t tid;
    int rank;
    int tid_len = 0, rank_len = 0;
    size_t newfmt_len;

    if (!p_local_xstream) {
        prefix = "<UNKNOWN> ";
        prefix_fmt = "%s%s";
    } else {
        ABTI_unit_type type = ABTI_self_get_type(p_local_xstream);
        if (ABTI_unit_type_is_thread(type)) {
            p_thread = p_local_xstream->p_thread;
            if (p_thread == NULL) {
                if (p_local_xstream->type != ABTI_XSTREAM_TYPE_PRIMARY) {
                    prefix_fmt = "<U%" PRIu64 ":E%d> %s";
                    rank = p_local_xstream->rank;
                    tid = 0;
                } else {
                    prefix = "<U0:E0> ";
                    prefix_fmt = "%s%s";
                }
            } else {
                rank = p_local_xstream->rank;
#ifndef ABT_CONFIG_DISABLE_STACKABLE_SCHED
                if (p_thread->p_sched) {
                    prefix_fmt = "<S%" PRIu64 ":E%d> %s";
                    tid = p_thread->p_sched->id;
                } else
#endif
                {
                    prefix_fmt = "<U%" PRIu64 ":E%d> %s";
                    tid = ABTI_thread_get_id(p_thread);
                }
            }
        } else if (type == ABTI_UNIT_TYPE_TASK) {
            rank = p_local_xstream->rank;
            p_task = p_local_xstream->p_task;
            prefix_fmt = "<T%" PRIu64 ":E%d> %s";
            tid = p_task ? ABTI_task_get_id(p_task) : 0;
        } else {
            prefix = "<EXT> ";
            prefix_fmt = "%s%s";
        }
    }

    if (prefix == NULL) {
        tid_len = ABTU_get_int_len(tid);
        rank_len = ABTU_get_int_len(rank);
        newfmt_len = 6 + tid_len + rank_len + strlen(format);
        newfmt = (char *)ABTU_malloc(newfmt_len + 1);
        sprintf(newfmt, prefix_fmt, tid, rank, format);
    } else {
        newfmt_len = strlen(prefix) + strlen(format);
        newfmt = (char *)ABTU_malloc(newfmt_len + 1);
        sprintf(newfmt, prefix_fmt, prefix, format);
    }

    va_list list;
    va_start(list, format);
    vfprintf(fh, newfmt, list);
    va_end(list);
    fflush(fh);

    ABTU_free(newfmt);
}

void ABTI_log_pool_push(ABTI_pool *p_pool, ABT_unit unit,
                        ABTI_native_thread_id producer_id)
{
    if (gp_ABTI_global->use_logging == ABT_FALSE)
        return;

    ABTI_thread *p_thread = NULL;
    ABTI_task *p_task = NULL;
    switch (p_pool->u_get_type(unit)) {
        case ABT_UNIT_TYPE_THREAD:
            p_thread = ABTI_thread_get_ptr(p_pool->u_get_thread(unit));
            if (p_thread->p_last_xstream) {
                LOG_DEBUG("[U%" PRIu64 ":E%d] pushed to P%" PRIu64 " "
                          "(producer: NT %p)\n",
                          ABTI_thread_get_id(p_thread),
                          p_thread->p_last_xstream->rank, p_pool->id,
                          (void *)producer_id);
            } else {
                LOG_DEBUG("[U%" PRIu64 "] pushed to P%" PRIu64 " "
                          "(producer: NT %p)\n",
                          ABTI_thread_get_id(p_thread), p_pool->id,
                          (void *)producer_id);
            }
            break;

        case ABT_UNIT_TYPE_TASK:
            p_task = ABTI_task_get_ptr(p_pool->u_get_task(unit));
            if (p_task->p_xstream) {
                LOG_DEBUG("[T%" PRIu64 ":E%d] pushed to P%" PRIu64 " "
                          "(producer: NT %p)\n",
                          ABTI_task_get_id(p_task), p_task->p_xstream->rank,
                          p_pool->id, (void *)producer_id);
            } else {
                LOG_DEBUG("[T%" PRIu64 "] pushed to P%" PRIu64 " "
                          "(producer: NT %p)\n",
                          ABTI_task_get_id(p_task), p_pool->id,
                          (void *)producer_id);
            }
            break;

        default:
            ABTI_ASSERT(0);
            break;
    }
}

void ABTI_log_pool_remove(ABTI_pool *p_pool, ABT_unit unit,
                          ABTI_native_thread_id consumer_id)
{
    if (gp_ABTI_global->use_logging == ABT_FALSE)
        return;

    ABTI_thread *p_thread = NULL;
    ABTI_task *p_task = NULL;
    switch (p_pool->u_get_type(unit)) {
        case ABT_UNIT_TYPE_THREAD:
            p_thread = ABTI_thread_get_ptr(p_pool->u_get_thread(unit));
            if (p_thread->p_last_xstream) {
                LOG_DEBUG("[U%" PRIu64 ":E%d] removed from "
                          "P%" PRIu64 " (consumer: NT %p)\n",
                          ABTI_thread_get_id(p_thread),
                          p_thread->p_last_xstream->rank, p_pool->id,
                          (void *)consumer_id);
            } else {
                LOG_DEBUG("[U%" PRIu64 "] removed from P%" PRIu64 " "
                          "(consumer: NT %p)\n",
                          ABTI_thread_get_id(p_thread), p_pool->id,
                          (void *)consumer_id);
            }
            break;

        case ABT_UNIT_TYPE_TASK:
            p_task = ABTI_task_get_ptr(p_pool->u_get_task(unit));
            if (p_task->p_xstream) {
                LOG_DEBUG("[T%" PRIu64 ":E%d] removed from "
                          "P%" PRIu64 " (consumer: NT %p)\n",
                          ABTI_task_get_id(p_task), p_task->p_xstream->rank,
                          p_pool->id, (void *)consumer_id);
            } else {
                LOG_DEBUG("[T%" PRIu64 "] removed from P%" PRIu64 " "
                          "(consumer: NT %p)\n",
                          ABTI_task_get_id(p_task), p_pool->id,
                          (void *)consumer_id);
            }
            break;

        default:
            ABTI_ASSERT(0);
            break;
    }
}

void ABTI_log_pool_pop(ABTI_pool *p_pool, ABT_unit unit)
{
    if (gp_ABTI_global->use_logging == ABT_FALSE)
        return;
    if (unit == ABT_UNIT_NULL)
        return;

    ABTI_thread *p_thread = NULL;
    ABTI_task *p_task = NULL;
    switch (p_pool->u_get_type(unit)) {
        case ABT_UNIT_TYPE_THREAD:
            p_thread = ABTI_thread_get_ptr(p_pool->u_get_thread(unit));
            if (p_thread->p_last_xstream) {
                LOG_DEBUG("[U%" PRIu64 ":E%d] popped from "
                          "P%" PRIu64 "\n",
                          ABTI_thread_get_id(p_thread),
                          p_thread->p_last_xstream->rank, p_pool->id);
            } else {
                LOG_DEBUG("[U%" PRIu64 "] popped from P%" PRIu64 "\n",
                          ABTI_thread_get_id(p_thread), p_pool->id);
            }
            break;

        case ABT_UNIT_TYPE_TASK:
            p_task = ABTI_task_get_ptr(p_pool->u_get_task(unit));
            if (p_task->p_xstream) {
                LOG_DEBUG("[T%" PRIu64 ":E%d] popped from "
                          "P%" PRIu64 "\n",
                          ABTI_task_get_id(p_task), p_task->p_xstream->rank,
                          p_pool->id);
            } else {
                LOG_DEBUG("[T%" PRIu64 "] popped from P%" PRIu64 "\n",
                          ABTI_task_get_id(p_task), p_pool->id);
            }
            break;

        default:
            ABTI_ASSERT(0);
            break;
    }
}

#endif /* ABT_CONFIG_USE_DEBUG_LOG */
