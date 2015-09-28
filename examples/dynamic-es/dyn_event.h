/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#ifndef DYN_EVENT_H
#define DYN_EVENT_H

void rt1_init(int max_xstreams, ABT_xstream *xstreams);
void rt1_finalize(void);
void rt1_launcher(void *arg);

#endif /* DYN_EVENT_H */

