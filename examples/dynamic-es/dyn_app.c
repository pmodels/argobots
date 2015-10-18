/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <poll.h>
#include <assert.h>

#include "abt.h"
#include "common.h"


static struct pollfd abt_pfd;
static int abt_alive = 0;

static ABT_mutex g_mutex = ABT_MUTEX_NULL;
static ABT_xstream *g_xstreams = NULL;
static ABT_pool *g_pools = NULL;
static int *g_signal = NULL;

static int max_xstreams;
static int num_xstreams;
static double g_timeout = 2.0;
static volatile int g_running = 1;


static void abt_connect(char *host_str, char *port_str);
static void abt_disconnect(void);
static void abt_check_events(int idx);
static void increase_xstream(void);
static void decrease_xstream(void);
static void thread_join_xstream(void *arg);
static void send_num_xstream(void);
static void init(int num_xstreams);
static void finalize(void);
static void create_xstream(int idx);
static void thread_add_sched(void *arg);
static void thread_work(void *arg);
static void thread_hello(void *arg);


/* scheduler data structure and functions */
typedef struct {
    uint32_t event_freq;
    int idx;
} sched_data_t;

static inline sched_data_t *sched_data_get_ptr(void *data)
{
    return (sched_data_t *)data;
}

static int sched_init(ABT_sched sched, ABT_sched_config config)
{
    int ret = ABT_SUCCESS;

    sched_data_t *p_data = (sched_data_t *)calloc(1, sizeof(sched_data_t));

    /* Set the variables from the config */
    ABT_sched_config_read(config, 2, &p_data->event_freq, &p_data->idx);

    ret = ABT_sched_set_data(sched, (void *)p_data);

    return ret;
}

static void sched_run(ABT_sched sched)
{
    uint32_t work_count = 0;
    void *data;
    sched_data_t *p_data;
    ABT_pool my_pool;
    size_t size;
    ABT_unit unit;
    uint32_t event_freq;
    int num_pools;
    ABT_pool *pools;
    int target;
    unsigned seed = time(NULL);

    ABT_sched_get_data(sched, &data);
    p_data = sched_data_get_ptr(data);
    event_freq = p_data->event_freq;

    ABT_sched_get_num_pools(sched, &num_pools);

    pools = (ABT_pool *)malloc(sizeof(ABT_pool) * num_pools);
    ABT_sched_get_pools(sched, num_pools, 0, pools);
    my_pool = pools[0];

    while (1) {
        /* Execute one work unit from the scheduler's pool */
        ABT_pool_pop(my_pool, &unit);
        if (unit != ABT_UNIT_NULL) {
            ABT_xstream_run_unit(unit, my_pool);
        } else if (num_pools > 1) {
            /* Steal a work unit from other pools */
            target = (num_pools == 2) ? 1 : (rand_r(&seed) % (num_pools-1) + 1);
            ABT_pool tar_pool = pools[target];
            ABT_pool_get_size(tar_pool, &size);
            if (size > 0) {
                /* Pop one work unit */
                ABT_pool_pop(tar_pool, &unit);
                if (unit != ABT_UNIT_NULL) {
                    ABT_xstream_run_unit(unit, tar_pool);
                }
            }
        }

        if (++work_count >= event_freq) {
            abt_check_events(p_data->idx);

            ABT_bool stop;
            ABT_sched_has_to_stop(sched, &stop);
            if (stop == ABT_TRUE) break;
            work_count = 0;
            ABT_xstream_check_events(sched);
        }
    }

    free(pools);
}

static int sched_free(ABT_sched sched)
{
    int ret = ABT_SUCCESS;
    void *data;

    ABT_sched_get_data(sched, &data);
    sched_data_t *p_data = sched_data_get_ptr(data);
    free(p_data);

    return ret;
}

