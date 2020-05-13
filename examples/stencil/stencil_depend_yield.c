/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */
/*
 *
 * Base implementation: stencil_barrier.c
 *
 * Parallel 2D stencil code that uses yield-based synchronization.  First, it
 * spawns as many ULTs as the number of blocks (num_blocksX * num_blocksY).  For
 * halo synchronization, a ULT waits for completion of all the four neighbors'
 * computations by checking their "progress" flags; each element stores the
 * completed iteration (i.e., t) of the associated block.  If neighbors'
 * computations have not finished, the ULT yields to a scheduler.
 *
 * This exposes more parallelism than the barrier method; in this method, each
 * ULT only waits for completion of previous computations of four neighbor
 * blocks whereas, with a barrier synchronization, each ULT needs to wait for
 * completion of previous computations of all blocks.
 *
 * ULTs cannot be simply replaced with tasklets in this case since
 * ABT_thread_yield() in kernel() is a synchronization operation that cannot be
 * called on a tasklet.
 */

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <abt.h>
#include "stencil_helper.h"

/* Global variables. */
int num_blocksX, num_blocksY;
int blocksize;
int num_iters;
int num_xstreams;
int validate;

typedef struct {
    double *values_old;
    double *values_new;
    int blockX;
    int blockY;
    /* Progress flag stores the completed iteration (t). */
    int *self_progress_flag;
    /* Progress flags of neighbors. */
    int *neighbor_progress_flags[4];
} kernel_arg_t;

void atomic_init(void);
void atomic_finalize(void);
int atomic_acquire_load(int *ptr);
void atomic_release_store(int *ptr, int val);

void kernel(void *arg)
{
    double *values_old = ((kernel_arg_t *)arg)->values_old;
    double *values_new = ((kernel_arg_t *)arg)->values_new;
    int blockX = ((kernel_arg_t *)arg)->blockX;
    int blockY = ((kernel_arg_t *)arg)->blockY;
    int *self_progress_flag = ((kernel_arg_t *)arg)->self_progress_flag;
    int **neighbor_progress_flags =
        ((kernel_arg_t *)arg)->neighbor_progress_flags;
    /* Iterates stencil computation. */
    for (int t = 0; t < num_iters; t++) {
        /* Check progress of neighbors. */
        for (int i = 0; i < 4; i++) {
            if (neighbor_progress_flags[i] == NULL)
                continue;
            while (1) {
                if (atomic_acquire_load(neighbor_progress_flags[i]) >= t) {
                    break;
                }
                /* Yield to a scheduler if computation has not finished yet. */
                ABT_thread_yield();
            }
        }
        /* Run the stencil kernel. */
        for (int y = blockY * blocksize; y < (blockY + 1) * blocksize; y++) {
            for (int x = blockX * blocksize; x < (blockX + 1) * blocksize;
                 x++) {
                values_new[INDEX(x, y)] =
                    values_old[INDEX(x, y)] * (1.0 / 2.0) +
                    (values_old[INDEX(x + 1, y)] + values_old[INDEX(x - 1, y)] +
                     values_old[INDEX(x, y + 1)] +
                     values_old[INDEX(x, y - 1)]) *
                        (1.0 / 8.0);
            }
        }
        /* Update its progress flag. */
        atomic_release_store(self_progress_flag, t + 1);
        /* Swap values_old and values_new. */
        double *values_tmp = values_new;
        values_new = values_old;
        values_old = values_tmp;
    }
}

