/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"

/* FIXME: is the global pointer the best way? */
__thread ABTD_Stream *g_stream = NULL;  /* Current stream */
__thread ABTD_Thread *g_thread = NULL;  /* Current running thread */

static ABT_Stream_id ABTI_Stream_get_new_id();


/*@
ABT_Stream_create - Creates a new execution stream

Output Parameters:
. newstream - new execution stream (handle)
@*/
int ABT_Stream_create(ABT_Scheduler sched, ABT_Stream *newstream)
{
    int abt_errno = ABT_SUCCESS;
    ABTD_Stream *stream_ptr;

    stream_ptr = (ABTD_Stream *)ABTU_Malloc(sizeof(ABTD_Stream));
    if (!stream_ptr) {
        HANDLE_ERROR("ABTU_Malloc");
        *newstream = NULL;
        abt_errno = ABT_ERR_MEM;
        goto fn_fail;
    }

    stream_ptr->id    = ABTI_Stream_get_new_id();
    stream_ptr->type  = ABT_STREAM_TYPE_CREATED;
    stream_ptr->name  = NULL;
    stream_ptr->state = ABT_STREAM_STATE_READY;

    /* Set the scheduler */
    if (sched == ABT_SCHEDULER_NULL) {
        /* Default scheduler */
        abt_errno = ABTI_Scheduler_create_default(&stream_ptr->sched);
        if (abt_errno != ABT_SUCCESS) {
            HANDLE_ERROR("ABTI_Scheduler_create_default");
            free(stream_ptr);
            *newstream = NULL;
            goto fn_fail;
        }
    } else {
        stream_ptr->sched = ABTI_Scheduler_get_ptr(sched);
    }

    /* Initialize the lock variable */
    if (ABTA_ES_lock_create(&stream_ptr->lock, NULL) != ABTA_ES_SUCCESS) {
        HANDLE_ERROR("ABTA_ES_lock_create");
        abt_errno = ABT_ERR_STREAM;
        goto fn_fail;
    }

    /* Create a work unit pool that contains terminated work units */
    ABTD_Pool *deads;
    if (ABTI_Pool_create(&deads) != ABT_SUCCESS) {
        HANDLE_ERROR("ABTI_Pool_create");
        abt_errno = ABT_ERR_STREAM;
        goto fn_fail;
    }
    stream_ptr->deads = ABTI_Pool_get_handle(deads);

    /* Return value */
    *newstream = ABTI_Stream_get_handle(stream_ptr);

  fn_exit:
    return abt_errno;

  fn_fail:
    goto fn_exit;
}

int ABT_Stream_set_scheduler(ABT_Stream stream, ABT_Scheduler sched)
{
    int abt_errno = ABT_SUCCESS;
    ABTD_Stream *stream_ptr = ABTI_Stream_get_ptr(stream);
    ABTD_Scheduler *sched_ptr = ABTI_Scheduler_get_ptr(sched);

    stream_ptr->sched = sched_ptr;

    return abt_errno;
}

int ABT_Stream_join(ABT_Stream stream)
{
    int abt_errno = ABT_SUCCESS;
    ABTD_Stream *stream_ptr;
    void *status;

    stream_ptr = ABTI_Stream_get_ptr(stream);
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

    stream_ptr = ABTI_Stream_get_ptr(stream);
    if (stream_ptr->name) free(stream_ptr->name);
    ABTA_ES_lock_free(&stream_ptr->lock);

    ABTD_Scheduler *sched = stream_ptr->sched;

    /* Clean up work units if there remain */
    while (sched->p_get_size(sched->pool) > 0) {
        ABT_Unit unit = sched->p_pop(sched->pool);
        ABT_Unit_type type = sched->u_get_type(unit);
        if (type == ABT_UNIT_TYPE_THREAD) {
            /* Free thd ABTD_Thread structure */
            ABT_Thread thread = sched->u_get_thread(unit);
            ABTD_Thread *thread_ptr = ABTI_Thread_get_ptr(thread);
            if (thread_ptr->name) free(thread_ptr->name);
            free(thread_ptr->stack);
            free(thread_ptr);
        } else {
            /* TODO: ABT_UNIT_TYPE_TASK */
        }

        /* Release the associated work unit */
        sched->u_free(unit);
    }

    ABT_Scheduler sched_handle = ABTI_Scheduler_get_handle(sched);
    abt_errno = ABT_Scheduler_free(sched_handle);
    if (abt_errno != ABT_SUCCESS) {
        HANDLE_ERROR("ABT_Scheduler_free");
        goto fn_fail;
    }

    if (stream_ptr->deads != ABT_POOL_NULL) {
        ABT_Pool deads = stream_ptr->deads;
        ABTD_Pool *deads_ptr = ABTI_Pool_get_ptr(deads);
        while (deads_ptr->num_units > 0) {
            ABT_Unit unit = ABTI_Pool_pop(deads);
            ABT_Thread thread = ABTI_Unit_get_thread(unit);
            assert(thread != ABT_THREAD_NULL);

            /* Free thd ABTD_Thread structure */
            ABTD_Thread *thread_ptr = ABTI_Thread_get_ptr(thread);
            if (thread_ptr->name) free(thread_ptr->name);
            free(thread_ptr->stack);
            free(thread_ptr);

            /* Release the associated work unit */
            ABTI_Unit_free(unit);
        }

        abt_errno = ABTI_Pool_free(deads);
        if (abt_errno != ABT_SUCCESS) {
            HANDLE_ERROR("ABTI_Pool_free");
            goto fn_fail;
        }
    }

    free(stream_ptr);

  fn_exit:
    return abt_errno;

  fn_fail:
    goto fn_exit;
}

