
//
// How to compile?
// gcc -O3 strassen.c -lm
// clang -O3 strassen_thread.c -lm -I/homes/USERNAME/argobots-install/include -L/homes/USERNAME/argobots-install/lib -labt -o strassen_thread
// export ABT_THREAD_STACKSIZE=128000
// Further possible improvement:
// - Use BLAS (e.g., MKL)
// - Better memory management (in-place etc)
// - Better scheduling (locality-aware etc)
// - Better parallelization strategy
//

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <string.h>
#include <sys/time.h>
#include <abt.h>

#define NUM_XSTREAMS 1
#define NUM_INNER_XSTREAMS 1
#define NUM_THREADS 1

/* global */
ABT_pool g_pool = ABT_POOL_NULL;
ABT_pool inner_pool = ABT_POOL_NULL;
typedef double real_t;
static int num_inner_xstreams;

typedef struct {
    int64_t n;
    int64_t dn;
    real_t * a;
    real_t * b;
    real_t * c;
} thread_args;

typedef struct {
  const real_t * restrict a; 
  const real_t * restrict b;
  real_t * restrict c;
  int64_t di;
  int64_t dj_start;
  int64_t dj_end;
  int64_t dk;
  int64_t aN;
  int64_t bN;
  int64_t cN;
} matmul_args;

static inline double get_time()
{
  struct timeval tv;
  gettimeofday(&tv,0);
  return tv.tv_sec + (double)tv.tv_usec * 1e-6;
}

int64_t g_padding = 0;
int64_t g_cutoff = 0;

int init_abt(int num_xstreams, ABT_xstream *xstreams, ABT_pool *g_pool) {
    int set_main_sched_err;
    int initialized = ABT_initialized() != ABT_ERR_UNINITIALIZED;
    /* initialize argobots */
    ABT_init(0, NULL);

    /* create a scheduler pool? */
    ABT_pool_create_basic(ABT_POOL_FIFO, ABT_POOL_ACCESS_MPMC, ABT_TRUE, g_pool);
    ////////////////////////////////////////////////////////////////////////////

    ABT_xstream_self(&xstreams[0]);
    if(initialized)
	set_main_sched_err = -1;
    else
    	set_main_sched_err = ABT_xstream_set_main_sched_basic(xstreams[0], ABT_SCHED_DEFAULT, 1, g_pool);
	
    int start_i = (set_main_sched_err != ABT_SUCCESS) ? 0 : 1;
	
    for(int i = start_i; i < num_xstreams; i++) {
	ABT_xstream_create_basic(ABT_SCHED_DEFAULT, 1, g_pool, ABT_SCHED_CONFIG_NULL, &xstreams[i]);
	ABT_xstream_start(xstreams[i]);
    }

    return start_i;
}

void matmul_handler(void* data) {
  matmul_args *args = (matmul_args*)data;

    for (int64_t j = args->dj_start; j < args->dj_end; j++)
        for (int64_t i = 0; i < args->di; i++)
            for (int64_t k = 0; k < args->dk; k++)
                args->c[i + j * args->cN] += args->a[k + j * args->aN] * args->b[i + k * args->bN];
}

void matmul(const real_t * restrict a, const real_t * restrict b,
            real_t * restrict c, int64_t di, int64_t dj, int64_t dk, int64_t aN,
            int64_t bN, int64_t cN)
{
    	    printf("# of Inner ESs: %d\n", num_inner_xstreams);
            ABT_xstream *xstreams;
	 
	    xstreams = (ABT_xstream *)malloc(sizeof(ABT_xstream) * num_inner_xstreams);
 
	    int start_i = init_abt(num_inner_xstreams, xstreams, &inner_pool);
	    int iter = dj/NUM_THREADS;
	    int leftover = dj % NUM_THREADS;
	    ABT_thread *threads = malloc(sizeof(ABT_thread) * NUM_THREADS);
	
	for (int i = 0; i < NUM_THREADS; i++) {
 	   matmul_args *args = (matmul_args*)malloc(sizeof(thread_args));
	   args->a = a;
	   args->b = b;
	   args->c = c;
	   args->di = di;
	   args->dj_start = i*iter;
	   args->dj_end = args->dj_start + iter - 1;
	   if(i == NUM_THREADS - 1) args->dj_end = args->dj_end + leftover;
	   args->dk = dk;
	   args->aN = aN;
	   args->bN = bN;
	   args->cN = cN;

	   ABT_thread_create(inner_pool, matmul_handler, args, ABT_THREAD_ATTR_NULL, &threads[i]);
	}

	for(int i = 0; i < NUM_THREADS; i++) {
	    ABT_thread_join(threads[i]);
	    ABT_thread_free(&threads[i]);
	}
	
	for(int j = start_i; j < num_inner_xstreams; j++) {
	      ABT_xstream_join(xstreams[j]);
	      ABT_xstream_free(&xstreams[j]);
	}

	free(threads);
	free(xstreams);
	ABT_finalize();
}