int main(int argc, char *argv[])
{
    if (argc != 5) {
        printf("Usage: %s <max # of ESs> <timeout> <server> <port>\n", argv[0]);
        return EXIT_SUCCESS;
    }

    max_xstreams = atoi(argv[1]);
    if (max_xstreams < 1) {
        max_xstreams = 1;
    }
    num_xstreams = max_xstreams;

    g_timeout = atof(argv[2]);

    printf("# of ESs  : %d\n", max_xstreams);
    printf("Timeout   : %.1f sec.\n", g_timeout);

    abt_connect(argv[3], argv[4]);

    /* Initialize */
    ABT_init(argc, argv);

    init(num_xstreams);

    thread_add_sched((void *)0);

    finalize();

    /* Finalize */
    ABT_finalize();

    abt_disconnect();

    printf("Done.\n"); fflush(stdout);

    return EXIT_SUCCESS;
}

static void handle_error(const char *msg)
{
    perror(msg);
    exit(EXIT_FAILURE);
}

static void abt_connect(char *host_str, char *port_str)
{
    int sockfd;
    struct sockaddr_in serv_addr;
    struct hostent *server;
    int ret;

    server = gethostbyname(host_str);
    if (server == NULL) {
        fprintf(stderr,"ERROR: no such host (%s)\n", host_str);
        exit(0);
    }

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) handle_error("ERROR: socket");

    bzero((char *)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr,
          server->h_length);
    serv_addr.sin_port = htons(atoi(port_str));

    ret = connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    if (ret < 0) {
        /* handle_error("ERROR: connect"); */
        return;
    }
    abt_pfd.fd = sockfd;
    abt_pfd.events = POLLIN | POLLHUP;
    abt_alive = 1;

    printf("Connected...\n");
}

static void abt_disconnect(void)
{
    static int called = 0;

    if (called || !abt_alive) return;
    called = 1;

    close(abt_pfd.fd);
    printf("Disonnected...\n");

    abt_alive = 0;
}

static void abt_check_events(int idx)
{
    int n, ret;
    char recv_buf[RECV_BUF_LEN];

    if (!abt_alive) return;

    ret = ABT_mutex_trylock(g_mutex);
    if (ret == ABT_ERR_MUTEX_LOCKED) return;
    assert(ret == ABT_SUCCESS);

    ret = poll(&abt_pfd, 1, 1);
    if (ret == -1) {
        handle_error("ERROR: poll");
    } else if (ret != 0) {
        if (abt_pfd.revents & POLLIN) {
            bzero(recv_buf, RECV_BUF_LEN);
            n = read(abt_pfd.fd, recv_buf, RECV_BUF_LEN);
            if (n < 0) handle_error("ERROR: read");

            printf("\nES%d: received request '%c'\n", idx, recv_buf[0]);
            switch (recv_buf[0]) {
                case 'd': decrease_xstream(); break;
                case 'i': increase_xstream(); break;
                case 'n': send_num_xstream(); break;
                case 'q': abt_disconnect(); break;
                default:
                    printf("Unknown commend: %s\n", recv_buf);
                    break;
            }
        }

        if (abt_pfd.revents & POLLHUP) {
            abt_alive = 0;
            printf("Server disconnected...\n");
        }
        abt_pfd.revents = 0;
    }

    ABT_mutex_unlock(g_mutex);
}

static void increase_xstream(void)
{
    char send_buf[SEND_BUF_LEN];
    int n;

    if (num_xstreams >= max_xstreams) {
        printf("cannot increase # of ESs\n");
        sprintf(send_buf, "max");
    } else {
        create_xstream(num_xstreams++);
        printf("# of ESs: %d\n", num_xstreams);
        sprintf(send_buf, "done (%d)", num_xstreams);
    }

    n = write(abt_pfd.fd, send_buf, strlen(send_buf));
    assert(n == strlen(send_buf));
}

static void decrease_xstream(void)
{
    char send_buf[SEND_BUF_LEN];
    int n;

    if (num_xstreams <= 1) {
        printf("cannot decrease # of ESs\n");
        sprintf(send_buf, "min");
        n = write(abt_pfd.fd, send_buf, strlen(send_buf));
        assert(n == strlen(send_buf));
        return;
    } else {
        ABT_thread_create(g_pools[0], thread_join_xstream, NULL,
                          ABT_THREAD_ATTR_NULL, NULL);
    }
}

