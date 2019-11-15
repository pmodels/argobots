/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include "abti.h"
#include <unistd.h>
#include <strings.h>

#ifdef ABT_CONFIG_HANDLE_POWER_EVENT
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <poll.h>

#define ABTI_DEFAULT_MAX_CB_FN  4
#define ABTI_MSG_BUF_LEN        20
#endif

#ifdef ABT_CONFIG_PUBLISH_INFO
#include <stdio.h>

#ifdef HAVE_BEACON_H
/* To suppress warnings due to same macro names generated by autotools */
#ifdef PACKAGE
#undef PACKAGE
#endif
#ifdef PACKAGE_BUGREPORT
#undef PACKAGE_BUGREPORT
#endif
#ifdef PACKAGE_NAME
#undef PACKAGE_NAME
#endif
#ifdef PACKAGE_STRING
#undef PACKAGE_STRING
#endif
#ifdef PACKAGE_TARNAME
#undef PACKAGE_TARNAME
#endif
#ifdef PACKAGE_VERSION
#undef PACKAGE_VERSION
#endif
#ifdef VERSION
#undef VERSION
#endif

#include <beacon.h>
#endif

#if defined(HAVE_RAPLREADER_H) && defined(HAVE_LIBINTERCOOLR)
#include <raplreader.h>
#define RAPLREADER_INIT(p_rr)       raplreader_init(p_rr)
#define RAPLREADER_SAMPLE(p_rr)     raplreader_sample(p_rr)
#else
#define RAPLREADER_INIT(p_rr)       (0)
#define RAPLREADER_SAMPLE(p_rr)
#endif

typedef enum {
    ABTI_PUB_TYPE_BEACON,
    ABTI_PUB_TYPE_FILE
} ABTI_pub_type;
#endif

typedef struct ABTI_event_info  ABTI_event_info;

struct ABTI_event_info {
    ABTI_mutex mutex;
    char hostname[100];
    ABT_bool use_debug;

#ifdef ABT_CONFIG_HANDLE_POWER_EVENT
    struct pollfd pfd;

    int max_stop_xstream_fn;
    int num_stop_xstream_fn;
    ABT_event_cb_fn *stop_xstream_fn;
    void **stop_xstream_arg;

    int max_add_xstream_fn;
    int num_add_xstream_fn;
    ABT_event_cb_fn *add_xstream_fn;
    void **add_xstream_arg;
#endif
#ifdef ABT_CONFIG_PUBLISH_INFO
    ABTI_pub_type pub_type;

#ifdef HAVE_BEACON_H
    BEACON_beep_t binfo;
    BEACON_beep_handle_t handle;
    BEACON_topic_info_t *topic_info;
    BEACON_topic_properties_t *eprop;
#endif

    FILE *out_file;

    int max_xstream_rank;
    uint32_t *num_threads;      /* # of ULTs terminated on each ES */
    uint32_t *num_tasks;        /* # of tasklets terminated on each ES */
    double *idle_time;          /* idle time of each ES */
    uint32_t *old_num_units;    /* # of units executed at the last timestamp */
    double *old_timestamp;
    double timestamp;           /* last timestamp */

#if defined(HAVE_RAPLREADER_H) && defined(HAVE_LIBINTERCOOLR)
    struct raplreader rr;       /* For power profiling with RAPL */
#endif
    double prof_start_time;     /* Profiling start time */
    double prof_stop_time;      /* Profiling stop_time */
#endif
};

static ABTI_event_info *gp_einfo = NULL;


#define EVT_DEBUG(fmt,...)                      \
    do {                                        \
        if (gp_einfo->use_debug == ABT_TRUE)    \
            printf(fmt,__VA_ARGS__);            \
    } while (0)


/** @defgroup EVENT Event
 * This group is for event handling.
 */