int ABT_Stream_equal(ABT_Stream stream1, ABT_Stream stream2)
{
    ABTD_Stream *stream1_ptr = ABTI_Stream_get_ptr(stream1);
    ABTD_Stream *stream2_ptr = ABTI_Stream_get_ptr(stream2);
    return stream1_ptr == stream2_ptr;
}

ABT_Stream ABT_Stream_self()
{
    if (g_stream) {
        return ABTI_Stream_get_handle(g_stream);
    } else {
        /* When the main execution stream calls this function. */
        ABT_Stream newstream;
        int err = ABT_Stream_create(ABT_SCHEDULER_NULL, &newstream);
        if (err != ABT_SUCCESS) {
            HANDLE_ERROR("ABT_Stream_self");
        }
        ABTD_Stream *newstream_ptr = ABTI_Stream_get_ptr(newstream);
        newstream_ptr->type = ABT_STREAM_TYPE_MAIN;
        g_stream = newstream_ptr;
        return newstream;
    }
}

int ABT_Stream_set_name(ABT_Stream stream, const char *name)
{
    int abt_errno = ABT_SUCCESS;
    ABTD_Stream *stream_ptr = ABTI_Stream_get_ptr(stream);

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
    ABTD_Stream *stream_ptr = ABTI_Stream_get_ptr(stream);

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

    stream->state = ABT_STREAM_STATE_RUNNING;

  fn_exit:
    return abt_errno;

  fn_fail:
    goto fn_exit;
}

void *ABTI_Stream_loop(void *arg)
{
    ABTD_Stream *stream = (ABTD_Stream *)arg;

    /* Set this stream as the current global stream */
    g_stream = stream;

    DEBUG_PRINT("[S%lu] started\n", stream->id);

    ABTD_Scheduler *sched = stream->sched;
    ABT_Pool pool = sched->pool;

    while (stream->state == ABT_STREAM_STATE_RUNNING ||
           sched->p_get_size(pool) > 0) {
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

int ABTI_Stream_schedule(ABTD_Stream *stream)
{
    int abt_errno = ABT_SUCCESS;

    ABTD_Scheduler *sched = stream->sched;
    ABT_Pool pool = sched->pool;
    while (sched->p_get_size(pool) > 0) {
        ABTA_ES_lock(&stream->lock);
        ABT_Unit unit = sched->p_pop(pool);
        ABTA_ES_unlock(&stream->lock);

        ABT_Unit_type type = sched->u_get_type(unit);
        if (type == ABT_UNIT_TYPE_THREAD) {
            ABT_Thread thread = sched->u_get_thread(unit);

            /* Set the current thread of this stream as the running thread */
            g_thread = ABTI_Thread_get_ptr(thread);

            /* Change the thread state */
            g_thread->state = ABT_THREAD_STATE_RUNNING;

            /* Switch the context */
            DEBUG_PRINT("[S%lu:T%lu] started\n", stream->id, g_thread->id);
            int ret = ABTA_ULT_swap(&stream->ult, &g_thread->ult);
            if (ret != ABTA_ULT_SUCCESS) {
                HANDLE_ERROR("ABTA_ULT_swap");
                abt_errno = ABT_ERR_THREAD;
                goto fn_fail;
            }
            DEBUG_PRINT("[S%lu:T%lu] ended\n", stream->id, g_thread->id);

            if (g_thread->state == ABT_THREAD_STATE_TERMINATED) {
                if (g_thread->refcount == 0) {
                    ABT_Thread_free(ABTI_Thread_get_handle(g_thread));
                } else {
                    ABTI_Stream_keep_thread(stream, g_thread);
                }
            } else {
                /* The thread did not finish its execution.
                 * Add it to the pool again. */
                ABTA_ES_lock(&stream->lock);
                sched->p_push(pool, g_thread->unit);
                ABTA_ES_unlock(&stream->lock);
            }

            g_thread = NULL;
        } else if (type == ABT_UNIT_TYPE_TASK) {
            /* TODO */
        } else {
            HANDLE_ERROR("Not supported type!");
        }
    }

  fn_exit:
    return abt_errno;

  fn_fail:
    goto fn_exit;
}

int ABTI_Stream_keep_thread(ABTD_Stream *stream, ABTD_Thread *thread)
{
    /* FIXME: need to be improved */
    ABTD_Scheduler *sched = stream->sched;
    sched->u_free(thread->unit);

    /* Create a new unit in order to deal with it */
    ABT_Thread thread_handle = ABTI_Thread_get_handle(thread);
    thread->unit = ABTI_Unit_create_from_thread(thread_handle);

    /* Save the unit in the deads pool */
    ABTI_Pool_push(stream->deads, thread->unit);

    return ABT_SUCCESS;
}


/* Internal static functions */
static ABT_Stream_id g_stream_id = 0;
static ABT_Stream_id ABTI_Stream_get_new_id()
{
    /* FIXME: Need to be atomic */
    return g_stream_id++;
}

