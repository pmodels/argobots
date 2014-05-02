/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"

/* FIXME: is the global pointer the best way? */
__thread ABTD_Stream *g_stream = NULL;  /* Current stream */
__thread ABTD_Thread *g_thread = NULL;  /* Current running thread */

static ABTD_Stream_id ABTI_Stream_get_new_id();
static void *ABTI_Stream_loop(void *arg);
static int ABTI_Stream_schedule(ABTD_Stream *stream);
static int ABTI_Stream_free_thread(ABTD_Stream *stream, ABTD_Thread *thread);
static void ABTI_Stream_del_thread(ABTD_Stream *stream, ABTD_Thread *thread);
static void ABTI_Stream_move_thread_to_first(ABTD_Stream *stream,
                                             ABTD_Thread *thread);

/*@
ABT_Stream_create - Creates a new execution stream

Output Parameters:
. newstream - new execution stream (handle)
@*/
int ABT_Stream_create(ABT_Stream *newstream)
{
    int abt_errno = ABT_SUCCESS;
    ABTD_Stream *newstream_ptr;

    newstream_ptr = (ABTD_Stream *)ABTU_Malloc(sizeof(ABTD_Stream));
    if (!newstream_ptr) {
        HANDLE_ERROR("ABTU_Malloc");
        *newstream = NULL;
        abt_errno = ABT_ERR_MEM;
        goto fn_fail;
    }
    newstream_ptr->id = ABTI_Stream_get_new_id();
    newstream_ptr->num_threads = 0;
    newstream_ptr->first_thread = NULL;
    newstream_ptr->last_thread = NULL;
    newstream_ptr->type = ABT_STREAM_TYPE_CREATED;
    newstream_ptr->state = ABT_STREAM_STATE_READY;
    newstream_ptr->name = NULL;
    *newstream = (ABT_Stream)newstream_ptr;

  fn_exit:
    return abt_errno;

  fn_fail:
    goto fn_exit;
}

int ABT_Stream_join(ABT_Stream stream)
{
    int abt_errno = ABT_SUCCESS;
    ABTD_Stream *stream_ptr;
    void *status;

    stream_ptr = (ABTD_Stream *)stream;
    if (stream_ptr->state != ABT_STREAM_STATE_RUNNING) goto fn_exit;

    stream_ptr->state = ABT_STREAM_STATE_JOIN;
    int ret = ABTA_ES_join(stream_ptr->es, &status);
    if (ret) {
        HANDLE_ERROR("ABTA_ES_join");
        abt_errno = ABT_ERR_STREAM;
        goto fn_fail;
    }

  fn_exit:
    return abt_errno;

  fn_fail:
    goto fn_exit;
}

int ABT_Stream_free(ABT_Stream stream)
{
    int abt_errno = ABT_SUCCESS;
    ABTD_Stream *stream_ptr;

    stream_ptr = (ABTD_Stream *)stream;
    while (stream_ptr->num_threads > 0) {
        abt_errno = ABTI_Stream_free_thread(stream_ptr, stream_ptr->first_thread);
        if (abt_errno != ABT_SUCCESS) goto fn_fail;
    }
    ABTA_ES_lock_free(&stream_ptr->lock);
    if (stream_ptr->name) free(stream_ptr->name);
    free(stream_ptr);

  fn_exit:
    return abt_errno;

  fn_fail:
    goto fn_exit;
}

int ABT_Stream_equal(ABT_Stream stream1, ABT_Stream stream2)
{
    return stream1 == stream2;
}

ABT_Stream ABT_Stream_self()
{
    if (g_stream) {
        return (ABT_Stream)g_stream;
    } else {
        /* This happens when the main execution stream calls this function. */
        ABT_Stream newstream;
        int err = ABT_Stream_create(&newstream);
        if (err != ABT_SUCCESS) {
            HANDLE_ERROR("ABT_Stream_self");
        }
        ABTD_Stream *newstream_ptr = (ABTD_Stream *)newstream;
        newstream_ptr->type = ABT_STREAM_TYPE_MAIN;
        g_stream = (ABTD_Stream *)newstream;
        return newstream;
    }
}

