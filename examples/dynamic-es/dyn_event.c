/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include "abt.h"
#include "dyn_event.h"

static int g_max_xstreams;          /* maximum # of ESs */
static int g_num_xstreams;          /* current # of ESs */
double g_timeout = 2.0;             /* timeout value in secs. */

ABT_xstream *g_xstreams = NULL;

static void init(int max_xstreams);
static void finalize(int max_xstreams);
static void run_app(void);


int main(int argc, char *argv[])
{
    if (argc != 3) {
        printf("Usage: %s <max # of ESs> <timeout>\n", argv[0]);
        return EXIT_SUCCESS;
    }

    g_max_xstreams = atoi(argv[1]);
    if (g_max_xstreams < 1) {
        g_max_xstreams = 1;
    }
    g_num_xstreams = g_max_xstreams;
    g_timeout = atof(argv[2]);

    printf("# of ESs: %d\n", g_max_xstreams);
    printf("Timeout : %.1f sec.\n", g_timeout);

    /* Initialize */
    init(g_max_xstreams);

    run_app();

    /* Finalize */
    finalize(g_max_xstreams);

    printf("Done.\n");

    return EXIT_SUCCESS;
}

static void init(int max_xstreams)
{
    int i;

    ABT_init(0, NULL);

    g_xstreams = (ABT_xstream *)malloc(max_xstreams * sizeof(ABT_xstream));

    /* Create ESs */
    ABT_xstream_self(&g_xstreams[0]);
    for (i = 1; i < max_xstreams; i++) {
        ABT_xstream_create(ABT_SCHED_NULL, &g_xstreams[i]);
    }
}

static void finalize(int max_xstreams)
{
    int i;

    for (i = 1; i < max_xstreams; i++) {
        if (g_xstreams[i] != ABT_XSTREAM_NULL) {
            ABT_xstream_join(g_xstreams[i]);
            ABT_xstream_free(&g_xstreams[i]);
        }
    }
    free(g_xstreams);

    ABT_finalize();
}

static void run_app(void)
{
    ABT_pool pool;
    int i;

    rt1_init(g_max_xstreams, g_xstreams);

    for (i = 1; i < g_max_xstreams; i++) {
        ABT_xstream_get_main_pools(g_xstreams[i], 1, &pool);
        ABT_thread_create(pool, rt1_launcher, (void *)(intptr_t)i,
                          ABT_THREAD_ATTR_NULL, NULL);
    }

    rt1_launcher((void *)0);

    rt1_finalize();
}