void matadd(const real_t * restrict a, const real_t * restrict b,
            real_t * restrict c, int64_t di, int64_t dj, int64_t aN, int64_t bN,
            int64_t cN)
{
    #pragma omp parallel for
    for (int64_t j = 0; j < dj; j++)
        for (int64_t i = 0; i < di; i++)
            c[i + j * cN] = a[i + j * aN] + b[i + j * bN];
}

void mataddaddminadd(const real_t * restrict a1, const real_t * restrict a2,
                     const real_t * restrict a3, const real_t * restrict a4,
                     real_t * restrict c, int64_t di, int64_t dj, int64_t aN,
                     int64_t cN)
{
    #pragma omp parallel for
    for (int64_t j = 0; j < dj; j++)
        for (int64_t i = 0; i < di; i++)
            c[i + j * cN] = a1[i + j * aN] + a2[i + j * aN] - a3[i + j * aN]
                            + a4[i + j * aN];
}

void matmin(const real_t * restrict a, const real_t * restrict b,
            real_t * restrict c, int64_t di, int64_t dj, int64_t aN, int64_t bN,
            int64_t cN)
{
    #pragma omp parallel for
    for (int64_t j = 0; j < dj; j++)
        for (int64_t i = 0; i < di; i++)
            c[i + j * cN] = a[i + j * aN] - b[i + j * bN];
}

void matassign(const real_t * restrict a, real_t * restrict c,
               int64_t di, int64_t dj, int64_t aN, int64_t cN)
{
    #pragma omp parallel for
    for (int64_t j = 0; j < dj; j++)
        memcpy(&c[j * cN], &a[j * aN], sizeof(real_t) * di);
}