void ABTI_event_init(void)
{
    char *env;

    gp_ABTI_global->pm_connected = ABT_FALSE;

    gp_einfo = (ABTI_event_info *)ABTU_calloc(1, sizeof(ABTI_event_info));
    ABTI_mutex_init(&gp_einfo->mutex);

    gethostname(gp_einfo->hostname, 100);

    env = getenv("ABT_USE_EVENT_DEBUG");
    if (env == NULL) env = getenv("ABT_ENV_USE_EVENT_DEBUG");
    if (env != NULL) {
        if (strcmp(env, "0") == 0 || strcasecmp(env, "n") == 0 ||
            strcasecmp(env, "no") == 0) {
            gp_einfo->use_debug = ABT_FALSE;
        } else {
            gp_einfo->use_debug = ABT_TRUE;
        }
    } else {
        gp_einfo->use_debug = ABT_TRUE;
    }

#ifdef ABT_CONFIG_HANDLE_POWER_EVENT
    gp_einfo->max_stop_xstream_fn = ABTI_DEFAULT_MAX_CB_FN;
    gp_einfo->num_stop_xstream_fn = 0;
    gp_einfo->stop_xstream_fn = (ABT_event_cb_fn *)
        ABTU_calloc(ABTI_DEFAULT_MAX_CB_FN * 2, sizeof(ABT_event_cb_fn));
    gp_einfo->stop_xstream_arg = (void **)
        ABTU_calloc(ABTI_DEFAULT_MAX_CB_FN * 2, sizeof(void *));

    gp_einfo->max_add_xstream_fn = ABTI_DEFAULT_MAX_CB_FN;
    gp_einfo->num_add_xstream_fn = 0;
    gp_einfo->add_xstream_fn = (ABT_event_cb_fn *)
        ABTU_calloc(ABTI_DEFAULT_MAX_CB_FN * 2, sizeof(ABT_event_cb_fn));
    gp_einfo->add_xstream_arg = (void **)
        ABTU_calloc(ABTI_DEFAULT_MAX_CB_FN * 2, sizeof(void *));

    ABTI_event_connect_power(gp_ABTI_global->pm_host, gp_ABTI_global->pm_port);
#endif
#ifdef ABT_CONFIG_PUBLISH_INFO
    if (gp_ABTI_global->pub_needed == ABT_TRUE) {
        if (!strcmp(gp_ABTI_global->pub_filename, "beacon")) {
#ifdef HAVE_BEACON_H
            gp_einfo->pub_type = ABTI_PUB_TYPE_BEACON;

            /* Setup beacon */
            ABTU_strcpy(gp_einfo->binfo.beep_version, "1.0");
            ABTU_strcpy(gp_einfo->binfo.beep_name, "beacon_test");
            int ret = BEACON_Connect(&gp_einfo->binfo, &gp_einfo->handle);
            if (ret != BEACON_SUCCESS) {
                printf("BEACON_Connect is not successful ret=%d\n", ret);
                exit(-1);
            }

            gp_einfo->topic_info = (BEACON_topic_info_t *)
                              ABTU_malloc(sizeof(BEACON_topic_info_t));
            ABTU_strcpy(gp_einfo->topic_info->topic_name, "NODE_POWER");
            sprintf(gp_einfo->topic_info->severity, "INFO");

            gp_einfo->eprop = (BEACON_topic_properties_t *)
                              ABTU_malloc(sizeof(BEACON_topic_properties_t));
            ABTU_strcpy(gp_einfo->eprop->topic_scope, "global");
#else /* HAVE_BEACON_H */
            fprintf(stderr, "BEACON is unavailable. stdout is used instead.\n");
            gp_einfo->pub_type = ABTI_PUB_TYPE_FILE;
            gp_einfo->out_file = stdout;
#endif /* HAVE_BEACON_H */

        } else {
            gp_einfo->pub_type = ABTI_PUB_TYPE_FILE;
            if (!strcmp(gp_ABTI_global->pub_filename, "stdout")) {
                gp_einfo->out_file = stdout;
            } else if (!strcmp(gp_ABTI_global->pub_filename, "stderr")) {
                gp_einfo->out_file = stderr;
            } else {
                gp_einfo->out_file = fopen(gp_ABTI_global->pub_filename, "w");
                if (!gp_einfo->out_file) {
                    gp_ABTI_global->pub_needed = ABT_FALSE;
                    goto fn_exit;
                }
            }
        }

        gp_einfo->max_xstream_rank = gp_ABTI_global->max_xstreams * 2;
        gp_einfo->num_threads = (uint32_t *)ABTU_calloc(
                gp_einfo->max_xstream_rank, sizeof(uint32_t));
        gp_einfo->num_tasks = (uint32_t *)ABTU_calloc(
                gp_einfo->max_xstream_rank, sizeof(uint32_t));
        gp_einfo->idle_time = (double *)ABTU_calloc(
                gp_einfo->max_xstream_rank, sizeof(double));
        gp_einfo->old_num_units = (uint32_t *)ABTU_calloc(
                gp_einfo->max_xstream_rank, sizeof(uint32_t));
        gp_einfo->old_timestamp = (double *)ABTU_calloc(
                gp_einfo->max_xstream_rank, sizeof(double));
        gp_einfo->timestamp = ABTI_get_wtime();

        int ret = RAPLREADER_INIT(&gp_einfo->rr);
        if (ret) {
            printf("raplrease_init is not successful ret=%d\n", ret);
            exit(-1);
        }
    }

  fn_exit:
    return;
#endif
}

void ABTI_event_finalize(void)
{
#ifdef ABT_CONFIG_HANDLE_POWER_EVENT
    ABTI_event_disconnect_power();

    ABTU_free(gp_einfo->stop_xstream_fn);
    ABTU_free(gp_einfo->stop_xstream_arg);
    ABTU_free(gp_einfo->add_xstream_fn);
    ABTU_free(gp_einfo->add_xstream_arg);
#endif
#ifdef ABT_CONFIG_PUBLISH_INFO
    if (gp_ABTI_global->pub_needed == ABT_TRUE) {
        if (gp_einfo->pub_type == ABTI_PUB_TYPE_BEACON) {
#ifdef HAVE_BEACON_H
            BEACON_Disconnect(gp_einfo->handle);
            ABTU_free(gp_einfo->topic_info);
            ABTU_free(gp_einfo->eprop);
#endif
        } else {
            if (gp_einfo->out_file != stdout && gp_einfo->out_file != stderr) {
                fclose(gp_einfo->out_file);
                gp_einfo->out_file = NULL;
            }
        }
        ABTU_free(gp_einfo->num_threads);
        ABTU_free(gp_einfo->num_tasks);
        ABTU_free(gp_einfo->idle_time);
    }
#endif

    ABTI_mutex_fini(&gp_einfo->mutex);
    ABTU_free(gp_einfo);
    gp_einfo = NULL;
}


#ifdef ABT_CONFIG_HANDLE_POWER_EVENT
void ABTI_event_connect_power(char *p_host, int port)
{
    int sockfd;
    struct sockaddr_in serv_addr;
    struct hostent *server;
    int ret;

    server = gethostbyname(p_host);
    if (server == NULL) {
        EVT_DEBUG("[%s] Power mgmt. (%s:%d) not available\n",
                  gp_einfo->hostname, p_host, port);
        return;
    }

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        EVT_DEBUG("[%s] Power event: socket failed\n", gp_einfo->hostname);
        return;
    }

    bzero((char *)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr,
          server->h_length);
    serv_addr.sin_port = htons(port);

    ret = connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    if (ret < 0) {
        EVT_DEBUG("[%s] Power mgmt. (%s:%d) connect failed\n",
                  gp_einfo->hostname, p_host, port);
        return;
    }
    gp_einfo->pfd.fd = sockfd;
    gp_einfo->pfd.events = POLLIN;

    gp_ABTI_global->pm_connected = ABT_TRUE;

    EVT_DEBUG("[%s] Power mgmt. (%s:%d) connected\n",
              gp_einfo->hostname, p_host, port);
}

void ABTI_event_disconnect_power(void)
{
    if (gp_ABTI_global->pm_connected == ABT_FALSE) return;

    close(gp_einfo->pfd.fd);
    gp_ABTI_global->pm_connected = ABT_FALSE;

    EVT_DEBUG("[%s] power mgmt. disconnected\n", gp_einfo->hostname);
}

