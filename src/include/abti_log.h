/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#ifndef ABTI_LOG_H_INCLUDED
#define ABTI_LOG_H_INCLUDED

#include "abt_config.h"

#ifdef ABT_CONFIG_USE_DEBUG_LOG
typedef struct {
    ABTI_sched *p_sched;
} ABTI_log;

extern ABTD_XSTREAM_LOCAL ABTI_log *lp_ABTI_log;

void ABTI_log_init(void);
void ABTI_log_finalize(void);
void ABTI_log_print(FILE *fh, const char *format, ...);
void ABTI_log_event(FILE *fh, const char *format, ...);
void ABTI_log_debug(FILE *fh, char *path, int line, const char *format, ...);
void ABTI_log_pool_push(ABTI_pool *p_pool, ABT_unit unit,
                        ABTI_xstream *p_producer);
void ABTI_log_pool_remove(ABTI_pool *p_pool, ABT_unit unit,
                          ABTI_xstream *p_consumer);
void ABTI_log_pool_pop(ABTI_pool *p_pool, ABT_unit unit);

static inline void ABTI_log_set_sched(ABTI_sched *p_sched)
{
    lp_ABTI_log->p_sched = p_sched;
}

static inline ABTI_sched *ABTI_log_get_sched(void)
{
    return lp_ABTI_log->p_sched;
}

#define ABTI_LOG_INIT()             ABTI_log_init()
#define ABTI_LOG_FINALIZE()         ABTI_log_finalize()
#define ABTI_LOG_SET_SCHED(s)       ABTI_log_set_sched(s)
#define ABTI_LOG_GET_SCHED(ret)     ret = ABTI_log_get_sched()
#define LOG_EVENT(fmt,...)          ABTI_log_event(stderr,fmt,__VA_ARGS__)
#define LOG_DEBUG(fmt,...)          \
    ABTI_log_debug(stderr,__FILE__,__LINE__,fmt,__VA_ARGS__)

#define LOG_EVENT_POOL_PUSH(p_pool, unit, p_produer)    \
    ABTI_log_pool_push(p_pool, unit, p_produer)
#define LOG_EVENT_POOL_REMOVE(p_pool, unit, p_consumer) \
    ABTI_log_pool_remove(p_pool, unit, p_consumer)
#define LOG_EVENT_POOL_POP(p_pool, unit) \
    ABTI_log_pool_pop(p_pool, unit)

#else

#define ABTI_LOG_INIT()
#define ABTI_LOG_FINALIZE()
#define ABTI_LOG_SET_SCHED(s)
#define ABTI_LOG_GET_SCHED(ret)

#define LOG_EVENT(fmt,...)
#define LOG_DEBUG(fmt,...)

#define LOG_EVENT_POOL_PUSH(p_pool, unit, p_produer)
#define LOG_EVENT_POOL_REMOVE(p_pool, unit, p_consumer)
#define LOG_EVENT_POOL_POP(p_pool, unit)

#endif /* ABT_CONFIG_USE_DEBUG_LOG */

#endif /* ABTI_LOG_H_INCLUDED */
