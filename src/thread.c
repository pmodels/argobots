/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>

#include "abt.h"
#include "abti.h"

static void thread_func_wrapper(void (*thread_func)(void *), void *arg);

ABT_thread_t *ABT_thread_create(ABT_stream_t *stream,
                              void (*thread_func)(void *), void *arg,
                              size_t stacksize,
                              int *err)
{
    int ret = ABT_SUCCESS;
    ABT_thread_t *thread = (ABT_thread_t *)malloc(sizeof(ABT_thread_t));
    thread->id = ABT_thread_get_new_id();
    thread->state = ABT_STATE_READY;
    thread->stack = malloc(stacksize);
    thread->stacksize = stacksize;
    if (getcontext(&thread->ctx) == -1) {
        HANDLE_ERROR("getcontext");
        ret = ABT_FAILURE;
    }
    thread->ctx.uc_link = &stream->ctx;
    thread->ctx.uc_stack.ss_sp = thread->stack;
    thread->ctx.uc_stack.ss_size = stacksize;
    makecontext(&thread->ctx, (void (*)())thread_func_wrapper,
                2, thread_func, arg);

    ABT_stream_add_thread(stream, thread);

    if (err) *err = ret;
    return thread;
}

int ABT_thread_free(ABT_stream_t *stream, ABT_thread_t *thread)
{
    ABT_stream_del_thread(stream, thread);
    free(thread);
    return ABT_SUCCESS;
}

int ABT_thread_yield()
{
    // Move this thread to the end of scheduling queue
    g_stream->last_thread = g_thread;
    g_stream->first_thread = g_thread->next;

    g_thread->state = ABT_STATE_READY;
    if (swapcontext(&g_thread->ctx, &g_stream->ctx) == -1) {
        HANDLE_ERROR("swapcontext");
        return ABT_FAILURE;
    }
    return ABT_SUCCESS;
}

int ABT_thread_yield_to(ABT_thread_t *thread)
{
    // Move this thread to the end of scheduling queue
    if (thread != g_thread) {
        g_stream->last_thread = g_thread;
        g_stream->first_thread = thread;
        thread->prev->next = thread->next;
        thread->next->prev = thread->prev;
        thread->prev = g_thread;
        thread->next = g_thread->next;
        g_thread->next->prev = thread;
        g_thread->next = thread;
    }

    g_thread->state = ABT_STATE_READY;
    if (swapcontext(&g_thread->ctx, &g_stream->ctx) == -1) {
        HANDLE_ERROR("swapcontext");
        return ABT_FAILURE;
    }
    return ABT_SUCCESS;
}

/* For internal use */
static ABT_thread_id_t gs_ABT_thread_id = 0;
ABT_thread_id_t ABT_thread_get_new_id() {
    /* FIXME */
    return gs_ABT_thread_id++;
}

static void thread_func_wrapper(void (*thread_func)(void *), void *arg)
{
    thread_func(arg);

    // Now, the thread have finished its job.
    // Change the thread state.
    g_thread->state = ABT_STATE_TERMINATED;
}