void ABTI_event_send_num_xstream(void)
{
    char send_buf[ABTI_MSG_BUF_LEN];
    int num_xstreams;
    int n;

    num_xstreams = gp_ABTI_global->num_xstreams;

    sprintf(send_buf, "%d", num_xstreams);
    n = write(gp_einfo->pfd.fd, send_buf, strlen(send_buf));
    ABTI_ASSERT(n == strlen(send_buf));
}

static void ABTI_event_free_xstream(void *arg)
{
    ABTI_local *p_local = lp_ABTI_local;
    char send_buf[ABTI_MSG_BUF_LEN];
    ABT_xstream xstream = (ABT_xstream)arg;
    ABTI_xstream *p_xstream = ABTI_xstream_get_ptr(xstream);
    int abt_errno, n;

    while (ABTD_atomic_load_uint32((uint32_t *)p_xstream->state)
           != ABT_XSTREAM_STATE_TERMINATED) {
#ifndef ABT_CONFIG_DISABLE_EXT_THREAD
        if (ABTI_self_get_type() != ABT_UNIT_TYPE_THREAD) {
            ABTD_atomic_pause();
            continue;
        }
#endif
        ABTI_thread_yield(p_local->p_thread);
    }

    abt_errno = ABTI_xstream_join(p_xstream);
    ABTI_ASSERT(abt_errno == ABT_SUCCESS);
    abt_errno = ABTI_xstream_free(p_xstream);
    ABTI_ASSERT(abt_errno == ABT_SUCCESS);

    if (gp_ABTI_global->pm_connected == ABT_TRUE) {
        LOG_DEBUG("# of ESs: %d\n", gp_ABTI_global->num_xstreams);
        sprintf(send_buf, "[S] killed 1 (%d)", gp_ABTI_global->num_xstreams);
        n = write(gp_einfo->pfd.fd, send_buf, strlen(send_buf));
        ABTI_ASSERT(n == strlen(send_buf));
    }
}

static void ABTI_event_free_multiple_xstreams(void *arg)
{
    ABTI_local *p_local = lp_ABTI_local;
    char send_buf[ABTI_MSG_BUF_LEN];
    ABTI_xstream **p_xstreams = (ABTI_xstream **)arg;
    int num_xstreams = (int)(intptr_t)p_xstreams[0];
    int abt_errno, n;

    for (n = 0; n < num_xstreams; n++) {
        ABTI_xstream *p_xstream = p_xstreams[n+1];
        while (ABTD_atomic_load_uint32((uint32_t *)p_xstream->state)
               != ABT_XSTREAM_STATE_TERMINATED) {
#ifndef ABT_CONFIG_DISABLE_EXT_THREAD
            if (ABTI_self_get_type() != ABT_UNIT_TYPE_THREAD) {
                ABTD_atomic_pause();
                continue;
            }
#endif
            ABTI_thread_yield(p_local->p_thread);
        }

        abt_errno = ABTI_xstream_join(p_xstream);
        ABTI_ASSERT(abt_errno == ABT_SUCCESS);
        abt_errno = ABTI_xstream_free(p_xstream);
        ABTI_ASSERT(abt_errno == ABT_SUCCESS);
    }
    ABTU_free(p_xstreams);

    if (gp_ABTI_global->pm_connected == ABT_TRUE) {
        LOG_DEBUG("# of ESs: %d\n", gp_ABTI_global->num_xstreams);
        sprintf(send_buf, "[S] killed %d (%d)", num_xstreams,
                          gp_ABTI_global->num_xstreams);
        n = write(gp_einfo->pfd.fd, send_buf, strlen(send_buf));
        ABTI_ASSERT(n == strlen(send_buf));
    }
}

ABT_bool ABTI_event_stop_xstream(ABTI_xstream *p_xstream)
{
    int abt_errno = ABT_SUCCESS;
    ABT_bool can_stop = ABT_TRUE;
    ABT_event_cb_fn cb_fn;
    ABT_xstream xstream = ABTI_xstream_get_handle(p_xstream);
    ABTI_xstream *p_primary;
    int i;

    /* Ask whether the target ES can be stopped */
    for (i = 0; i < gp_einfo->max_stop_xstream_fn; i++) {
        cb_fn = gp_einfo->stop_xstream_fn[i*2];
        if (cb_fn) {
            can_stop = cb_fn(gp_einfo->stop_xstream_arg[i*2], xstream);
            if (can_stop == ABT_FALSE) break;
        }
    }

    if (can_stop == ABT_TRUE) {
        ABTI_xstream_set_request(p_xstream, ABTI_XSTREAM_REQ_STOP);

        /* Execute action callback functions */
        for (i = 0; i < gp_einfo->max_stop_xstream_fn; i++) {
            cb_fn = gp_einfo->stop_xstream_fn[i*2+1];
            if (cb_fn) {
                cb_fn(gp_einfo->stop_xstream_arg[i*2+1], xstream);
            }
        }

        /* Create a ULT on the primary ES to join the target ES */
        p_primary = gp_ABTI_global->p_xstreams[0];
        ABTI_pool *p_pool = ABTI_xstream_get_main_pool(p_primary);
        abt_errno = ABTI_thread_create(p_pool, ABTI_event_free_xstream, xstream,
                                       ABT_THREAD_ATTR_NULL, NULL);
        ABTI_ASSERT(abt_errno == ABT_SUCCESS);
    }

    return can_stop;
}

