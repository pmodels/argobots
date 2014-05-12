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

/* Global task pool */
ABTI_Task_pool *gp_tasks = NULL;