void strassen(const real_t *a,const real_t *b, real_t *c, int64_t dn,
              int64_t n)
{
    if (dn <= g_cutoff) {
        matmul(a, b, c, dn, dn, dn, n, n, n);
    } else {
        const real_t *A[4];
        const real_t *B[4];
        real_t *C[4];
        A[0] = a;
        A[1] = a + dn / 2;
        A[2] = a + n * dn / 2;
        A[3] = a + n * dn / 2 + dn / 2;
        B[0] = b;
        B[1] = b + dn / 2;
        B[2] = b + n * dn / 2;
        B[3] = b + n * dn / 2 + dn / 2;
        C[0] = c;
        C[1] = c + dn / 2;
        C[2] = c + n * dn / 2;
        C[3] = c + n * dn / 2 + dn / 2;

        int Sn = dn / 2 + g_padding;
        real_t* S[14];
        real_t* Sbuf = (real_t *)malloc(sizeof(real_t) * Sn * dn / 2 * 14);
        ////////////////////////////////////////////////////////////////////////
        // Preprocessing
        for(int i = 0; i < 14; i++)
            S[i] = Sbuf + Sn * (dn / 2) * i;
        const real_t *A11 = A[0];
        const real_t *A12 = A[1];
        const real_t *A21 = A[2];
        const real_t *A22 = A[3];
        const real_t *B11 = B[0];
        const real_t *B12 = B[1];
        const real_t *B21 = B[2];
        const real_t *B22 = B[3];
        // S1  = A11 + A22
        // S2  = B11 + B22
        // S3  = A21 + A22
        // S4  = B11
        // S5  = A11
        // S6  = B12 - B22
        // S7  = A22
        // S8  = B21 - B11
        // S9  = A11 + A12
        // S10 = B22
        // S11 = A21 - A11
        // S12 = B11 + B12
        // S13 = A12 - A22
        // S14 = B21 + B22
        matadd(A11, A22, S[0],  dn / 2, dn / 2, n, n, Sn);
        matadd(A11, A22, S[0],  dn / 2, dn / 2, n, n, Sn);
        matadd(B11, B22, S[1],  dn / 2, dn / 2, n, n, Sn);
        matadd(A21, A22, S[2],  dn / 2, dn / 2, n, n, Sn);
        matassign(B11,   S[3],  dn / 2, dn / 2, n, Sn);
        matassign(A11,   S[4],  dn / 2, dn / 2, n, Sn);
        matmin(B12, B22, S[5],  dn / 2, dn / 2, n, n, Sn);
        matassign(A22,   S[6],  dn / 2, dn / 2, n, Sn);
        matmin(B21, B11, S[7],  dn / 2, dn / 2, n, n, Sn);
        matadd(A11, A12, S[8],  dn / 2, dn / 2, n, n, Sn);
        matassign(B22,   S[9],  dn / 2, dn / 2, n, Sn);
        matmin(A21, A11, S[10], dn / 2, dn / 2, n, n, Sn);
        matadd(B11, B12, S[11], dn / 2, dn / 2, n, n, Sn);
        matmin(A12, A22, S[12], dn / 2, dn / 2, n, n, Sn);
        matadd(B21, B22, S[13], dn / 2, dn / 2, n, n, Sn);

        // Kernel
        real_t* P[7];
        real_t* Pbuf = (real_t *)calloc(Sn * dn / 2 * 7, sizeof(real_t *));
        for (int i = 0; i < 7; i++)
            P[i] = Pbuf + Sn * dn / 2 * i;
        // P1 = S1*S2, P2 = S3*S4, ...
        for (int i = 0; i < 7; i++)
            strassen(S[i * 2], S[i * 2 + 1], P[i], dn / 2, Sn);

        ////////////////////////////////////////////////////////////////////////
        // Postprocessing
        // C11 = P1 + P4 - P5 + P7
        // C12 = P3 + P5
        // C21 = P2 + P4
        // C22 = P1 + P3 - P2 + P6
        const real_t *P1 = P[0];
        const real_t *P2 = P[1];
        const real_t *P3 = P[2];
        const real_t *P4 = P[3];
        const real_t *P5 = P[4];
        const real_t *P6 = P[5];
        const real_t *P7 = P[6];
        real_t *C11 = C[0];
        real_t *C12 = C[1];
        real_t *C21 = C[2];
        real_t *C22 = C[3];
        mataddaddminadd(P1, P4, P5, P7, C11, dn / 2, dn / 2, Sn, n);
        matadd(P3, P5, C12, dn / 2, dn / 2, Sn, Sn, n);
        matadd(P2, P4, C21, dn / 2, dn / 2, Sn, Sn, n);
        mataddaddminadd(P1, P3, P2, P6, C22, dn / 2, dn / 2, Sn, n);

        free(Sbuf);
        free(Pbuf);
    }
}