void ABTI_event_decrease_xstream(int target_rank)
{
    char send_buf[ABTI_MSG_BUF_LEN];
    ABTI_xstream *p_xstream;
    int rank, n;
    ABT_bool can_stop = ABT_FALSE;
    ABTI_global *p_global = gp_ABTI_global;

    if (p_global->num_xstreams == 1) {
        LOG_DEBUG("Cannot shrink: # of ESs (%d)\n", p_global->num_xstreams);
        sprintf(send_buf, "[F] only one ES");
        n = write(gp_einfo->pfd.fd, send_buf, strlen(send_buf));
        ABTI_ASSERT(n == strlen(send_buf));
        return;
    }

    if (target_rank == ABT_XSTREAM_ANY_RANK) {
        /* Determine the ES to shut down.  For now, we try to shut down the most
         * recently created one. */
        for (rank = p_global->num_xstreams - 1; rank > 0; rank--) {
            p_xstream = p_global->p_xstreams[rank];
            if (p_xstream) {
                can_stop = ABTI_event_stop_xstream(p_xstream);
                if (can_stop == ABT_TRUE) break;
            }
        }
    } else {
        /* Stop a specific ES */
        if (target_rank < p_global->max_xstreams) {
            p_xstream = p_global->p_xstreams[target_rank];
            if (p_xstream) {
                can_stop = ABTI_event_stop_xstream(p_xstream);
            }
        }
    }

    /* We couldn't stop an ES */
    if (can_stop == ABT_FALSE) {
        sprintf(send_buf, "[F] not possible");
        n = write(gp_einfo->pfd.fd, send_buf, strlen(send_buf));
        ABTI_ASSERT(n == strlen(send_buf));
    }
}

/* Shut down \c num_xstremas ESs. */
void ABTI_event_shrink_xstreams(int num_xstreams)
{
    char send_buf[ABTI_MSG_BUF_LEN];
    ABTI_xstream **p_xstreams;
    ABTI_xstream *p_xstream;
    ABT_xstream xstream;
    int i, rank, n;
    ABT_event_cb_fn cb_fn;
    ABTI_global *p_global = gp_ABTI_global;

    if (p_global->num_xstreams == 1) {
        LOG_DEBUG("Cannot shrink: # of ESs (%d)\n", p_global->num_xstreams);
        sprintf(send_buf, "[F] only one ES");
        n = write(gp_einfo->pfd.fd, send_buf, strlen(send_buf));
        ABTI_ASSERT(n == strlen(send_buf));
        return;
    }

    size_t size = (num_xstreams + 1) * sizeof(ABTI_xstream *);
    p_xstreams = (ABTI_xstream **)ABTU_malloc(size);
    n = 0;

    /* Determine ESs to shut down.  For now, we try to shut down from the most
     * recently created ones. */
    for (rank = p_global->num_xstreams - 1; rank > 0; rank--) {
        p_xstream = p_global->p_xstreams[rank];
        if (p_xstream) {
            /* Ask whether the target ES can be stopped */
            xstream = ABTI_xstream_get_handle(p_xstream);
            ABT_bool can_stop = ABT_TRUE;
            for (i = 0; i < gp_einfo->max_stop_xstream_fn; i++) {
                cb_fn = gp_einfo->stop_xstream_fn[i*2];
                if (cb_fn) {
                    can_stop = cb_fn(gp_einfo->stop_xstream_arg[i*2], xstream);
                    if (can_stop == ABT_FALSE) break;
                }
            }
            if (can_stop == ABT_FALSE) continue;

            ABTI_xstream_set_request(p_xstream, ABTI_XSTREAM_REQ_STOP);

            /* Execute action callback functions */
            for (i = 0; i < gp_einfo->max_stop_xstream_fn; i++) {
                cb_fn = gp_einfo->stop_xstream_fn[i*2+1];
                if (cb_fn) {
                    cb_fn(gp_einfo->stop_xstream_arg[i*2+1], xstream);
                }
            }

            p_xstreams[n+1] = p_xstream;
            if (++n == num_xstreams) break;
        }
    }

    if (n == 0) {
        /* We couldn't stop ESs */
        sprintf(send_buf, "[F] not possible");
        n = write(gp_einfo->pfd.fd, send_buf, strlen(send_buf));
        ABTI_ASSERT(n == strlen(send_buf));
        ABTU_free(p_xstreams);
        return;
    }

    /* Create a ULT on the primary ES to join the target ESs */
    /* NOTE: p_xstreams will be freed by the ULT. */
    ABTI_xstream *p_primary = gp_ABTI_global->p_xstreams[0];
    p_xstreams[0] = (ABTI_xstream *)(intptr_t)n;
    ABTI_pool *p_pool = ABTI_xstream_get_main_pool(p_primary);
    int abt_errno;
    abt_errno = ABTI_thread_create(p_pool, ABTI_event_free_multiple_xstreams,
                                   (void *)p_xstreams, ABT_THREAD_ATTR_NULL,
                                   NULL);
    ABTI_ASSERT(abt_errno == ABT_SUCCESS);
}

void ABTI_event_increase_xstream(int target_rank)
{
    void *abt_arg = (void *)(intptr_t)target_rank;
    char send_buf[ABTI_MSG_BUF_LEN];
    ABT_event_cb_fn cb_fn;
    ABT_bool ret;
    int i, n;

    for (i = 0; i < gp_einfo->max_add_xstream_fn; i++) {
        /* "ask" callback */
        cb_fn = gp_einfo->add_xstream_fn[i*2];
        if (!cb_fn) continue;

        /* TODO: fairness */
        ret = cb_fn(gp_einfo->add_xstream_arg[i*2], abt_arg);
        if (ret == ABT_TRUE) {
            /* "act" callback */
            cb_fn = gp_einfo->add_xstream_fn[i*2+1];
            if (!cb_fn) continue;

            ret = cb_fn(gp_einfo->add_xstream_arg[i*2+1], abt_arg);
            if (ret == ABT_TRUE) {
                LOG_DEBUG("# of ESs: %d\n", gp_ABTI_global->num_xstreams);
                sprintf(send_buf, "[S] created 1 (%d)", gp_ABTI_global->num_xstreams);
                goto send_ack;
            }
        }
    }

    /* We couldn't create a new ES */
    sprintf(send_buf, "[F] not possible");

  send_ack:
    n = write(gp_einfo->pfd.fd, send_buf, strlen(send_buf));
    ABTI_ASSERT(n == strlen(send_buf));
}