static void thread_join_xstream(void *arg)
{
    char send_buf[SEND_BUF_LEN];
    int idx = num_xstreams - 1;
    int n;
    ABT_xstream_state state;

    g_signal[idx] = 1;
    while (1) {
        ABT_xstream_get_state(g_xstreams[idx], &state);
        if (state == ABT_XSTREAM_STATE_TERMINATED) break;
        ABT_thread_yield();
    }
    ABT_xstream_free(&g_xstreams[idx]);
    num_xstreams--;

    if (abt_alive) {
        printf("# of ESs: %d\n", num_xstreams);
        sprintf(send_buf, "done (%d)", num_xstreams);
        n = write(abt_pfd.fd, send_buf, strlen(send_buf));
        assert(n == strlen(send_buf));
    }
}

static void send_num_xstream(void)
{
    char send_buf[SEND_BUF_LEN];
    int num_xstreams;
    int n;

    ABT_xstream_get_num(&num_xstreams);

    sprintf(send_buf, "%d", num_xstreams);
    n = write(abt_pfd.fd, send_buf, strlen(send_buf));
    assert(n == strlen(send_buf));
}

static void init(int num_xstreams)
{
    int i;

    ABT_mutex_create(&g_mutex);

    g_xstreams = (ABT_xstream *)malloc(sizeof(ABT_xstream) * max_xstreams);
    g_pools = (ABT_pool *)malloc(sizeof(ABT_pool) * max_xstreams);
    g_signal = (int *)calloc(max_xstreams, sizeof(int));

    for (i = 0; i < max_xstreams; i++) {
        g_xstreams[i] = ABT_XSTREAM_NULL;
        g_pools[i] = ABT_POOL_NULL;
    }

    /* Create pools */
    for (i = 0; i < max_xstreams; i++) {
        ABT_pool_create_basic(ABT_POOL_FIFO, ABT_POOL_ACCESS_MPMC, ABT_TRUE,
                              &g_pools[i]);
    }

    /* Create ESs */
    ABT_xstream_self(&g_xstreams[0]);
    for (i = 1; i < num_xstreams; i++) {
        create_xstream(i);
    }
}

static void finalize(void)
{
    int i;

    for (i = 1; i < max_xstreams; i++) {
        if (g_xstreams[i] != ABT_XSTREAM_NULL) {
            ABT_xstream_free(&g_xstreams[i]);
        }
    }

    ABT_mutex_free(&g_mutex);
    free(g_pools);
    free(g_xstreams);
}

static void create_xstream(int idx)
{
    ABT_pool pool;

    ABT_xstream_create(ABT_SCHED_NULL, &g_xstreams[idx]);
    ABT_xstream_get_main_pools(g_xstreams[idx], 1, &pool);

    ABT_thread_create(pool, thread_add_sched, (void *)(intptr_t)idx,
                      ABT_THREAD_ATTR_NULL, NULL);
}