void strassen_thread(void *args) 
{
    thread_args *arguments = (thread_args*)args;
    int64_t n = arguments->n;
    int64_t dn = arguments->dn;
    real_t *a = arguments->a;
    real_t *b = arguments->b;
    real_t *c = arguments->c;

    if (dn <= g_cutoff) {
        matmul(a, b, c, dn, dn, dn, n, n, n);
    } else {
        const real_t *A[4];
        const real_t *B[4];
        real_t *C[4];
        A[0] = a;
        A[1] = a + dn / 2;
        A[2] = a + n * dn / 2;
        A[3] = a + n * dn / 2 + dn / 2;
        B[0] = b;
        B[1] = b + dn / 2;
        B[2] = b + n * dn / 2;
        B[3] = b + n * dn / 2 + dn / 2;
        C[0] = c;
        C[1] = c + dn / 2;
        C[2] = c + n * dn / 2;
        C[3] = c + n * dn / 2 + dn / 2;

        int Sn = dn / 2 + g_padding;
        real_t* S[14];
        real_t* Sbuf = (real_t *)malloc(sizeof(real_t) * Sn * dn / 2 * 14);
        ////////////////////////////////////////////////////////////////////////
        // Preprocessing
        for(int i = 0; i < 14; i++)
            S[i] = Sbuf + Sn * (dn / 2) * i;
        const real_t *A11 = A[0];
        const real_t *A12 = A[1];
        const real_t *A21 = A[2];
        const real_t *A22 = A[3];
        const real_t *B11 = B[0];
        const real_t *B12 = B[1];
        const real_t *B21 = B[2];
        const real_t *B22 = B[3];
        // S1  = A11 + A22
        // S2  = B11 + B22
        // S3  = A21 + A22
        // S4  = B11
        // S5  = A11
        // S6  = B12 - B22
        // S7  = A22
        // S8  = B21 - B11
        // S9  = A11 + A12
        // S10 = B22
        // S11 = A21 - A11
        // S12 = B11 + B12
        // S13 = A12 - A22
        // S14 = B21 + B22
        matadd(A11, A22, S[0],  dn / 2, dn / 2, n, n, Sn);
        matadd(A11, A22, S[0],  dn / 2, dn / 2, n, n, Sn);
        matadd(B11, B22, S[1],  dn / 2, dn / 2, n, n, Sn);
        matadd(A21, A22, S[2],  dn / 2, dn / 2, n, n, Sn);
        matassign(B11,   S[3],  dn / 2, dn / 2, n, Sn);
        matassign(A11,   S[4],  dn / 2, dn / 2, n, Sn);
        matmin(B12, B22, S[5],  dn / 2, dn / 2, n, n, Sn);
        matassign(A22,   S[6],  dn / 2, dn / 2, n, Sn);
        matmin(B21, B11, S[7],  dn / 2, dn / 2, n, n, Sn);
        matadd(A11, A12, S[8],  dn / 2, dn / 2, n, n, Sn);
        matassign(B22,   S[9],  dn / 2, dn / 2, n, Sn);
        matmin(A21, A11, S[10], dn / 2, dn / 2, n, n, Sn);
        matadd(B11, B12, S[11], dn / 2, dn / 2, n, n, Sn);
        matmin(A12, A22, S[12], dn / 2, dn / 2, n, n, Sn);
        matadd(B21, B22, S[13], dn / 2, dn / 2, n, n, Sn);

        ////////////////////////////////////////////////////////////////////////
        // Kernel
        real_t* P[7];
        real_t* Pbuf = (real_t *)calloc(Sn * dn / 2 * 7, sizeof(real_t *));
        for (int i = 0; i < 7; i++)
            P[i] = Pbuf + Sn * dn / 2 * i;
        // P1 = S1*S2, P2 = S3*S4, ...
        ABT_thread *threads = malloc(sizeof(ABT_thread) * 7);
	
	for (int i = 0; i < 7; i++) {
 	   thread_args *args = (thread_args*)malloc(sizeof(thread_args));
	   args->n = Sn;
	   args->dn = dn/2;
	   args->a = S[i * 2];
	   args->b = S[i * 2 + 1];
	   args->c = P[i];
	   ABT_thread_create(g_pool, strassen_thread, args, ABT_THREAD_ATTR_NULL, &threads[i]);
	   //strassen(S[i * 2], S[i * 2 + 1], P[i], dn / 2, Sn);
	}

	for(int i = 0; i < 7; i++) {
	    ABT_thread_join(threads[i]);
	    ABT_thread_free(&threads[i]);
	}

        ////////////////////////////////////////////////////////////////////////
        // Postprocessing
        // C11 = P1 + P4 - P5 + P7
        // C12 = P3 + P5
        // C21 = P2 + P4
        // C22 = P1 + P3 - P2 + P6
        const real_t *P1 = P[0];
        const real_t *P2 = P[1];
        const real_t *P3 = P[2];
        const real_t *P4 = P[3];
        const real_t *P5 = P[4];
        const real_t *P6 = P[5];
        const real_t *P7 = P[6];
        real_t *C11 = C[0];
        real_t *C12 = C[1];
        real_t *C21 = C[2];
        real_t *C22 = C[3];
        mataddaddminadd(P1, P4, P5, P7, C11, dn / 2, dn / 2, Sn, n);
        matadd(P3, P5, C12, dn / 2, dn / 2, Sn, Sn, n);
        matadd(P2, P4, C21, dn / 2, dn / 2, Sn, Sn, n);
        mataddaddminadd(P1, P3, P2, P6, C22, dn / 2, dn / 2, Sn, n);

        free(Sbuf);
        free(Pbuf);
    }
	
}

