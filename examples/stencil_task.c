/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <abt.h>

#define N                       4
#define NBLOCKS                 32
#define NITER                   50
#define DEFAULT_NUM_XSTREAMS    4
#define PRINT                   0

#define HANDLE_ERROR(ret,msg)                       \
  if (ret != ABT_SUCCESS) {                         \
    fprintf(stderr, "ERROR[%d]: %s\n", ret, msg);   \
    exit(EXIT_FAILURE);                             \
  }

#define totalSize  (ncells+2)
#define here(x,y)  ((x)*totalSize+y)
#define left(x,y)  ((x-1)*totalSize+y)
#define right(x,y) ((x+1)*totalSize+y)
#define up(x,y)    ((x)*totalSize+y-1)
#define down(x,y)  ((x)*totalSize+y+1)

static void compute(void *args);
static void update(void *args);
static void run(void);

/* global variables */
static int num_xstreams;
static ABT_xstream *xstreams;
static ABT_pool *pools;
static int ncells;
static int blockSize;
static int nBlocks;
static int niterations;
static double *values;
static double *results;
static int print;

static void compute(void *args)
{
    int x, y;
    int *intargs = (int *)args;
    int firstX = intargs[0];
    int firstY = intargs[1];

    for (x = firstX; x < firstX+blockSize; x++) {
        for (y = firstY; y < firstY+blockSize; y++) {
            results[x*totalSize+y] = (values[left(x,y)]+values[right(x,y)]
                                     +values[up(x,y)]+values[down(x,y)])/4.0;
        }
    }
}

static void update(void *args)
{
    int x, y;
    int *intargs = (int *)args;
    int firstX = intargs[0];
    int firstY = intargs[1];

    for (x = firstX; x < firstX+blockSize; x++) {
        for (y = firstY; y < firstY+blockSize; y++) {
            values[here(x,y)] = results[here(x,y)];
        }
    }
}

static void run(void)
{
    ABT_task *tasks = malloc(nBlocks*nBlocks*sizeof(ABT_task));
    int *args = (int *)malloc(nBlocks*nBlocks*2*sizeof(int));
    int taskIdx, argsIdx;

    int s = 0;
    int i, x, y, t;
    int ret;

    for (i = 0; i < niterations; i++) {
        taskIdx = 0;
        for (x = 1; x < ncells+1; x += blockSize) {
            for (y = 1; y < ncells+1; y += blockSize) {
                argsIdx = taskIdx * 2;
                args[argsIdx+0] = x;
                args[argsIdx+1] = y;
                ret = ABT_task_create(pools[s], compute, (void *)&args[argsIdx],
                                      &tasks[taskIdx]);
                HANDLE_ERROR(ret, "ABT_task_create");
                taskIdx++;
                s = (s + 1) % num_xstreams;
            }
        }

        for (t = 0; t < nBlocks*nBlocks; t++) {
            ABT_task_free(&tasks[t]);
        }

        taskIdx = 0;
        for (x = 1; x < ncells+1; x += blockSize) {
            for (y = 1; y < ncells+1; y += blockSize) {
                argsIdx = taskIdx * 2;
                args[argsIdx+0] = x;
                args[argsIdx+1] = y;
                ret = ABT_task_create(pools[s], update, (void *)&args[argsIdx],
                                      &tasks[taskIdx]);
                HANDLE_ERROR(ret, "ABT_task_create");
                taskIdx++;
                s = (s + 1) % num_xstreams;
            }
        }

        for (t = 0; t < nBlocks*nBlocks; t++) {
            ABT_task_free(&tasks[t]);
        }
    }

    free(tasks);
    free(args);
}

static int eq(double a, double b)
{
    double e = 0.00001;
    if (a < b)
        return (b-a < e);
    else
        return (a-b < e);
}

static int check(double *results, int n)
{
    int i, j;
    for (i = 0; i < n/2; i++) {
        for (j = 0; j < n/2; j++) {
            if (!eq(results[i*n+j], results[i*n+(n-1-j)]) ||
                !eq(results[i*n+j], results[(n-1-i)*n+j]) ||
                !eq(results[i*n+j], results[(n-1-i)*n+(n-1-j)]))
                return 0;
        }
    }
    return 1;
}

int main(int argc, char *argv[])
{
    int ret;
    int x, y;
    int i;

    ABT_init(argc, argv);

    blockSize    = (argc > 1) ? atoi(argv[1]) : N;
    nBlocks      = (argc > 2) ? atoi(argv[2]) : NBLOCKS;
    niterations  = (argc > 3) ? atoi(argv[3]) : NITER;
    num_xstreams = (argc > 4) ? atoi(argv[4]) : DEFAULT_NUM_XSTREAMS;
    print        = (argc > 5) ? atoi(argv[5]) : PRINT;

    assert(blockSize > 0);

    ncells = blockSize*nBlocks;

    /* ES creation */
    xstreams = (ABT_xstream *)malloc(sizeof(ABT_xstream) * num_xstreams);
    ret = ABT_xstream_self(&xstreams[0]);
    for (i = 1; i < num_xstreams; i++) {
        ret = ABT_xstream_create(ABT_SCHED_NULL, &xstreams[i]);
        HANDLE_ERROR(ret, "ABT_xstream_create");
    }

    /* Get the main pools */
    pools = (ABT_pool *)malloc(sizeof(ABT_pool) * num_xstreams);
    for (i = 0; i < num_xstreams; i++) {
        ABT_xstream_get_main_pools(xstreams[i], 1, &pools[i]);
    }

    results = (double *)calloc((ncells+2)*(ncells+2), sizeof(double));
    values = (double *)calloc((ncells+2)*(ncells+2), sizeof(double));
    for (y = 1; y < ncells+1; y++) {
        values[here(0, y)] = 1;
        values[here(ncells+1, y)] = 1;
    }

    run();

    /* Show results */
    if (print) {
        for (x = 1; x < ncells+1; x++) {
            for (y = 1; y < ncells+1; y++) {
                printf("%5f ", results[here(x,y)]);
            }
            printf("\n");
        }
    }

    if (!check(results, ncells+2)) {
        printf("Wrong result !!!!\n");
        return -1;
    } else {
        printf("No Errors\n");
    }

    /* Join ESs */
    for (i = 1; i < num_xstreams; i++) {
        ret = ABT_xstream_join(xstreams[i]);
        HANDLE_ERROR(ret, "ABT_xstream_join");
    }

    /* Deallocating ESs */
    for (i = 1; i < num_xstreams; i++) {
        ret = ABT_xstream_free(&xstreams[i]);
        HANDLE_ERROR(ret, "ABT_xstream_free");
    }
    free(xstreams);
    free(pools);

    /* Frees */
    free(results);
    free(values);

    ABT_finalize();

    return EXIT_SUCCESS;
}

