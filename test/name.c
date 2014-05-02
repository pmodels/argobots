/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "abt.h"

#define DEFAULT_NUM_THREADS     2
#define DEFAULT_NAME_LEN        16

typedef struct thread_arg {
    int id;
    char name[DEFAULT_NAME_LEN];
} thread_arg_t;

void thread_func(void *arg)
{
    thread_arg_t *t_arg = (thread_arg_t *)arg;
    printf("    [T%d]: My name is %s.\n", t_arg->id, t_arg->name);
}

int main(int argc, char *argv[])
{
    int i, ret;
    int num_threads = DEFAULT_NUM_THREADS;
    if (argc > 1) num_threads = atoi(argv[1]);
    assert(num_threads >= 0);

    ABT_Stream stream;
    ABT_Thread *threads;
    thread_arg_t *args;
    char stream_name[16];
    threads = (ABT_Thread *)malloc(sizeof(ABT_Thread *) * num_threads);
    args = (thread_arg_t *)malloc(sizeof(thread_arg_t) * num_threads);

    /* Get the SELF stream */
    stream = ABT_Stream_self();

    /* Set the stream's name */
    sprintf(stream_name, "SELF-stream");
    printf("Set the stream's name as '%s'\n", stream_name);
    ret = ABT_Stream_set_name(stream, stream_name);
    if (ret != ABT_SUCCESS) {
        fprintf(stderr, "ERROR: ABT_Stream_set_name\n");
        exit(EXIT_FAILURE);
    }

    /* Create threads */
    for (i = 0; i < num_threads; i++) {
        args[i].id = i;
        sprintf(args[i].name, "arogobot-%d", i);
        ret = ABT_Thread_create(stream,
                thread_func, (void *)&args[i], 4096,
                &threads[i]);
        if (ret != ABT_SUCCESS) {
            fprintf(stderr, "ERROR: ABT_Thread_create for LUT%d\n", i);
            exit(EXIT_FAILURE);
        }

        /* Set the thread's name */
        ret = ABT_Thread_set_name(threads[i], args[i].name);
        if (ret != ABT_SUCCESS) {
            fprintf(stderr, "ERROR: ABT_Thread_set_name for LUT%d\n", i);
            exit(EXIT_FAILURE);
        }
    }

    /* Get the stream's name */
    ret = ABT_Stream_get_name(stream, stream_name, DEFAULT_NAME_LEN);
    if (ret != ABT_SUCCESS) {
        fprintf(stderr, "ERROR: ABT_Stream_set_name\n");
        exit(EXIT_FAILURE);
    }
    printf("Stream's name: %s\n", stream_name);

    /* Get the threads' names */
    for (i = 0; i < num_threads; i++) {
        char thread_name[16];
        ret = ABT_Thread_get_name(threads[i], thread_name, 16);
        if (ret != ABT_SUCCESS) {
            fprintf(stderr, "ERROR: ABT_Thread_get_name for LUT%d\n", i);
            exit(EXIT_FAILURE);
        }
        printf("T%d's name: %s\n", i, thread_name);
    }

    /* Switch to other user level threads */
    ABT_Thread_yield();

    /* Free streams */
    ret = ABT_Stream_free(stream);
    if (ret != ABT_SUCCESS) {
        fprintf(stderr, "ERROR: AB_stream_free\n");
        exit(EXIT_FAILURE);
    }

    free(args);
    free(threads);

    return EXIT_SUCCESS;
}
