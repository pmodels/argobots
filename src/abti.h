/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#ifndef ABTI_H_INCLUDED
#define ABTI_H_INCLUDED

#include <stdio.h>
#include "abt.h"

extern __thread ABT_stream_t *g_stream;
ABT_stream_id_t ABT_stream_get_new_id();
void ABT_stream_add_thread(ABT_stream_t *stream, ABT_thread_t *thread);
void ABT_stream_del_thread(ABT_stream_t *stream, ABT_thread_t *thread);

extern __thread ABT_thread_t *g_thread;
ABT_thread_id_t ABT_thread_get_new_id();
int ABT_thread_free(ABT_stream_t *stream, ABT_thread_t *thread);

#define HANDLE_ERROR(msg)   perror(msg)

#define DEBUG
#ifdef DEBUG
#define DEBUG_PRINT(...)    fprintf(stderr, __VA_ARGS__)
#else
#define DEBUG_PRINT(...)
#endif

#endif /* ABTI_H_INCLUDED */
