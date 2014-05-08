#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "abt.h"

#define DEFAULT_NUM_STREAMS     2
#define DEFAULT_NUM_THREADS     2

void thread_func(void *arg)
{
    size_t my_id = (size_t)arg;
    printf("[T%lu]: Hello, world!\n", my_id);

}

int main(int argc, char *argv[])
{
    int i, j;
    int ret;
    int num_streams = DEFAULT_NUM_STREAMS;
    int num_threads = DEFAULT_NUM_THREADS;
    if (argc > 1) num_streams = atoi(argv[1]);
    assert(num_streams >= 0);
    if (argc > 2) num_threads = atoi(argv[2]);
    assert(num_threads >= 0);

    ABT_Stream *streams;
    streams = (ABT_Stream *)malloc(sizeof(ABT_Stream) * num_streams);

    /* Create streams */
    streams[0] = ABT_Stream_self();
    for (i = 1; i < num_streams; i++) {
        ret = ABT_Stream_create(ABT_SCHEDULER_NULL, &streams[i]);
        if (ret != ABT_SUCCESS) {
            fprintf(stderr, "ERROR: ABT_Stream_create for ES%d\n", i);
            exit(EXIT_FAILURE);
        }
    }

    /* Create threads */
    for (i = 0; i < num_streams; i++) {
        for (j = 0; j < num_threads; j++) {
            size_t tid = i * num_threads + j;
            ret = ABT_Thread_create(streams[i],
                    thread_func, (void *)tid, 16384,
                    NULL);
            if (ret != ABT_SUCCESS) {
                fprintf(stderr, "ERROR: ABT_Thread_create for ES%d-LUT%d\n",
                        i, j);
                exit(EXIT_FAILURE);
            }
        }
    }

    /* Switch to other user level threads */
    ABT_Thread_yield();

    /* Join streams */
    for (i = 1; i < num_streams; i++) {
        ret = ABT_Stream_join(streams[i]);
        if (ret != ABT_SUCCESS) {
            fprintf(stderr, "ERROR: ABT_Stream_join for ES%d\n", i);
            exit(EXIT_FAILURE);
        }
    }

    /* Free streams */
    for (i = 0; i < num_streams; i++) {
        ret = ABT_Stream_free(streams[i]);
        if (ret != ABT_SUCCESS) {
            fprintf(stderr, "ERROR: ABT_Stream_free for ES%d\n", i);
            exit(EXIT_FAILURE);
        }
    }

    free(streams);

    return EXIT_SUCCESS;
}
