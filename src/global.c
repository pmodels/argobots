/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"

/* FIXME: is the global pointer the best way? */

/* Current stream */
__thread ABTI_Stream *gp_stream = NULL;

/* Current running thread */
__thread ABTI_Thread *gp_thread = NULL;

/* Argobots Global Data Structure */
ABTI_Global *gp_ABT = NULL;


int ABT_Init(int argc, char **argv)
{
    assert(gp_ABT == NULL);
    int abt_errno = ABT_SUCCESS;

    gp_ABT = (ABTI_Global *)ABTU_Malloc(sizeof(ABTI_Global));
    if (!gp_ABT) {
        HANDLE_ERROR("ABTU_Malloc");
        abt_errno = ABT_ERR_MEM;
        goto fn_fail;
    }

    /* Initialize the stream pool */
    gp_ABT->p_streams = (ABTI_Stream_pool *)ABTU_Malloc(
                        sizeof(ABTI_Stream_pool));
    if (!gp_ABT->p_streams) {
        HANDLE_ERROR("ABTU_Malloc");
        abt_errno = ABT_ERR_MEM;
        goto fn_fail;
    }
    abt_errno = ABTI_Stream_init(gp_ABT->p_streams);
    if (abt_errno != ABT_SUCCESS) {
        HANDLE_ERROR("ABTI_Stream_init");
        goto fn_fail;
    }

    /* Initialize the task pool */
    gp_ABT->p_tasks = (ABTI_Task_pool *)ABTU_Malloc(sizeof(ABTI_Task_pool));
    if (!gp_ABT->p_tasks) {
        HANDLE_ERROR("ABTU_Malloc");
        abt_errno = ABT_ERR_MEM;
        goto fn_fail;
    }
    abt_errno = ABTI_Task_init(gp_ABT->p_tasks);
    if (abt_errno != ABT_SUCCESS) {
        HANDLE_ERROR("ABTI_Task_init");
        goto fn_fail;
    }

  fn_exit:
    return abt_errno;

  fn_fail:
    goto fn_exit;
}

int ABT_Finalize()
{
    assert(gp_ABT != NULL);
    int abt_errno = ABT_SUCCESS;

    /* Finalize the task pool */
    abt_errno = ABTI_Task_finalize(gp_ABT->p_tasks);
    if (abt_errno != ABT_SUCCESS) {
        HANDLE_ERROR("ABTI_Task_finalize");
        goto fn_fail;
    }
    ABTU_Free(gp_ABT->p_tasks);

    /* Finalize the stream pool */
    abt_errno = ABTI_Stream_finalize(gp_ABT->p_streams);
    if (abt_errno != ABT_SUCCESS) {
        HANDLE_ERROR("ABTI_Stream_finalize");
        goto fn_fail;
    }
    ABTU_Free(gp_ABT->p_streams);

    /* Free the ABTI_Global structure */
    ABTU_Free(gp_ABT);
    gp_ABT = NULL;

  fn_exit:
    return abt_errno;

  fn_fail:
    goto fn_exit;
}

