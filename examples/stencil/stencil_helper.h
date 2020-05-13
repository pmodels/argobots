/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#ifndef __STENCIL_HELPER_H__
#define __STENCIL_HELPER_H__

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>

#define WIDTH (num_blocksX * blocksize + 2)
#define HEIGHT (num_blocksY * blocksize + 2)
#define INDEX(_X, _Y) ((_X) + 1 + WIDTH * (_Y + 1))

#define DEFAULT_NUM_BLOCKSX 8
#define DEFAULT_NUM_BLOCKSY 8
#define DEFAULT_BLOCKSIZE 32
#define DEFAULT_NUM_ITERS 10
#define DEFAULT_NUM_XSTREAMS 4
#define DEFAULT_VALIDATE 1
#define ERROR_TRESHOLD (1.0e-6)

/* Read arguments.  Return non-zero value if failed. */
static int read_args(int argc, char **argv, int *p_num_blocksX,
                     int *p_num_blocksY, int *p_blocksize, int *p_num_iters,
                     int *p_num_xstreams, int *p_validate);
/* Initialize grid values. */
static void init_values(double *values, int num_blocksX, int num_blocksY,
                        int blocksize);
/* Validate results.  Return non-zero value if failed.*/
static int validate_values(const double *values, int num_blocksX,
                           int num_blocksY, int blocksize, int num_iters);

static void print_help(int argc, char **argv)
{
    printf("Usage: %s [-e NUM_XSTREAMS] [-x NUM_BLOCKX] [-y NUM_BLOCKY] "
           "[-b BLOCKSIZE] [-i NUM_ITERS] [-v VALIDATE]\n",
           argv[0]);
}

static int read_args(int argc, char **argv, int *p_num_blocksX,
                     int *p_num_blocksY, int *p_blocksize, int *p_num_iters,
                     int *p_num_xstreams, int *p_validate)
{
    *p_num_blocksX = DEFAULT_NUM_BLOCKSX;
    *p_num_blocksY = DEFAULT_NUM_BLOCKSY;
    *p_blocksize = DEFAULT_BLOCKSIZE;
    *p_num_iters = DEFAULT_NUM_ITERS;
    *p_num_xstreams = DEFAULT_NUM_XSTREAMS;
    *p_validate = DEFAULT_VALIDATE;
    while (1) {
        int opt = getopt(argc, argv, "hx:y:b:i:e:v:");
        if (opt == -1)
            break;
        switch (opt) {
            case 'x':
                *p_num_blocksX = atoi(optarg);
                if (*p_num_blocksX <= 0) {
                    printf("NUM_BLOCKX (-x) must be a positive integer.\n");
                    return -1;
                }
                break;
            case 'y':
                *p_num_blocksY = atoi(optarg);
                if (*p_num_blocksY <= 0) {
                    printf("NUM_BLOCKY (-y) must be a positive integer.\n");
                    return -1;
                }
                break;
            case 'b':
                *p_blocksize = atoi(optarg);
                if (*p_num_blocksY <= 0) {
                    printf("BLOCKSIZE (-b) must be a positive integer.\n");
                    return -1;
                }
                break;
            case 'i':
                *p_num_iters = atoi(optarg);
                if (*p_num_iters < 0) {
                    printf("NUM_ITERS (-i) must be a non-negative integer.\n");
                    return -1;
                }
                break;
            case 'e':
                *p_num_xstreams = atoi(optarg);
                if (*p_num_xstreams <= 0) {
                    printf("NUM_XSTREAMS (-e) must be a positive integer.\n");
                    return -1;
                }
                break;
            case 'v':
                *p_validate = atoi(optarg);
                break;
            case 'h':
            default:
                print_help(argc, argv);
                return -1;
        }
    }
    return 0;
}

static void init_values(double *values, int num_blocksX, int num_blocksY,
                        int blocksize)
{
    const int width = num_blocksX * blocksize + 2;
    const int height = num_blocksY * blocksize + 2;
    const double coeff = 1.0 / RAND_MAX;
    srand(width * height);
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            values[x + y * width] = rand() * coeff;
        }
    }
}

static int validate_values(const double *values, int num_blocksX,
                           int num_blocksY, int blocksize, int num_iters)
{
    const int width = num_blocksX * blocksize + 2;
    const int height = num_blocksY * blocksize + 2;
    /* Compute the answer in a sequential manner. */
    double *ans_old = (double *)malloc(sizeof(double) * width * height);
    double *ans_new = (double *)malloc(sizeof(double) * width * height);
    init_values(ans_old, num_blocksX, num_blocksY, blocksize);
    for (int t = 0; t < num_iters; t++) {
        for (int y = 1; y < height - 1; y++) {
            for (int x = 1; x < width - 1; x++) {
                ans_new[x + y * width] = ans_old[x + y * width] * (1.0 / 2.0) +
                                         (ans_old[(x + 1) + y * width] +
                                          ans_old[(x - 1) + y * width] +
                                          ans_old[x + (y + 1) * width] +
                                          ans_old[x + (y - 1) * width]) *
                                             (1.0 / 8.0);
            }
        }
        double *ans_tmp = ans_new;
        ans_new = ans_old;
        ans_old = ans_tmp;
    }
    /* Compare the results. */
    int num_failures = 0;
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            double value = values[x + y * width];
            double ans = ans_old[x + y * width];
            double diff = value - ans;
            if (diff > ERROR_TRESHOLD || diff < -ERROR_TRESHOLD) {
                printf("value[%d, %d] (= %f) != ans[%d, %d] (= %f)\n", x, y,
                       value, x, y, ans);
                num_failures++;
                if (num_failures >= 10) {
                    goto END_FOR_LOOPS;
                }
            }
        }
    }
END_FOR_LOOPS:
    free(ans_old);
    free(ans_new);
    return num_failures == 0 ? 0 : -1;
}

#endif /* __STENCIL_HELPER_H__ */
