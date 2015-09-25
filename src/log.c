/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdarg.h>
#include "abti.h"

#ifdef ABT_CONFIG_USE_DEBUG_LOG
ABTD_XSTREAM_LOCAL ABTI_log *lp_ABTI_log = NULL;

void ABTI_log_init(void)
{
    lp_ABTI_log = (ABTI_log *)ABTU_malloc(sizeof(ABTI_log));
    lp_ABTI_log->p_sched = NULL;
}

void ABTI_log_finalize(void)
{
    ABTU_free(lp_ABTI_log);
    lp_ABTI_log = NULL;
}

void ABTI_log_print(FILE *fh, const char *format, ...)
{
    if (gp_ABTI_global->use_logging == ABT_FALSE) return;

    va_list list;
    va_start(list, format);
    vfprintf(fh, format, list);
    va_end(list);
    fflush(fh);
}

void ABTI_log_event(FILE *fh, const char *format, ...)
{
    if (gp_ABTI_global->use_logging == ABT_FALSE) return;

    ABT_unit_type type;
    ABTI_xstream *p_xstream = NULL;
    ABTI_thread *p_thread = NULL;
    ABTI_task *p_task = NULL;
    char *prefix_fmt = NULL, *prefix = NULL;
    char *newfmt;
    size_t tid, rank;
    int tid_len = 0, rank_len = 0;
    size_t newfmt_len;

    ABT_self_get_type(&type);
    switch (type) {
        case ABT_UNIT_TYPE_THREAD:
            p_xstream = ABTI_local_get_xstream();
            p_thread = ABTI_local_get_thread();
            if (p_thread == NULL) {
                if (p_xstream && p_xstream->type != ABTI_XSTREAM_TYPE_PRIMARY) {
                    prefix_fmt = "<U%" PRIu64 ":E%" PRIu64 "> %s";
                    rank = p_xstream->rank;
                    tid = 0;
                } else {
                    prefix = "<U0:E0> ";
                    prefix_fmt = "%s%s";
                }
            } else {
                rank = p_xstream->rank;
                if (lp_ABTI_log->p_sched) {
                    prefix_fmt = "<S%" PRIu64 ":E%" PRIu64 "> %s";
                    tid = lp_ABTI_log->p_sched->id;
                } else {
                    prefix_fmt = "<U%" PRIu64 ":E%" PRIu64 "> %s";
                    tid = ABTI_thread_get_id(p_thread);
                }
            }
            break;

        case ABT_UNIT_TYPE_TASK:
            p_xstream = ABTI_local_get_xstream();
            rank = p_xstream->rank;
            p_task = ABTI_local_get_task();
            if (lp_ABTI_log->p_sched) {
                prefix_fmt = "<S%" PRIu64 ":E%" PRIu64 "> %s";
                tid = lp_ABTI_log->p_sched->id;
            } else {
                prefix_fmt = "<T%" PRIu64 ":E%" PRIu64 "> %s";
                tid = ABTI_task_get_id(p_task);
            }
            break;

        case ABT_UNIT_TYPE_EXT:
            prefix = "<EXT> ";
            prefix_fmt = "%s%s";
            break;

        default:
            prefix = "<UNKNOWN> ";
            prefix_fmt = "%s%s";
            break;
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

void ABTI_log_debug(FILE *fh, char *path, int line, const char *format, ...)
{
    if (gp_ABTI_global->use_debug == ABT_FALSE) return;

    int line_len;
    size_t newfmt_len;
    char *newfmt;

    line_len = ABTU_get_int_len(line);
    newfmt_len = strlen(path) + line_len + 4 + strlen(format);
    newfmt = (char *)ABTU_malloc(newfmt_len + 1);
    sprintf(newfmt, "[%s:%d] %s", path, line, format);

    va_list list;
    va_start(list, format);
    vfprintf(fh, newfmt, list);
    va_end(list);
    fflush(fh);

    ABTU_free(newfmt);
}

void ABTI_log_pool_push(ABTI_pool *p_pool, ABT_unit unit,
                        ABTI_xstream *p_producer)
{
    if (gp_ABTI_global->use_logging == ABT_FALSE) return;

    ABTI_thread *p_thread = NULL;
    ABTI_task *p_task = NULL;
    switch (p_pool->u_get_type(unit)) {
        case ABT_UNIT_TYPE_THREAD:
            p_thread = ABTI_thread_get_ptr(p_pool->u_get_thread(unit));
            if (p_thread->p_last_xstream) {
                LOG_EVENT("[U%" PRIu64 ":E%" PRIu64 "] pushed to P%" PRIu64 " "
                          "(producer: E%" PRIu64 ")\n",
                          ABTI_thread_get_id(p_thread),
                          p_thread->p_last_xstream->rank,
                          p_pool->id,
                          p_producer->rank);
            } else {
                LOG_EVENT("[U%" PRIu64 "] pushed to P%" PRIu64 " "
                          "(producer: E%" PRIu64 ")\n",
                          ABTI_thread_get_id(p_thread),
                          p_pool->id,
                          p_producer->rank);
            }
            break;

        case ABT_UNIT_TYPE_TASK:
            p_task = ABTI_task_get_ptr(p_pool->u_get_task(unit));
            if (p_task->p_xstream) {
                LOG_EVENT("[T%" PRIu64 ":E%" PRIu64 "] pushed to P%" PRIu64 " "
                          "(producer: E%" PRIu64 ")\n",
                          ABTI_task_get_id(p_task),
                          p_task->p_xstream->rank,
                          p_pool->id,
                          p_producer->rank);
            } else {
                LOG_EVENT("[T%" PRIu64 "] pushed to P%" PRIu64 " "
                          "(producer: E%" PRIu64 ")\n",
                          ABTI_task_get_id(p_task),
                          p_pool->id,
                          p_producer->rank);
            }
            break;

        default:
            ABTI_ASSERT(0);
            break;
    }
}

void ABTI_log_pool_remove(ABTI_pool *p_pool, ABT_unit unit,
                          ABTI_xstream *p_consumer)
{
    if (gp_ABTI_global->use_logging == ABT_FALSE) return;

    ABTI_thread *p_thread = NULL;
    ABTI_task *p_task = NULL;
    switch (p_pool->u_get_type(unit)) {
        case ABT_UNIT_TYPE_THREAD:
            p_thread = ABTI_thread_get_ptr(p_pool->u_get_thread(unit));
            if (p_thread->p_last_xstream) {
                LOG_EVENT("[U%" PRIu64 ":E%" PRIu64 "] removed from "
                          "P%" PRIu64 " (consumer: E%" PRIu64 ")\n",
                          ABTI_thread_get_id(p_thread),
                          p_thread->p_last_xstream->rank,
                          p_pool->id,
                          p_consumer->rank);
            } else {
                LOG_EVENT("[U%" PRIu64 "] removed from P%" PRIu64 " "
                          "(consumer: E%" PRIu64 ")\n",
                          ABTI_thread_get_id(p_thread),
                          p_pool->id,
                          p_consumer->rank);
            }
            break;

        case ABT_UNIT_TYPE_TASK:
            p_task = ABTI_task_get_ptr(p_pool->u_get_task(unit));
            if (p_task->p_xstream) {
                LOG_EVENT("[T%" PRIu64 ":E%" PRIu64 "] removed from "
                          "P%" PRIu64 " (consumer: E%" PRIu64 ")\n",
                          ABTI_task_get_id(p_task),
                          p_task->p_xstream->rank,
                          p_pool->id,
                          p_consumer->rank);
            } else {
                LOG_EVENT("[T%" PRIu64 "] removed from P%" PRIu64 " "
                          "(consumer: E%" PRIu64 ")\n",
                          ABTI_task_get_id(p_task),
                          p_pool->id,
                          p_consumer->rank);
            }
            break;

        default:
            ABTI_ASSERT(0);
            break;
    }
}

void ABTI_log_pool_pop(ABTI_pool *p_pool, ABT_unit unit)
{
    if (gp_ABTI_global->use_logging == ABT_FALSE) return;
    if (unit == ABT_UNIT_NULL) return;

    ABTI_thread *p_thread = NULL;
    ABTI_task *p_task = NULL;
    switch (p_pool->u_get_type(unit)) {
        case ABT_UNIT_TYPE_THREAD:
            p_thread = ABTI_thread_get_ptr(p_pool->u_get_thread(unit));
            if (p_thread->p_last_xstream) {
                LOG_EVENT("[U%" PRIu64 ":E%" PRIu64 "] popped from "
                          "P%" PRIu64 "\n",
                          ABTI_thread_get_id(p_thread),
                          p_thread->p_last_xstream->rank,
                          p_pool->id);
            } else {
                LOG_EVENT("[U%" PRIu64 "] popped from P%" PRIu64 "\n",
                          ABTI_thread_get_id(p_thread),
                          p_pool->id);
            }
            break;

        case ABT_UNIT_TYPE_TASK:
            p_task = ABTI_task_get_ptr(p_pool->u_get_task(unit));
            if (p_task->p_xstream) {
                LOG_EVENT("[T%" PRIu64 ":E%" PRIu64 "] popped from "
                          "P%" PRIu64 "\n",
                          ABTI_task_get_id(p_task),
                          p_task->p_xstream->rank,
                          p_pool->id);
            } else {
                LOG_EVENT("[T%" PRIu64 "] popped from P%" PRIu64 "\n",
                          ABTI_task_get_id(p_task),
                          p_pool->id);
            }
            break;

        default:
            ABTI_ASSERT(0);
            break;
    }
}

#endif /* ABT_CONFIG_USE_DEBUG_LOG */