void ABTI_event_expand_xstreams(int num_xstreams)
{
    void *abt_arg = (void *)(intptr_t)ABT_XSTREAM_ANY_RANK;
    char send_buf[ABTI_MSG_BUF_LEN];
    ABT_event_cb_fn cb_fn;
    ABT_bool can_add;
    int i, n;

    for (n = 0; n < num_xstreams; n++) {
        can_add = ABT_FALSE;
        for (i = 0; i < gp_einfo->max_add_xstream_fn; i++) {
            /* "ask" callback */
            cb_fn = gp_einfo->add_xstream_fn[i*2];
            if (!cb_fn) continue;

            /* TODO: fairness */
            can_add = cb_fn(gp_einfo->add_xstream_arg[i*2], abt_arg);
            if (can_add == ABT_TRUE) {
                /* "act" callback */
                cb_fn = gp_einfo->add_xstream_fn[i*2+1];
                if (!cb_fn) {
                    can_add = ABT_FALSE;
                    continue;
                }

                can_add = cb_fn(gp_einfo->add_xstream_arg[i*2+1], abt_arg);
                if (can_add == ABT_TRUE) break;
            }
        }
        if (can_add == ABT_FALSE) break;
    }

    if (n > 0) {
        LOG_DEBUG("Create %d ESs (# of ESs: %d)\n", n, gp_ABTI_global->num_xstreams);
        sprintf(send_buf, "[S] created %d (%d)", n, gp_ABTI_global->num_xstreams);
    } else {
        /* We couldn't create a new ES */
        sprintf(send_buf, "[F] not possible");
    }

    n = write(gp_einfo->pfd.fd, send_buf, strlen(send_buf));
    ABTI_ASSERT(n == strlen(send_buf));
}

void ABTI_event_set_num_xstreams(int num_xstreams)
{
    ABTI_global *p_global = gp_ABTI_global;
    char send_buf[ABTI_MSG_BUF_LEN];
    int diff;

    if (p_global->num_xstreams > num_xstreams) {
        diff = p_global->num_xstreams - num_xstreams;
        ABTI_event_shrink_xstreams(diff);

    } else if (p_global->num_xstreams < num_xstreams) {
        diff = num_xstreams - p_global->num_xstreams;
        ABTI_event_expand_xstreams(diff);

    } else {
        sprintf(send_buf, "[S] no change (%d)", p_global->num_xstreams);
        int n = write(gp_einfo->pfd.fd, send_buf, strlen(send_buf));
        ABTI_ASSERT(n == strlen(send_buf));
    }
}

ABT_bool ABTI_event_check_power(ABTI_local *p_local)
{
    ABT_bool stop_xstream = ABT_FALSE;
    int rank, n, ret;
    char recv_buf[ABTI_MSG_BUF_LEN];
    ABTI_xstream *p_xstream;

    if (gp_ABTI_global->pm_connected == ABT_FALSE) goto fn_exit;

    p_xstream = p_local->p_xstream;
    ABTI_ASSERT(p_xstream);
    rank = (int)p_xstream->rank;

    ret = ABTI_mutex_trylock(&gp_einfo->mutex);
    if (ret == ABT_ERR_MUTEX_LOCKED) goto fn_exit;
    ABTI_ASSERT(ret == ABT_SUCCESS);

    ret = poll(&gp_einfo->pfd, 1, 1);
    if (ret == -1) {
        LOG_DEBUG("ERROR: poll (%d)\n", ret);
        ABTI_ASSERT(0);
    } else if (ret != 0) {
        if (gp_einfo->pfd.revents & POLLIN) {
            bzero(recv_buf, ABTI_MSG_BUF_LEN);
            n = read(gp_einfo->pfd.fd, recv_buf, ABTI_MSG_BUF_LEN);
            ABTI_ASSERT(n > 0);

            char *cmd = ABTU_strtrim(recv_buf);
            LOG_DEBUG("ES%d: received request '%s'\n", rank, cmd);
            switch (cmd[0]) {
                case 'd':
                    if (cmd[1] == '\0') {
                        ABTI_event_decrease_xstream(ABT_XSTREAM_ANY_RANK);
                    } else {
                        n = atoi(&cmd[1]);
                        ABTI_event_shrink_xstreams(n);
                    }
                    break;

                case 's':
                    rank = atoi(&cmd[1]);
                    ABTI_event_decrease_xstream(rank);
                    break;

                case 'i':
                    if (cmd[1] == '\0') {
                        ABTI_event_increase_xstream(ABT_XSTREAM_ANY_RANK);
                    } else {
                        n = atoi(&cmd[1]);
                        ABTI_event_expand_xstreams(n);
                    }
                    break;

                case 'c':
                    rank = atoi(&cmd[1]);
                    ABTI_event_increase_xstream(rank);
                    break;

                case 'e':
                    n = atoi(&cmd[1]);
                    ABTI_event_set_num_xstreams(n);
                    break;

                case 'n':
                    ABTI_event_send_num_xstream();
                    break;

                case 'q':
                    ABTI_event_disconnect_power();
                    break;

                default:
                    LOG_DEBUG("Unknown command: %s\n", cmd);
                    break;
            }
        }

        if (gp_einfo->pfd.revents & POLLHUP) {
            gp_ABTI_global->pm_connected = ABT_FALSE;
            EVT_DEBUG("[%s] Server disconnected...\n", gp_einfo->hostname);
        }
        gp_einfo->pfd.revents = 0;
    }

    ABTI_mutex_unlock(&gp_einfo->mutex);

    p_xstream = p_local->p_xstream;
    if (p_xstream->request & ABTI_XSTREAM_REQ_STOP) {
        stop_xstream = ABT_TRUE;
    }

 fn_exit:
    return stop_xstream;
}
#endif /* ABT_CONFIG_HANDLE_POWER_EVENT */

