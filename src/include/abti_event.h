/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#ifndef EVENT_H_INCLUDED
#define EVENT_H_INCLUDED

#ifdef ABT_CONFIG_PUBLISH_INFO
#define ABTI_EVENT_INC_UNIT_CNT(es,t)   ABTI_event_inc_unit_cnt(es,t)
#define ABTI_EVENT_PUBLISH_INFO()       ABTI_event_publish_info()
#else
#define ABTI_EVENT_INC_UNIT_CNT(es,t)
#define ABTI_EVENT_PUBLISH_INFO()
#endif

#endif /* EVENT_H_INCLUDED */
