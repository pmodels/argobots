/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
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
static int *ready;
static ABT_cond *ready_cond;
static ABT_mutex *ready_mutex;
static int print;

static void compute(void *args)
{
    int x, y;
    int *intargs = (int *)args;
    int firstX = intargs[0];
    int firstY = intargs[1];

    for (x = firstX; x < firstX+blockSize; x++) {
        for (y = firstY; y < firstY+blockSize; y++) {
            results[here(x,y)] = (values[left(x,y)]+values[right(x,y)]
                                 +values[up(x,y)]+values[down(x,y)])/4.0;
        }
    }

    /* We signal the neigbours that we have read their value */
    /* Left neighbour */
    x = firstX;
    if (x > 1) {
        for (y = firstY; y < firstY+blockSize; y++) {
            int coord = left(x,y)*4;
            ABT_mutex_lock(ready_mutex[coord]);
            ready[coord] = 1;
            ABT_cond_signal(ready_cond[coord]);
            ABT_mutex_unlock(ready_mutex[coord]);
        }
    }

    /* Right neighbour */
    x = firstX+blockSize-1;
    if (x < ncells) {
        for (y = firstY; y < firstY+blockSize; y++) {
            int coord = right(x,y)*4+1;
            ABT_mutex_lock(ready_mutex[coord]);
            ready[coord] = 1;
            ABT_cond_signal(ready_cond[coord]);
            ABT_mutex_unlock(ready_mutex[coord]);
        }
    }

    /* Up neighbour */
    y = firstY;
    if (y > 1) {
        for (x = firstX; x < firstX+blockSize; x++) {
            int coord = up(x,y)*4+2;
            ABT_mutex_lock(ready_mutex[coord]);
            ready[coord] = 1;
            ABT_cond_signal(ready_cond[coord]);
            ABT_mutex_unlock(ready_mutex[coord]);
        }
    }

    /* Down neighbour */
    y = firstY+blockSize-1;
    if (y < ncells) {
        for (x = firstX; x < firstX+blockSize; x++) {
            int coord = down(x,y)*4+3;
            ABT_mutex_lock(ready_mutex[coord]);
            ready[coord] = 1;
            ABT_cond_signal(ready_cond[coord]);
            ABT_mutex_unlock(ready_mutex[coord]);
        }
    }

    /* We wait for the signal of the neigbours */
    /* From left neighbour */
    x = firstX;
    if (x > 1) {
        for (y = firstY; y < firstY+blockSize; y++) {
            int coord = here(x,y)*4+1;
            ABT_mutex_lock(ready_mutex[coord]);
            if (!ready[coord]) {
                ABT_cond_wait(ready_cond[coord], ready_mutex[coord]);
            }
            ABT_mutex_unlock(ready_mutex[coord]);
        }
    }

    /* From right neighbour */
    x = firstX+blockSize-1;
    if (x < ncells) {
        for (y = firstY; y < firstY+blockSize; y++) {
            int coord = here(x,y)*4;
            ABT_mutex_lock(ready_mutex[coord]);
            if (!ready[coord]) {
                ABT_cond_wait(ready_cond[coord], ready_mutex[coord]);
            }
            ABT_mutex_unlock(ready_mutex[coord]);
        }
    }

    /* From up neighbour */
    y = firstY;
    if (y > 1) {
        for (x = firstX; x < firstX+blockSize; x++) {
            int coord = here(x,y)*4+3;
            ABT_mutex_lock(ready_mutex[coord]);
            if (!ready[coord]) {
                ABT_cond_wait(ready_cond[coord], ready_mutex[coord]);
            }
            ABT_mutex_unlock(ready_mutex[coord]);
        }
    }

    /* From down neighbour */
    y = firstY+blockSize-1;
    if (y < ncells) {
        for (x = firstX; x < firstX+blockSize; x++) {
            int coord = here(x,y)*4+2;
            ABT_mutex_lock(ready_mutex[coord]);
            if (!ready[coord]) {
                ABT_cond_wait(ready_cond[coord], ready_mutex[coord]);
            }
            ABT_mutex_unlock(ready_mutex[coord]);
        }
    }

    for (x = firstX; x < firstX+blockSize; x++) {
        for (y = firstY; y < firstY+blockSize; y++) {
            values[here(x,y)] = results[here(x,y)];
        }
    }
}

static void run(void)
{
    ABT_thread *threads = malloc(nBlocks*nBlocks*sizeof(ABT_thread));
    int *args = (int *)malloc(nBlocks*nBlocks*2*sizeof(int));
    int threadIdx, argsIdx;

    int s = 0;
    int i, x, y, t;
    int ret;

    for (i = 0; i < niterations; i++) {
        threadIdx = 0;
        for (x = 1; x < ncells+1; x += blockSize) {
            for (y = 1; y < ncells+1; y += blockSize) {
                argsIdx = threadIdx * 2;
                args[argsIdx+0] = x;
                args[argsIdx+1] = y;
                ret = ABT_thread_create(pools[s], compute,
                                        (void *)&args[argsIdx],
                                        ABT_THREAD_ATTR_NULL,
                                        &threads[threadIdx]);
                HANDLE_ERROR(ret, "ABT_thread_create");
                threadIdx++;
                s = (s + 1) % num_xstreams;
            }
        }

        for (t = 0; t < nBlocks*nBlocks; t++) {
            ABT_thread_free(&threads[t]);
        }

        /* Reset the ready array */
        memset(ready, 0, 4*(ncells+2)*(ncells+2)*sizeof(int));
    }

    free(threads);
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

    ready = (int *)calloc(4*(ncells+2)*(ncells+2), sizeof(int));
    ready_cond = (ABT_cond *)malloc(4*(ncells+2)*(ncells+2)*sizeof(ABT_cond));
    ready_mutex = (ABT_mutex *)malloc(4*(ncells+2)*(ncells+2)*sizeof(ABT_mutex));
    for (x = 1; x < ncells+1; x++) {
        for (y = 1; y < ncells+1; y++) {
            for (i = 0; i < 4; i++) {
                ABT_cond_create(&ready_cond[here(x,y)*4+i]);
                ABT_mutex_create(&ready_mutex[here(x,y)*4+i]);
            }
        }
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

    for (x = 1; x < ncells+1; x++) {
        for (y = 1; y < ncells+1; y++) {
            for (i = 0; i < 4; i++) {
                ABT_cond_free(&ready_cond[here(x,y)*4+i]);
                ABT_mutex_free(&ready_mutex[here(x,y)*4+i]);
            }
        }
    }

    free(ready);
    free(ready_mutex);
    free(ready_cond);

    ABT_finalize();

    return EXIT_SUCCESS;
}