/**
 * @ingroup EVENT
 * @brief   Add callback functions for a specific event.
 *
 * \c ABT_event_add_callback() adds two callback functions for a specified
 * event, \c event, and returns a unique ID through \c cb_id.  \c cb_id can be
 * used to delete registered callbacks in \c ABT_event_del_callback().  All
 * registered callbacks will be invoked when the event happens.
 *
 * @param[in] event         event kind
 * @param[in] ask_cb        callback to ask whether the event can be handled
 * @param[in] ask_user_arg  user argument for \c ask_cb
 * @param[in] act_cb        callback to notify that the event will be handled
 * @param[in] act_user_arg  user argument for \c act_cb
 * @param[out] cb_id        callback ID
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_event_add_callback(ABT_event_kind event,
                           ABT_event_cb_fn ask_cb, void *ask_user_arg,
                           ABT_event_cb_fn act_cb, void *act_user_arg,
                           int *cb_id)
{
#ifdef ABT_CONFIG_HANDLE_POWER_EVENT
    int abt_errno = ABT_SUCCESS;
    int cur_num, max_num;
    size_t new_size;
    int cid = -1;
    int i;

    ABTI_mutex_spinlock(&gp_einfo->mutex);
    switch (event) {
        case ABT_EVENT_STOP_XSTREAM:
            cur_num = gp_einfo->num_stop_xstream_fn;
            max_num = gp_einfo->max_stop_xstream_fn;
            if (cur_num == max_num) {
                /* We need to allocate more space */
                max_num = max_num * 2;
                gp_einfo->max_stop_xstream_fn = max_num;
                new_size = max_num * 2 * sizeof(ABT_event_cb_fn);
                gp_einfo->stop_xstream_fn = (ABT_event_cb_fn *)
                    ABTU_realloc(gp_einfo->stop_xstream_fn, new_size);
                new_size = max_num * 2 * sizeof(void *);
                gp_einfo->stop_xstream_arg = (void **)
                    ABTU_realloc(gp_einfo->stop_xstream_arg, new_size);
            }
            ABTI_ASSERT(cur_num < max_num);

            if (gp_einfo->stop_xstream_fn[cur_num*2] == NULL) {
                gp_einfo->stop_xstream_fn[cur_num*2] = ask_cb;
                gp_einfo->stop_xstream_arg[cur_num*2] = ask_user_arg;
                gp_einfo->stop_xstream_fn[cur_num*2+1] = act_cb;
                gp_einfo->stop_xstream_arg[cur_num*2+1] = act_user_arg;
                cid = cur_num;
            } else {
                for (i = 0; i < max_num; i++) {
                    if (gp_einfo->stop_xstream_fn[i*2] == NULL) {
                        gp_einfo->stop_xstream_fn[i*2] = ask_cb;
                        gp_einfo->stop_xstream_arg[i*2] = ask_user_arg;
                        gp_einfo->stop_xstream_fn[i*2+1] = act_cb;
                        gp_einfo->stop_xstream_arg[i*2+1] = act_user_arg;
                        cid = i;
                        break;
                    }
                }
                ABTI_ASSERT(i < max_num);
            }
            gp_einfo->num_stop_xstream_fn++;
            break;

        case ABT_EVENT_ADD_XSTREAM:
            cur_num = gp_einfo->num_add_xstream_fn;
            max_num = gp_einfo->max_add_xstream_fn;
            if (cur_num == max_num) {
                /* We need to allocate more space */
                max_num = max_num * 2;
                gp_einfo->max_add_xstream_fn = max_num;
                new_size = max_num * 2 * sizeof(ABT_event_cb_fn);
                gp_einfo->add_xstream_fn = (ABT_event_cb_fn *)
                    ABTU_realloc(gp_einfo->add_xstream_fn, new_size);
                new_size = max_num * 2 * sizeof(void *);
                gp_einfo->add_xstream_arg = (void **)
                    ABTU_realloc(gp_einfo->add_xstream_arg, new_size);
            }
            ABTI_ASSERT(cur_num < max_num);

            if (gp_einfo->add_xstream_fn[cur_num*2] == NULL) {
                gp_einfo->add_xstream_fn[cur_num*2] = ask_cb;
                gp_einfo->add_xstream_arg[cur_num*2] = ask_user_arg;
                gp_einfo->add_xstream_fn[cur_num*2+1] = act_cb;
                gp_einfo->add_xstream_arg[cur_num*2+1] = act_user_arg;
                cid = cur_num;
            } else {
                for (i = 0; i < max_num; i++) {
                    if (gp_einfo->add_xstream_fn[i*2] == NULL) {
                        gp_einfo->add_xstream_fn[i*2] = ask_cb;
                        gp_einfo->add_xstream_arg[i*2] = ask_user_arg;
                        gp_einfo->add_xstream_fn[i*2+1] = act_cb;
                        gp_einfo->add_xstream_arg[i*2+1] = act_user_arg;
                        cid = i;
                        break;
                    }
                }
                ABTI_ASSERT(i < max_num);
            }
            gp_einfo->num_add_xstream_fn++;
            break;

        default:
            abt_errno = ABT_ERR_INV_EVENT;
            goto fn_fail;
            break;
    }

  fn_exit:
    ABTI_mutex_unlock(&gp_einfo->mutex);
    *cb_id = cid;
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
#else
    return ABT_SUCCESS;
#endif
}