/* Create a work-stealing scheduler and push it to the pool */
static void thread_add_sched(void *arg)
{
    int idx = (int)(intptr_t)arg;
    int i;
    ABT_thread cur_thread;
    ABT_pool cur_pool;
    ABT_pool *my_pools;
    ABT_sched_config config;
    ABT_sched sched;
    size_t size;
    double t_start, t_end;

    ABT_sched_config_var cv_event_freq = {
        .idx = 0,
        .type = ABT_SCHED_CONFIG_INT
    };

    ABT_sched_config_var cv_idx = {
        .idx = 1,
        .type = ABT_SCHED_CONFIG_INT
    };

    ABT_sched_def sched_def = {
        .type = ABT_SCHED_TYPE_ULT,
        .init = sched_init,
        .run = sched_run,
        .free = sched_free,
        .get_migr_pool = NULL
    };

    /* Create a scheduler */
    ABT_sched_config_create(&config,
                            cv_event_freq, 10,
                            cv_idx, idx,
                            ABT_sched_config_var_end);
    my_pools = (ABT_pool *)malloc(sizeof(ABT_pool) * max_xstreams);
    for (i = 0; i < max_xstreams; i++) {
        my_pools[i] = g_pools[(idx + i) % max_xstreams];
    }
    ABT_sched_create(&sched_def, max_xstreams, my_pools, config, &sched);

    /* Create a ULT for the new scheduler */
    ABT_thread_create(my_pools[0], thread_work, arg, ABT_THREAD_ATTR_NULL,
                      NULL);

    /* Push the scheduler to the current pool */
    ABT_thread_self(&cur_thread);
    ABT_thread_get_last_pool(cur_thread, &cur_pool);
    ABT_pool_add_sched(cur_pool, sched);

    /* Free */
    ABT_sched_config_free(&config);
    free(my_pools);

    t_start = ABT_get_wtime();
    while (1) {
        ABT_thread_yield();

        ABT_pool_get_total_size(cur_pool, &size);
        if (size == 0) {
            ABT_sched_free(&sched);
            break;
        }

        t_end = ABT_get_wtime();
        if ((t_end - t_start) > g_timeout) {
            ABT_sched_finish(sched);
        }
    }
}

static void thread_work(void *arg)
{
    int idx = (int)(intptr_t)arg;
    int i;
    ABT_thread cur_thread;
    ABT_pool cur_pool;
    ABT_thread *threads;
    int num_threads;
    double t_start, t_end;

    ABT_thread_self(&cur_thread);
    ABT_thread_get_last_pool(cur_thread, &cur_pool);

    t_start = ABT_get_wtime();
    while (1) {
        num_threads = 2;
        threads = (ABT_thread *)malloc(sizeof(ABT_thread) * num_threads);
        for (i = 0; i < num_threads; i++) {
            ABT_thread_create(cur_pool, thread_hello, NULL,
                              ABT_THREAD_ATTR_NULL, &threads[i]);
        }
        for (i = 0; i < num_threads; i++) {
            ABT_thread_free(&threads[i]);
        }
        free(threads);

        if (g_signal[idx]) {
            ABT_xstream xstream;
            ABT_xstream_self(&xstream);
            ABT_xstream_cancel(xstream);
            g_signal[idx] = 0;
            break;
        }

        t_end = ABT_get_wtime();
        if ((t_end - t_start) > g_timeout) {
            break;
        }
    }
}

static void test_printf(const char *format, ...)
{
#if 0
    va_start(list, format);
    vprintf(format, list);
    va_end(list);
    fflush(stdout);
#endif
}

static void thread_hello(void *arg)
{
    int old_rank, cur_rank;
    ABT_thread self;
    ABT_thread_id id;
    char *msg;

    ABT_xstream_self_rank(&cur_rank);
    ABT_thread_self(&self);
    ABT_thread_get_id(self, &id);

    test_printf("[U%lu:E%d] Hello, world!\n", id, cur_rank);

    ABT_thread_yield();

    old_rank = cur_rank;
    ABT_xstream_self_rank(&cur_rank);
    msg = (cur_rank == old_rank) ? "" : " (stolen)";
    test_printf("[U%lu:E%d] Hello again #1.%s\n", id, cur_rank, msg);

    ABT_thread_yield();

    old_rank = cur_rank;
    ABT_xstream_self_rank(&cur_rank);
    msg = (cur_rank == old_rank) ? "" : " (stolen)";
    test_printf("[U%lu:E%d] Hello again #2.%s\n", id, cur_rank, msg);

    ABT_thread_yield();

    old_rank = cur_rank;
    ABT_xstream_self_rank(&cur_rank);
    msg = (cur_rank == old_rank) ? "" : " (stolen)";
    test_printf("[U%lu:E%d] Goodbye, world!%s\n", id, cur_rank, msg);
}

