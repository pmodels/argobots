/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <abt.h>

#define DEFAULT_NUM_THREADS     4
#define DEFAULT_NAME_LEN        16

#define HANDLE_ERROR(ret,msg)                           \
    if (ret != ABT_SUCCESS) {                           \
        fprintf(stderr, "ERROR[%d]: %s\n", ret, msg);   \
        exit(EXIT_FAILURE);                             \
    }

typedef struct thread_arg {
    int id;
    char name[DEFAULT_NAME_LEN];
} thread_arg_t;

void thread_func(void *arg)
{
    thread_arg_t *t_arg = (thread_arg_t *)arg;
    printf("[TH%d]: My name is %s.\n", t_arg->id, t_arg->name);
}

int main(int argc, char *argv[])
{
    int i, ret;
    int num_threads = DEFAULT_NUM_THREADS;
    if (argc > 1) num_threads = atoi(argv[1]);
    assert(num_threads >= 0);

    ABT_stream stream;
    ABT_thread *threads;
    thread_arg_t *args;
    char stream_name[16];
    size_t name_len;
    char *name;
    threads = (ABT_thread *)malloc(sizeof(ABT_thread *) * num_threads);
    args = (thread_arg_t *)malloc(sizeof(thread_arg_t) * num_threads);

    /* Initialize */
    ret = ABT_init(argc, argv);
    HANDLE_ERROR(ret, "ABT_init");

    /* Get the SELF stream */
    ret = ABT_stream_self(&stream);
    HANDLE_ERROR(ret, "ABT_stream_self");

    /* Set the stream's name */
    sprintf(stream_name, "SELF-stream");
    printf("Set the stream's name as '%s'\n", stream_name);
    ret = ABT_stream_set_name(stream, stream_name);
    HANDLE_ERROR(ret, "ABT_stream_set_name");

    /* Create threads */
    for (i = 0; i < num_threads; i++) {
        args[i].id = i + 1;
        sprintf(args[i].name, "arogobot-%d", i);
        ret = ABT_thread_create(stream,
                thread_func, (void *)&args[i], 4096,
                &threads[i]);
        HANDLE_ERROR(ret, "ABT_thread_create");

        /* Set the thread's name */
        ret = ABT_thread_set_name(threads[i], args[i].name);
        HANDLE_ERROR(ret, "ABT_thread_set_name");
    }

    /* Get the stream's name */
    ret = ABT_stream_get_name(stream, NULL, &name_len);
    HANDLE_ERROR(ret, "ABT_stream_get_name");
    name = (char *)malloc(sizeof(char) * (name_len + 1));
    ret = ABT_stream_get_name(stream, stream_name, &name_len);
    HANDLE_ERROR(ret, "ABT_stream_get_name");
    printf("Stream's name: %s\n", stream_name);
    free(name);

    /* Get the threads' names */
    for (i = 0; i < num_threads; i++) {
        char thread_name[16];
        ret = ABT_thread_get_name(threads[i], thread_name, &name_len);
        HANDLE_ERROR(ret, "ABT_thread_get_name");
        printf("TH%d's name: %s\n", i + 1, thread_name);
    }

    /* Switch to other user level threads */
    ABT_thread_yield();

    /* Free threads */
    for (i = 0; i < num_threads; i++) {
        ret = ABT_thread_free(&threads[i]);
        HANDLE_ERROR(ret, "ABT_thread_free");
    }

    /* Finalize */
    ret = ABT_finalize();
    HANDLE_ERROR(ret, "ABT_finalize");

    free(args);
    free(threads);

    return EXIT_SUCCESS;
}
