/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"

/* FIXME: is the global pointer the best way? */
__thread ABTI_Stream *gp_stream = NULL;  /* Current stream */
__thread ABTI_Thread *gp_thread = NULL;  /* Current running thread */

static ABT_Stream_id ABTI_Stream_get_new_id();


/*@
ABT_Stream_create - Creates a new execution stream

Output Parameters:
. newstream - new execution stream (handle)
@*/
int ABT_Stream_create(ABT_Scheduler sched, ABT_Stream *newstream)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_Stream *p_stream;

    p_stream = (ABTI_Stream *)ABTU_Malloc(sizeof(ABTI_Stream));
    if (!p_stream) {
        HANDLE_ERROR("ABTU_Malloc");
        *newstream = NULL;
        abt_errno = ABT_ERR_MEM;
        goto fn_fail;
    }

    p_stream->id     = ABTI_Stream_get_new_id();
    p_stream->type   = ABTI_STREAM_TYPE_CREATED;
    p_stream->p_name = NULL;
    p_stream->state  = ABT_STREAM_STATE_READY;

    /* Set the scheduler */
    if (sched == ABT_SCHEDULER_NULL) {
        /* Default scheduler */
        abt_errno = ABTI_Scheduler_create_default(&p_stream->p_sched);
        if (abt_errno != ABT_SUCCESS) {
            HANDLE_ERROR("ABTI_Scheduler_create_default");
            free(p_stream);
            *newstream = NULL;
            goto fn_fail;
        }
    } else {
        p_stream->p_sched = ABTI_Scheduler_get_ptr(sched);
    }

    /* Create a work unit pool that contains terminated work units */
    ABTI_Pool *p_deads;
    if (ABTI_Pool_create(&p_deads) != ABT_SUCCESS) {
        HANDLE_ERROR("ABTI_Pool_create");
        abt_errno = ABT_ERR_STREAM;
        goto fn_fail;
    }
    p_stream->deads = ABTI_Pool_get_handle(p_deads);

    /* Initialize the lock variable */
    if (ABTD_ES_lock_create(&p_stream->lock, NULL) != ABTD_ES_SUCCESS) {
        HANDLE_ERROR("ABTD_ES_lock_create");
        abt_errno = ABT_ERR_STREAM;
        goto fn_fail;
    }

    /* Return value */
    *newstream = ABTI_Stream_get_handle(p_stream);

  fn_exit:
    return abt_errno;

  fn_fail:
    goto fn_exit;
}

int ABT_Stream_set_scheduler(ABT_Stream stream, ABT_Scheduler sched)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_Stream *p_stream = ABTI_Stream_get_ptr(stream);
    ABTI_Scheduler *p_sched = ABTI_Scheduler_get_ptr(sched);

    p_stream->p_sched = p_sched;

    return abt_errno;
}

