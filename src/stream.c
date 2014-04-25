/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>

#include "abt.h"
#include "abti.h"

static void *ABT_stream_schedule(void *arg);

ABT_stream_t *ABT_stream_create(int *err)
{
    ABT_stream_t *stream = (ABT_stream_t *)malloc(sizeof(ABT_stream_t));
    stream->id = ABT_stream_get_new_id();
    stream->num_threads = 0;
    stream->first_thread = NULL;
    stream->last_thread = NULL;
    if (err) *err = ABT_SUCCESS;
    return stream;
}

int ABT_stream_start(ABT_stream_t *stream)
{
    int ret = pthread_create(&stream->es, NULL, ABT_stream_schedule, (void *)stream);
    if (ret) {
        HANDLE_ERROR("ABT_stream_create");
    }

    return ABT_SUCCESS;
}

int ABT_stream_join(ABT_stream_t *stream)
{
    void *status;
    int ret = pthread_join(stream->es, &status);
    if (ret) {
        HANDLE_ERROR("ABT_stream_join");
    }
    return ABT_SUCCESS;
}

int ABT_stream_free(ABT_stream_t *stream)
{
    while (stream->num_threads > 0) {
        ABT_thread_free(stream, stream->first_thread);
    }
    free(stream);
    return ABT_SUCCESS;
}

int ABT_stream_cancel(ABT_stream_t *stream)
{
    /* TODO */
    return ABT_SUCCESS;
}

int ABT_stream_exit()
{
    /* TODO */
    return ABT_SUCCESS;
}


/* For internal use */
/* FIXME: is the global pointer the best way? */
__thread ABT_stream_t *g_stream = NULL;
__thread ABT_thread_t *g_thread = NULL;

static void *ABT_stream_schedule(void *arg)
{
    ABT_stream_t *stream = (ABT_stream_t *)arg;

    // Set this stream as the current global stream
    g_stream = stream;

    DEBUG_PRINT("[S%lu] started\n", stream->id);
    while (stream->num_threads > 0) {
        ABT_thread_t *thread = stream->first_thread;
        thread->state = ABT_STATE_RUNNING;

        // Set the current thread for this stream as the running thread
        g_thread = thread;

        DEBUG_PRINT("  [S%lu:T%lu] started\n", stream->id, thread->id);
        swapcontext(&stream->ctx, &thread->ctx);
        DEBUG_PRINT("  [S%lu:T%lu] ended\n", stream->id, thread->id);

        if (thread->state == ABT_STATE_TERMINATED) {
            ABT_thread_free(stream, thread);
        }
    }
    g_thread = NULL;
    g_stream = NULL;
    DEBUG_PRINT("[S%lu] ended\n", stream->id);

    pthread_exit(NULL);
    return NULL;
}

static ABT_stream_id_t g_ABT_stream_id = 0;
ABT_stream_id_t ABT_stream_get_new_id()
{
    /* FIXME */
    return g_ABT_stream_id++;
}

void ABT_stream_add_thread(ABT_stream_t *stream, ABT_thread_t *thread)
{
    if (stream->num_threads == 0) {
        thread->prev = thread;
        thread->next = thread;
        stream->first_thread = thread;
        stream->last_thread = thread;
    } else {
        ABT_thread_t *first_thread = stream->first_thread;
        ABT_thread_t *last_thread = stream->last_thread;
        last_thread->next = thread;
        first_thread->prev = thread;
        thread->prev = last_thread;
        thread->next = first_thread;
        stream->last_thread = thread;
    }
    stream->num_threads++;
}

void ABT_stream_del_thread(ABT_stream_t *stream, ABT_thread_t *thread)
{
    if (stream->num_threads == 0) return;
    if (stream->num_threads == 1) {
        thread->prev = NULL;
        thread->next = NULL;
        stream->first_thread = NULL;
        stream->last_thread = NULL;
    } else {
        thread->prev->next = thread->next;
        thread->next->prev = thread->prev;
        if (stream->first_thread == thread)
            stream->first_thread = thread->next;
        if (stream->last_thread == thread)
            stream->last_thread = thread->prev;
    }
    stream->num_threads--;
}