/**
 * @ingroup EVENT
 * @brief   Delete callback functions registered for a specific event.
 *
 * \c ABT_event_del_callback() deletes callback functions that are registered
 * for \c event with the ID \c cb_id.
 *
 * @param[in] event  event kind
 * @param[in] cb_id  callback ID to delete
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_event_del_callback(ABT_event_kind event, int cb_id)
{
#ifdef ABT_CONFIG_HANDLE_POWER_EVENT
    int abt_errno = ABT_SUCCESS;
    ABTI_mutex_spinlock(&gp_einfo->mutex);
    switch (event) {
        case ABT_EVENT_STOP_XSTREAM:
            gp_einfo->stop_xstream_fn[cb_id*2] = NULL;
            gp_einfo->stop_xstream_fn[cb_id*2+1] = NULL;
            gp_einfo->stop_xstream_arg[cb_id*2] = NULL;
            gp_einfo->stop_xstream_arg[cb_id*2+1] = NULL;
            gp_einfo->num_stop_xstream_fn++;
            break;

        case ABT_EVENT_ADD_XSTREAM:
            gp_einfo->add_xstream_fn[cb_id*2] = NULL;
            gp_einfo->add_xstream_fn[cb_id*2+1] = NULL;
            gp_einfo->add_xstream_arg[cb_id*2] = NULL;
            gp_einfo->add_xstream_arg[cb_id*2+1] = NULL;
            gp_einfo->num_add_xstream_fn++;
            break;

        default:
            abt_errno = ABT_ERR_INV_EVENT;
            goto fn_fail;
            break;
    }

  fn_exit:
    ABTI_mutex_unlock(&gp_einfo->mutex);
    return abt_errno;

  fn_fail:
    HANDLE_ERROR_FUNC_WITH_CODE(abt_errno);
    goto fn_exit;
#else
    return ABT_SUCCESS;
#endif
}

#ifdef ABT_CONFIG_PUBLISH_INFO
void ABTI_event_realloc_pub_arrays(int size)
{
    ABTI_ASSERT(0);
}

void ABTI_event_inc_unit_cnt(ABTI_xstream *p_xstream, ABT_unit_type type)
{
    if (gp_ABTI_global->pub_needed == ABT_FALSE) return;

    int rank = (int)p_xstream->rank;

    if (rank > gp_einfo->max_xstream_rank) {
        ABTI_event_realloc_pub_arrays(rank);
    }

    if (type == ABT_UNIT_TYPE_THREAD) {
        ABTD_atomic_fetch_add_uint32(&gp_einfo->num_threads[rank], 1);
    } else if (type == ABT_UNIT_TYPE_TASK) {
        ABTD_atomic_fetch_add_uint32(&gp_einfo->num_tasks[rank], 1);
    }
}

void ABTI_event_publish_info(ABTI_local *p_local)
{
    ABTI_xstream *p_xstream;
    int rank, i, ret;
    double cur_time, elapsed_time;
    double idle_time, idle_ratio;
    uint32_t cur_num_units, num_threads, num_tasks;
    size_t info_size;
    char *info, *info_ptr;
    ABT_bool is_first;

    if (gp_ABTI_global->pub_needed == ABT_FALSE) return;

    p_xstream = p_local->p_xstream;
    rank = (int)p_xstream->rank;
    if (rank > gp_einfo->max_xstream_rank) {
        ABTI_event_realloc_pub_arrays(rank);
    }

    cur_time = ABTI_get_wtime();
    elapsed_time = cur_time - gp_einfo->timestamp;

    /* Update the idle time of the current ES */
    cur_num_units = gp_einfo->num_threads[rank]
                  + gp_einfo->num_tasks[rank];
    if (gp_einfo->old_timestamp[rank] > 0.0) {
        if (cur_num_units == gp_einfo->old_num_units[rank]) {
            idle_time = cur_time - gp_einfo->old_timestamp[rank];
            ABTD_atomic_fetch_add_double(&gp_einfo->idle_time[rank], idle_time);
        }
    }
    gp_einfo->old_num_units[rank] = cur_num_units;
    gp_einfo->old_timestamp[rank] = cur_time;

    if (elapsed_time < gp_ABTI_global->pub_interval) return;

    /* Only one scheduler has to write to the output file. */
    ret = ABTI_mutex_trylock(&gp_einfo->mutex);
    if (ret == ABT_ERR_MUTEX_LOCKED) return;
    ABTI_ASSERT(ret == ABT_SUCCESS);

    /* Update timestamp */
    gp_einfo->timestamp = cur_time;

    info_size = 200 + 100 * gp_ABTI_global->max_xstreams;
    info = (char *)ABTU_calloc(info_size, sizeof(char));
    info_ptr = info;

    sprintf(info_ptr, "{\"node\":\"%s\",\"sample\":\"argobots\","
                      "\"time\":%.3f,\"num_es\":%d,",
            gp_einfo->hostname, cur_time, gp_ABTI_global->num_xstreams);
    info_ptr += strlen(info_ptr);

    is_first = ABT_TRUE;
    sprintf(info_ptr, "\"num_threads\":{");
    info_ptr += strlen(info_ptr);
    for (i = 0; i < gp_ABTI_global->max_xstreams; i++) {
        num_threads = gp_einfo->num_threads[i];
        if (num_threads > 0) {
            ABTD_atomic_fetch_sub_uint32(&gp_einfo->num_threads[i], num_threads);
        }
        if (gp_ABTI_global->p_xstreams[i]) {
            if (is_first == ABT_TRUE) {
                is_first = ABT_FALSE;
            } else {
                sprintf(info_ptr, ",");
                info_ptr += 1;
            }
            sprintf(info_ptr, "\"es%d\":%d", i, num_threads);
            info_ptr += strlen(info_ptr);
        }
    }
    sprintf(info_ptr, "},");
    info_ptr += 2;

    is_first = ABT_TRUE;
    sprintf(info_ptr, "\"num_tasks\":{");
    info_ptr += strlen(info_ptr);
    for (i = 0; i < gp_ABTI_global->max_xstreams; i++) {
        num_tasks = gp_einfo->num_tasks[i];
        if (num_tasks > 0) {
            ABTD_atomic_fetch_sub_uint32(&gp_einfo->num_tasks[i], num_tasks);
        }
        if (gp_ABTI_global->p_xstreams[i]) {
            if (is_first == ABT_TRUE) {
                is_first = ABT_FALSE;
            } else {
                sprintf(info_ptr, ",");
                info_ptr += 1;
            }
            sprintf(info_ptr, "\"es%d\":%d", i, num_tasks);
            info_ptr += strlen(info_ptr);
        }
    }
    sprintf(info_ptr, "},");
    info_ptr += 2;

    is_first = ABT_TRUE;
    sprintf(info_ptr, "\"idle\":{");
    info_ptr += strlen(info_ptr);
    for (i = 0; i < gp_ABTI_global->max_xstreams; i++) {
        idle_time = gp_einfo->idle_time[i];
        if (idle_time > 0.0) {
            ABTD_atomic_fetch_sub_double(&gp_einfo->idle_time[i], idle_time);
        }
        if (gp_ABTI_global->p_xstreams[i]) {
            if (is_first == ABT_TRUE) {
                is_first = ABT_FALSE;
            } else {
                sprintf(info_ptr, ",");
                info_ptr += 1;
            }
            idle_ratio = idle_time / elapsed_time * 100.0;
            sprintf(info_ptr, "\"es%d\":%.1f", i, idle_ratio);
            info_ptr += strlen(info_ptr);
        }
    }
    sprintf(info_ptr, "}}\n");
    info_ptr += strlen(info_ptr);
    ABTI_ASSERT((intptr_t)info_ptr < ((intptr_t)info + info_size));

    if (gp_einfo->pub_type == ABTI_PUB_TYPE_BEACON) {
#ifdef HAVE_BEACON_H
        LOG_DEBUG("%s", info);
        ABTU_strcpy(gp_einfo->eprop->topic_payload, info);
        ret = BEACON_Publish(gp_einfo->handle, gp_einfo->topic_info->topic_name,
                             gp_einfo->eprop);
        if (ret != BEACON_SUCCESS) {
            printf("BEACON_Publish failed with ret=%d\n", ret);
            exit(-1);
        }
#endif
    } else if (gp_einfo->pub_type == ABTI_PUB_TYPE_FILE) {
        fprintf(gp_einfo->out_file, "%s", info);
        fflush(gp_einfo->out_file);
    }

    ABTU_free(info);

    ABTI_mutex_unlock(&gp_einfo->mutex);
}
#endif /* ABT_CONFIG_PUBLISH_INFO */