int ABT_Stream_join(ABT_Stream stream)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_Stream *p_stream;
    void *p_status;

    p_stream = ABTI_Stream_get_ptr(stream);
    if (p_stream->state != ABT_STREAM_STATE_RUNNING) goto fn_exit;

    p_stream->state = ABT_STREAM_STATE_JOIN;
    int ret = ABTD_ES_join(p_stream->es, &p_status);
    if (ret) {
        HANDLE_ERROR("ABTD_ES_join");
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
    ABTI_Stream *p_stream;

    p_stream = ABTI_Stream_get_ptr(stream);
    if (p_stream->p_name) free(p_stream->p_name);
    ABTD_ES_lock_free(&p_stream->lock);

    ABTI_Scheduler *p_sched = p_stream->p_sched;

    /* Clean up work units if there remain */
    ABT_Pool pool = p_sched->pool;
    while (p_sched->p_get_size(pool) > 0) {
        ABT_Unit unit = p_sched->p_pop(pool);
        ABT_Unit_type type = p_sched->u_get_type(unit);
        if (type == ABT_UNIT_TYPE_THREAD) {
            /* Free thd ABTI_Thread structure */
            ABT_Thread thread = p_sched->u_get_thread(unit);
            ABTI_Thread *p_thread = ABTI_Thread_get_ptr(thread);
            if (p_thread->p_name) free(p_thread->p_name);
            free(p_thread->p_stack);
            free(p_thread);
        } else {
            /* TODO: ABT_UNIT_TYPE_TASK */
        }

        /* Release the associated work unit */
        p_sched->u_free(unit);
    }

    ABT_Scheduler sched = ABTI_Scheduler_get_handle(p_sched);
    abt_errno = ABT_Scheduler_free(sched);
    if (abt_errno != ABT_SUCCESS) {
        HANDLE_ERROR("ABT_Scheduler_free");
        goto fn_fail;
    }

    if (p_stream->deads != ABT_POOL_NULL) {
        ABT_Pool deads = p_stream->deads;
        ABTI_Pool *p_deads = ABTI_Pool_get_ptr(deads);
        while (p_deads->num_units > 0) {
            ABT_Unit unit = ABTI_Pool_pop(deads);
            ABT_Thread thread = ABTI_Unit_get_thread(unit);
            assert(thread != ABT_THREAD_NULL);

            /* Free thd ABTI_Thread structure */
            ABTI_Thread *p_thread = ABTI_Thread_get_ptr(thread);
            if (p_thread->p_name) free(p_thread->p_name);
            free(p_thread->p_stack);
            free(p_thread);

            /* Release the associated work unit */
            ABTI_Unit_free(unit);
        }

        abt_errno = ABTI_Pool_free(deads);
        if (abt_errno != ABT_SUCCESS) {
            HANDLE_ERROR("ABTI_Pool_free");
            goto fn_fail;
        }
    }

    free(p_stream);

  fn_exit:
    return abt_errno;

  fn_fail:
    goto fn_exit;
}

int ABT_Stream_equal(ABT_Stream stream1, ABT_Stream stream2)
{
    ABTI_Stream *p_stream1 = ABTI_Stream_get_ptr(stream1);
    ABTI_Stream *p_stream2 = ABTI_Stream_get_ptr(stream2);
    return p_stream1 == p_stream2;
}

ABT_Stream ABT_Stream_self()
{
    if (gp_stream) {
        return ABTI_Stream_get_handle(gp_stream);
    } else {
        /* When the main execution stream calls this function. */
        ABT_Stream newstream;
        int err = ABT_Stream_create(ABT_SCHEDULER_NULL, &newstream);
        if (err != ABT_SUCCESS) {
            HANDLE_ERROR("ABT_Stream_self");
        }
        ABTI_Stream *p_newstream = ABTI_Stream_get_ptr(newstream);
        p_newstream->type = ABTI_STREAM_TYPE_MAIN;
        gp_stream = p_newstream;
        return newstream;
    }
}

int ABT_Stream_set_name(ABT_Stream stream, const char *name)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_Stream *p_stream = ABTI_Stream_get_ptr(stream);

    size_t len = strlen(name);
    if (p_stream->p_name) free(p_stream->p_name);
    p_stream->p_name = (char *)ABTU_Malloc(len + 1);
    if (!p_stream->p_name) {
        HANDLE_ERROR("ABTU_Malloc");
        abt_errno = ABT_ERR_MEM;
        goto fn_fail;
    }
    ABTU_Strcpy(p_stream->p_name, name);

  fn_exit:
    return abt_errno;

  fn_fail:
    goto fn_exit;
}

int ABT_Stream_get_name(ABT_Stream stream, char *name, size_t len)
{
    int abt_errno = ABT_SUCCESS;
    ABTI_Stream *p_stream = ABTI_Stream_get_ptr(stream);

    size_t name_len = strlen(p_stream->p_name);
    if (name_len >= len) {
        ABTU_Strncpy(name, p_stream->p_name, len - 1);
        name[len - 1] = '\0';
    } else {
        ABTU_Strncpy(name, p_stream->p_name, name_len);
        name[name_len] = '\0';
    }

    return abt_errno;
}


/* Internal non-static functions */
int ABTI_Stream_start(ABTI_Stream *p_stream)
{
    assert(p_stream->state == ABT_STREAM_STATE_READY);

    int abt_errno = ABT_SUCCESS;
    if (p_stream->type == ABTI_STREAM_TYPE_MAIN) {
        p_stream->es = ABTD_ES_self();
    } else {
        int ret = ABTD_ES_create(&p_stream->es, NULL,
                                 ABTI_Stream_loop, (void *)p_stream);
        if (ret != ABTD_ES_SUCCESS) {
            HANDLE_ERROR("ABTD_ES_create");
            abt_errno = ABT_ERR_STREAM;
            goto fn_fail;
        }
    }

    p_stream->state = ABT_STREAM_STATE_RUNNING;

  fn_exit:
    return abt_errno;

  fn_fail:
    goto fn_exit;
}