int main(int argc, char **argv)
{
    /* Read arguments. */
    int read_arg_ret =
        read_args(argc, argv, &num_blocksX, &num_blocksY, &blocksize,
                  &num_iters, &num_xstreams, &validate);
    if (read_arg_ret != 0) {
        return -1;
    }

    /* Allocate memory. */
    ABT_xstream *xstreams =
        (ABT_xstream *)malloc(sizeof(ABT_xstream) * num_xstreams);
    ABT_pool *pools = (ABT_pool *)malloc(sizeof(ABT_pool) * num_xstreams);
    double *values_old = (double *)malloc(sizeof(double) * WIDTH * HEIGHT);
    double *values_new = (double *)malloc(sizeof(double) * WIDTH * HEIGHT);
    ABT_thread *threads =
        (ABT_thread *)malloc(sizeof(ABT_thread) * num_blocksX * num_blocksY);
    int *progress_flags = (int *)calloc(num_blocksX * num_blocksY, sizeof(int));
    kernel_arg_t *kernel_args = (kernel_arg_t *)malloc(
        sizeof(kernel_arg_t) * num_blocksX * num_blocksY);

    /* Initialize grid values. */
    init_values(values_old, num_blocksX, num_blocksY, blocksize);

    /* Initialize Argobots and atomic functionality. */
    ABT_init(argc, argv);
    atomic_init();

    /* Get a primary execution stream. */
    ABT_xstream_self(&xstreams[0]);

    /* Create secondary execution streams. */
    for (int i = 1; i < num_xstreams; i++) {
        ABT_xstream_create(ABT_SCHED_NULL, &xstreams[i]);
    }

    /* Get default pools. */
    for (int i = 0; i < num_xstreams; i++) {
        ABT_xstream_get_main_pools(xstreams[i], 1, &pools[i]);
    }

    /* Create ULTs. */
    for (int blockX = 0; blockX < num_blocksX; blockX++) {
        for (int blockY = 0; blockY < num_blocksY; blockY++) {
            int index = blockX + blockY * num_blocksX;
            kernel_arg_t *p_kernel_arg = &kernel_args[index];
            p_kernel_arg->values_old = values_old;
            p_kernel_arg->values_new = values_new;
            p_kernel_arg->blockX = blockX;
            p_kernel_arg->blockY = blockY;
            p_kernel_arg->self_progress_flag = &progress_flags[index];
            const int dirX[] = { -1, 1, 0, 0 };
            const int dirY[] = { 0, 0, -1, 1 };
            for (int i = 0; i < 4; i++) {
                int neighborX = blockX + dirX[i];
                int neighborY = blockY + dirY[i];
                if (0 <= neighborX && neighborX < num_blocksX &&
                    0 <= neighborY && neighborY < num_blocksY) {
                    int neighbor_index = neighborX + neighborY * num_blocksX;
                    p_kernel_arg->neighbor_progress_flags[i] =
                        &progress_flags[neighbor_index];
                } else {
                    /* There is no grid. */
                    p_kernel_arg->neighbor_progress_flags[i] = NULL;
                }
            }
            int pool_id = index % num_xstreams;
            ABT_thread_create(pools[pool_id], kernel, p_kernel_arg,
                              ABT_THREAD_ATTR_NULL, &threads[index]);
        }
    }

    /* Join and free ULTs. */
    for (int blockX = 0; blockX < num_blocksX; blockX++) {
        for (int blockY = 0; blockY < num_blocksY; blockY++) {
            int index = blockX + blockY * num_blocksX;
            ABT_thread_free(&threads[index]);
        }
    }

    /* Swap values_old and values_new for validation. */
    if (num_iters % 2 == 1) {
        /* num_iters = 2N    : the latest is written to values_old. */
        /*           = 2N + 1: the latest is written to values_new. */
        double *values_tmp = values_new;
        values_new = values_old;
        values_old = values_tmp;
    }

    /* Join secondary execution streams. */
    for (int i = 1; i < num_xstreams; i++) {
        ABT_xstream_join(xstreams[i]);
        ABT_xstream_free(&xstreams[i]);
    }

    /* Finalize Argobots and atomic functionality. */
    atomic_finalize();
    ABT_finalize();

    /* Validate results.  values_old has the latest values. */
    int validate_ret = 0;
    if (validate) {
        validate_ret = validate_values(values_old, num_blocksX, num_blocksY,
                                       blocksize, num_iters);
    }

    /* Free allocated memory. */
    free(xstreams);
    free(pools);
    free(values_old);
    free(values_new);
    free(threads);
    free(progress_flags);
    free(kernel_args);

    if (validate_ret != 0) {
        printf("Validation failed.\n");
        return -1;
    } else if (validate) {
        printf("Validation succeeded.\n");
    }
    return 0;
}

#if !(defined(__GNUC__) && defined(__GNUC_MINOR__) &&                          \
      (__GNUC__ * 100 + __GNUC_MINOR__ >= 407))
/* GCC version is >= 4.7, so it supports __atomic. */
#define USE_ABT_MUTEX
ABT_mutex atomic_mutex;
#endif

void atomic_init(void)
{
#ifdef USE_ABT_MUTEX
    ABT_mutex_create(&atomic_mutex);
#endif
}

void atomic_finalize(void)
{
#ifdef USE_ABT_MUTEX
    ABT_mutex_free(&atomic_mutex);
#endif
}

int atomic_acquire_load(int *ptr)
{
#ifdef USE_ABT_MUTEX
    ABT_mutex_spinlock(atomic_mutex);
    int ret = *ptr;
    ABT_mutex_unlock(atomic_mutex);
    return ret;
#else
    return __atomic_load_n(ptr, __ATOMIC_ACQUIRE);
#endif
}

void atomic_release_store(int *ptr, int val)
{
#ifdef USE_ABT_MUTEX
    ABT_mutex_spinlock(atomic_mutex);
    *ptr = val;
    ABT_mutex_unlock(atomic_mutex);
#else
    __atomic_store_n(ptr, val, __ATOMIC_RELEASE);
#endif
}