int main(int argc, char *argvs[])
{
    if (argc < 5) {
        printf("Usage: MATSIZE_N NREPEATS G_PADDING G_CUTOFF\n");
        printf("ex: ./strassen 1024 1 16 128\n");
        return -1;
    }
    int num_xstreams = (argc > 5) ? atoi(argvs[5]) : NUM_XSTREAMS;
    num_inner_xstreams = (argc > 6) ? atoi(argvs[6]) : NUM_INNER_XSTREAMS;

    int64_t dn = atoi(argvs[1]);
    int64_t num_repeats = atoi(argvs[2]);
    g_padding = atoi(argvs[3]);
    g_cutoff = atoi(argvs[4]);
    if (dn & (dn - 1)) {
        printf("MATSIZE_N must be a power of 2.\n");
        return -1;
    }

    // Setup
    int64_t n = dn + g_padding;
    real_t *a = (real_t *)malloc(sizeof(real_t) * dn * n);
    real_t *b = (real_t *)malloc(sizeof(real_t) * dn * n);
    real_t *c = (real_t *)malloc(sizeof(real_t) * dn * n);
    for (int64_t j = 0; j < n; j++) {
        for (int64_t i = 0; i < n; i++) {
            // Any random values
            a[i + j* dn] = sin((double)(i + j));
            b[i + j* dn] = cos((double)(i + 2 * j));
            c[i + j* dn] = 0.0;
        }
    }
    if (num_repeats == 0) {
        // Error check.
        printf("Performing strassen's MM ...\n");
	strassen(a, b, c, dn, n);
	printf("... Done\n");
        real_t *ans = (real_t *)calloc(n * dn, sizeof(real_t));
	printf("Performing naive MM ...\n");
        matmul(a, b, ans, dn, dn, dn, n, n, n);
	printf("... Done\n");
        int num_fails = 0;
        for (int64_t j = 0; j < dn; j++) {
            for (int64_t i = 0; i < dn; i++) {
                printf("Different: c[%d,%d] (=%f) != %f\n", (int)i, (int)j,
                       c[i + j * n], ans[i + j * n]);
                if (fabs(c[i + j * n] - ans[i + j * n]) > (real_t)0.0001) {
                    printf("Different: c[%d,%d] (=%f) != %f\n", (int)i, (int)j,
                           c[i + j * n], ans[i + j * n]);
                    if (num_fails++ > 6)
                        goto END_LOOP;
                }
            }
        }
        printf("No error\n");
END_LOOP:
        free(ans);
        return 0;
    } else {
        // Performance check.
        for (int i = 0; i < num_repeats; i++) {
	    ABT_thread thread;
	    printf("# of ESs: %d\n", num_xstreams);
	    ABT_xstream *xstreams;
	    thread_args args_thread;
	 
	    xstreams = (ABT_xstream *)malloc(sizeof(ABT_xstream) * num_xstreams);
 
	    int j = init_abt(num_xstreams, xstreams, &g_pool);
            double t1 = get_time();
            args_thread.a = a;
            args_thread.b = b;
            args_thread.c = c;
            args_thread.dn = dn;
            args_thread.n = n;
            ABT_thread_create(g_pool, strassen_thread, &args_thread, ABT_THREAD_ATTR_NULL, &thread);
	    ABT_thread_join(thread);
            
	    double t2 = get_time();
            printf("[%d] Elapsed: %f [s]\n", i, t2 - t1);
	    ABT_thread_free(&thread);

	    while(j < num_xstreams) {
	      ABT_xstream_join(xstreams[j]);
	      ABT_xstream_free(&xstreams[j]);
	      j++;
	    }
	    free(xstreams);
	    ABT_finalize();
        }
    }
    return 0;
}