void *ABTI_Stream_loop(void *p_arg)
{
    ABTI_Stream *p_stream = (ABTI_Stream *)p_arg;

    /* Set this stream as the current global stream */
    gp_stream = p_stream;

    DEBUG_PRINT("[S%lu] started\n", p_stream->id);

    ABTI_Scheduler *p_sched = p_stream->p_sched;
    ABT_Pool pool = p_sched->pool;

    while (p_stream->state == ABT_STREAM_STATE_RUNNING ||
           p_sched->p_get_size(pool) > 0) {
        if (ABTI_Stream_schedule(p_stream) != ABT_SUCCESS) {
            HANDLE_ERROR("ABTI_Stream_schedule");
            goto fn_exit;
        }
        ABTD_ES_yield();
    }
    assert(p_stream->state == ABT_STREAM_STATE_JOIN);

  fn_exit:
    p_stream->state = ABT_STREAM_STATE_TERMINATED;
    DEBUG_PRINT("[S%lu] ended\n", p_stream->id);

    gp_stream = NULL;

    ABTD_ES_exit(NULL);
    return NULL;
}

int ABTI_Stream_schedule(ABTI_Stream *p_stream)
{
    int abt_errno = ABT_SUCCESS;

    ABTI_Scheduler *p_sched = p_stream->p_sched;
    ABT_Pool pool = p_sched->pool;
    while (p_sched->p_get_size(pool) > 0) {
        ABTD_ES_lock(&p_stream->lock);
        ABT_Unit unit = p_sched->p_pop(pool);
        ABTD_ES_unlock(&p_stream->lock);

        ABT_Unit_type type = p_sched->u_get_type(unit);
        if (type == ABT_UNIT_TYPE_THREAD) {
            ABT_Thread thread = p_sched->u_get_thread(unit);

            /* Set the current thread of this stream as the running thread */
            gp_thread = ABTI_Thread_get_ptr(thread);

            /* Change the thread state */
            gp_thread->state = ABT_THREAD_STATE_RUNNING;

            /* Switch the context */
            DEBUG_PRINT("[S%lu:T%lu] started\n", p_stream->id, gp_thread->id);
            int ret = ABTD_ULT_swap(&p_stream->ult, &gp_thread->ult);
            if (ret != ABTD_ULT_SUCCESS) {
                HANDLE_ERROR("ABTD_ULT_swap");
                abt_errno = ABT_ERR_THREAD;
                goto fn_fail;
            }
            DEBUG_PRINT("[S%lu:T%lu] ended\n", p_stream->id, gp_thread->id);

            if (gp_thread->state == ABT_THREAD_STATE_TERMINATED) {
                if (gp_thread->refcount == 0) {
                    ABT_Thread_free(ABTI_Thread_get_handle(gp_thread));
                } else {
                    ABTI_Stream_keep_thread(p_stream, gp_thread);
                }
            } else {
                /* The thread did not finish its execution.
                 * Add it to the pool again. */
                ABTD_ES_lock(&p_stream->lock);
                p_sched->p_push(pool, gp_thread->unit);
                ABTD_ES_unlock(&p_stream->lock);
            }

            gp_thread = NULL;
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

int ABTI_Stream_keep_thread(ABTI_Stream *p_stream, ABTI_Thread *p_thread)
{
    /* FIXME: need to be improved */
    ABTI_Scheduler *p_sched = p_stream->p_sched;
    p_sched->u_free(p_thread->unit);

    /* Create a new unit in order to deal with it */
    ABT_Thread thread = ABTI_Thread_get_handle(p_thread);
    p_thread->unit = ABTI_Unit_create_from_thread(thread);

    /* Save the unit in the deads pool */
    ABTI_Pool_push(p_stream->deads, p_thread->unit);

    return ABT_SUCCESS;
}


/* Internal static functions */
static ABT_Stream_id g_stream_id = 0;
static ABT_Stream_id ABTI_Stream_get_new_id()
{
    /* FIXME: Need to be atomic */
    return g_stream_id++;
}