int ABT_Stream_set_name(ABT_Stream stream, const char *name)
{
    int abt_errno = ABT_SUCCESS;
    ABTD_Stream *stream_ptr = (ABTD_Stream *)stream;

    size_t len = strlen(name);
    if (stream_ptr->name) free(stream_ptr->name);
    stream_ptr->name = (char *)ABTU_Malloc(len + 1);
    if (!stream_ptr->name) {
        HANDLE_ERROR("ABTU_Malloc");
        abt_errno = ABT_ERR_MEM;
        goto fn_fail;
    }
    strcpy(stream_ptr->name, name);

  fn_exit:
    return abt_errno;

  fn_fail:
    goto fn_exit;
}

int ABT_Stream_get_name(ABT_Stream stream, char *name, size_t len)
{
    int abt_errno = ABT_SUCCESS;
    ABTD_Stream *stream_ptr = (ABTD_Stream *)stream;

    size_t name_len = strlen(stream_ptr->name);
    if (name_len >= len) {
        strncpy(name, stream_ptr->name, len - 1);
        name[len - 1] = '\0';
    } else {
        strncpy(name, stream_ptr->name, name_len);
        name[name_len] = '\0';
    }

    return abt_errno;
}


/* Internal non-static functions */
int ABTI_Stream_start(ABTD_Stream *stream)
{
    assert(stream->state == ABT_STREAM_STATE_READY);

    int abt_errno = ABT_SUCCESS;
    if (stream->type == ABT_STREAM_TYPE_MAIN) {
        stream->es = ABTA_ES_self();
    } else {
        int ret = ABTA_ES_create(&stream->es, NULL,
                ABTI_Stream_loop, (void *)stream);
        if (ret != ABTA_ES_SUCCESS) {
            HANDLE_ERROR("ABTA_ES_create");
            abt_errno = ABT_ERR_STREAM;
            goto fn_fail;
        }
    }

    /* Initialize stream's lock variable */
    if (ABTA_ES_lock_create(&stream->lock, NULL) != ABTA_ES_SUCCESS) {
        HANDLE_ERROR("ABTA_ES_lock_create");
        abt_errno = ABT_ERR_STREAM;
        goto fn_fail;
    }

    stream->state = ABT_STREAM_STATE_RUNNING;

  fn_exit:
    return abt_errno;

  fn_fail:
    goto fn_exit;
}

/* NOTE: This function is only for the main program thread */
int ABTI_Stream_schedule_main(ABTD_Thread *thread)
{
    int abt_errno = ABT_SUCCESS;

    ABTD_Stream *stream = g_stream;
    if (stream->num_threads < 1) goto fn_exit;

    if (thread && stream->num_threads > 1) {
        ABTI_Stream_move_thread_to_first(stream, thread);
    }
    abt_errno = ABTI_Stream_schedule(stream);

 fn_exit:
    return abt_errno;
}

int ABTI_Stream_schedule_to(ABTD_Stream *stream, ABTD_Thread *thread)
{
    int abt_errno = ABT_SUCCESS;
    ABTD_Thread *cur_thread = g_thread;
    if (stream->num_threads < 2 || cur_thread == thread) goto fn_exit;

    ABTI_Stream_move_thread_to_first(stream, thread);

    /* Change the state of current running thread */
    cur_thread->state = ABT_THREAD_STATE_READY;

    /* Context-switch to thread */
    if (ABTA_ULT_swap(&cur_thread->ult, &stream->ult) != ABTA_ULT_SUCCESS) {
        HANDLE_ERROR("ABTA_ULT_swap");
        abt_errno = ABT_ERR_THREAD;
        goto fn_fail;
    }

  fn_exit:
    return abt_errno;

  fn_fail:
    goto fn_exit;
}