/**
 * @ingroup EVENT
 * @brief   Start performance profiling.
 *
 * \c ABT_event_prof_start() starts profiling performance data, such as power
 * consumption.
 *
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_event_prof_start(void)
{
#ifdef ABT_CONFIG_PUBLISH_INFO
    if (gp_ABTI_global->pub_needed == ABT_FALSE) return ABT_SUCCESS;

    gp_einfo->prof_start_time = ABTI_get_wtime();
    RAPLREADER_SAMPLE(&gp_einfo->rr);
#endif

    return ABT_SUCCESS;
}


/**
 * @ingroup EVENT
 * @brief   Stop performance profiling.
 *
 * \c ABT_event_prof_stop() stops performance profiling that was started by
 * \c ABT_event_prof_start().
 *
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_event_prof_stop(void)
{
#ifdef ABT_CONFIG_PUBLISH_INFO
    if (gp_ABTI_global->pub_needed == ABT_FALSE) return ABT_SUCCESS;

    gp_einfo->prof_stop_time = ABTI_get_wtime();
    RAPLREADER_SAMPLE(&gp_einfo->rr);
#endif

    return ABT_SUCCESS;
}

/**
 * @ingroup EVENT
 * @brief   Publish the performance profiling data.
 *
 * \c ABT_event_prof_publish() publishes the performance data, which is given
 * by the user as \c local_work and \c global_work, along with the profiling
 * data, which is measured by \c ABT_event_perf_start() and
 * \c ABT_event_perf_stop().  The output target of this routine can be BEACON
 * or a file.
 *
 * This routine is supposed to be called after \c ABT_event_perf_stop().
 *
 * @param[in] unit_name    the name of performance unit
 * @param[in] local_work   the amount of work done in a node
 * @param[in] global_work  the amount of work done by the application
 * @return Error code
 * @retval ABT_SUCCESS on success
 */
int ABT_event_prof_publish(const char *unit_name, double local_work,
                           double global_work)
{
#ifdef ABT_CONFIG_PUBLISH_INFO
    if (gp_ABTI_global->pub_needed == ABT_FALSE) return ABT_SUCCESS;

    const char *sample_name = "application";
    double elapsed_time = gp_einfo->prof_stop_time - gp_einfo->prof_start_time;
#if defined(HAVE_RAPLREADER_H) && defined(HAVE_LIBINTERCOOLR)
    double power = gp_einfo->rr.power_total;
#endif
    double local_rate = local_work / elapsed_time;
    double global_rate = global_work / elapsed_time;

    char *info = (char *)ABTU_calloc(1024, sizeof(char));
#if defined(HAVE_RAPLREADER_H) && defined(HAVE_LIBINTERCOOLR)
    sprintf(info,
            "{\"node\":\"%s\",\"sample\":\"%s\",\"time\":%lf,\"%s_per_sec_per_node\":%lf,"
            "\"%s_per_watt_per_node\":%lf,\"%s_per_sec\":%lf}\n",
            gp_einfo->hostname, sample_name, ABTI_get_wtime(), unit_name, local_rate,
            unit_name, local_work/power, unit_name, global_rate);
#else
    sprintf(info,
            "{\"node\":\"%s\",\"sample\":\"%s\",\"time\":%lf,\"%s_per_sec_per_node\":%lf,"
            "\"%s_per_sec\":%lf}\n",
            gp_einfo->hostname, sample_name, ABTI_get_wtime(), unit_name, local_rate,
            unit_name, global_rate);
#endif

    if (gp_einfo->pub_type == ABTI_PUB_TYPE_BEACON) {
#ifdef HAVE_BEACON_H
        EVT_DEBUG("%s", info);
        ABTU_strcpy(gp_einfo->eprop->topic_payload, info);
        int ret = BEACON_Publish(gp_einfo->handle, gp_einfo->topic_info->topic_name,
                                 gp_einfo->eprop);
        if (ret != BEACON_SUCCESS) {
            printf("BEACON_Publish failed with ret=%d\n", ret);
            exit(-1);
        }
#endif
    } else if (gp_einfo->pub_type == ABTI_PUB_TYPE_FILE) {
        fprintf(gp_einfo->out_file, "%s", info);
    }

    ABTU_free(info);
#endif

    return ABT_SUCCESS;
}