void ABTI_Stream_add_thread(ABTD_Stream *stream, ABTD_Thread *thread)
{
    ABTA_ES_lock(&stream->lock);
    if (stream->num_threads == 0) {
        thread->prev = thread;
        thread->next = thread;
        stream->first_thread = thread;
        stream->last_thread = thread;
    } else {
        ABTD_Thread *first_thread = stream->first_thread;
        ABTD_Thread *last_thread = stream->last_thread;
        last_thread->next = thread;
        first_thread->prev = thread;
        thread->prev = last_thread;
        thread->next = first_thread;
        stream->last_thread = thread;
    }
    stream->num_threads++;
    ABTA_ES_unlock(&stream->lock);
}


/* Internal static functions */
static ABTD_Stream_id g_stream_id = 0;
static ABTD_Stream_id ABTI_Stream_get_new_id()
{
    /* FIXME: Need to be atomic */
    return g_stream_id++;
}

static void *ABTI_Stream_loop(void *arg)
{
    ABTD_Stream *stream = (ABTD_Stream *)arg;

    /* Set this stream as the current global stream */
    g_stream = stream;

    DEBUG_PRINT("[S%lu] started\n", stream->id);

    while (stream->state == ABT_STREAM_STATE_RUNNING ||
           stream->num_threads > 0) {
        if (ABTI_Stream_schedule(stream) != ABT_SUCCESS) {
            HANDLE_ERROR("ABTI_Stream_schedule");
            goto fn_exit;
        }
        ABTA_ES_yield();
    }
    assert(stream->state == ABT_STREAM_STATE_JOIN);

  fn_exit:
    stream->state = ABT_STREAM_STATE_TERMINATED;
    DEBUG_PRINT("[S%lu] ended\n", stream->id);

    g_stream = NULL;

    ABTA_ES_exit(NULL);
    return NULL;
}

static int ABTI_Stream_schedule(ABTD_Stream *stream)
{
    int abt_errno = ABT_SUCCESS;
    while (stream->num_threads > 0) {
        ABTD_Thread *thread = stream->first_thread;
        thread->state = ABT_THREAD_STATE_RUNNING;

        /* Set the current thread of this stream as the running thread */
        g_thread = thread;

        DEBUG_PRINT("  [S%lu:T%lu] started\n", stream->id, thread->id);
        if (ABTA_ULT_swap(&stream->ult, &thread->ult) != ABTA_ULT_SUCCESS) {
            HANDLE_ERROR("ABTA_ULT_swap");
            abt_errno = ABT_ERR_THREAD;
            goto fn_fail;
        }
        DEBUG_PRINT("  [S%lu:T%lu] ended\n", stream->id, thread->id);

        if (thread->state == ABT_THREAD_STATE_TERMINATED) {
            ABTI_Stream_free_thread(stream, thread);
        }
    }
    g_thread = NULL;

  fn_exit:
    return abt_errno;

  fn_fail:
    goto fn_exit;
}

static int ABTI_Stream_free_thread(ABTD_Stream *stream, ABTD_Thread *thread)
{
    ABTI_Stream_del_thread(stream, thread);
    free(thread->stack);
    if (thread->name) free(thread->name);
    free(thread);
    return ABT_SUCCESS;
}

static void ABTI_Stream_del_thread(ABTD_Stream *stream, ABTD_Thread *thread)
{
    if (stream->num_threads == 0) return;

    ABTA_ES_lock(&stream->lock);
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
    ABTA_ES_unlock(&stream->lock);
}

static void ABTI_Stream_move_thread_to_first(ABTD_Stream *stream,
                                             ABTD_Thread *thread)
{
    ABTD_Thread *cur_thread = stream->first_thread;
    if (cur_thread == thread) return;

    stream->last_thread = cur_thread;
    stream->first_thread = thread;

    if (thread != cur_thread->next) {
        /* thread is not next to stream's first_thread */
        thread->prev->next = thread->next;
        thread->next->prev = thread->prev;
        thread->prev = cur_thread;
        thread->next = cur_thread->next;
        cur_thread->next->prev = thread;
        cur_thread->next = thread;
    }
}

