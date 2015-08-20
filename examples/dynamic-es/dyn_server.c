/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <poll.h>
#include <assert.h>

#include "common.h"

static void handle_error(const char *msg);

int main(int argc, char *argv[])
{
    int sockfd, port;
    struct sockaddr_in my_addr;
    struct sockaddr_in abt_addr;
    socklen_t addrlen;
    struct pollfd abt_pfd;

    char send_buf[SEND_BUF_LEN];
    char recv_buf[RECV_BUF_LEN];
    int quit = 0;
    int abt_alive = 0;
    int n, ret;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(1);
    }
    port = atoi(argv[1]);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) handle_error("ERROR: socket");

    bzero((char *)&my_addr, sizeof(my_addr));
    my_addr.sin_family = AF_INET;
    my_addr.sin_addr.s_addr = INADDR_ANY;
    my_addr.sin_port = htons(port);
    ret = bind(sockfd, (struct sockaddr *)&my_addr, sizeof(my_addr));
    if (ret < 0) handle_error("ERROR: bind");

    while (!quit) {
        printf("Waiting for connection...\n");

        listen(sockfd, 5);
        addrlen = sizeof(abt_addr);
        abt_pfd.fd = accept(sockfd, (struct sockaddr *)&abt_addr, &addrlen);
        if (abt_pfd.fd < 0) handle_error("ERROR: accept");
        abt_pfd.events = POLLIN | POLLRDHUP;
        abt_alive = 1;

        printf("Client connected...\n\n");

        while (abt_alive) {
            printf("d: decrease # of ESs\n");
            printf("i: increase # of ESs\n");
            printf("n: ask # of ESs\n");
            printf("q: quit\n");
            printf("Please enter your command: ");
            bzero(send_buf, SEND_BUF_LEN);
            fgets(send_buf, SEND_BUF_LEN, stdin);

            if (send_buf[0] != 'd' && send_buf[0] != 'i' &&
                send_buf[0] != 'n' && send_buf[0] != 'q') {
                printf("Unknown command: %s\n", send_buf);
                continue;
            }

            n = write(abt_pfd.fd, send_buf, strlen(send_buf));
            assert(n == strlen(send_buf));

            bzero(recv_buf, RECV_BUF_LEN);

            /* Wait for the ack */
            printf("Waiting for the response...\n");
            while (1) {
                ret = poll(&abt_pfd, 1, 10);
                if (ret == -1) {
                    handle_error("ERROR: poll");
                } else if (ret != 0) {
                    if (abt_pfd.revents & POLLIN) {
                        n = read(abt_pfd.fd, recv_buf, RECV_BUF_LEN);
                        if (n < 0) handle_error("ERROR: read");

                        printf("Response: %s\n\n", recv_buf);
                    }
                    if (abt_pfd.revents & POLLRDHUP) {
                        abt_alive = 0;
                        printf("Client disconnected...\n");
                        break;
                    }
                    abt_pfd.revents = 0;
                    break;
                }
            }

            if (send_buf[0] == 'q') {
                quit = 1;
                close(abt_pfd.fd);
                break;
            }
        }
    }

    close(sockfd);

    return EXIT_SUCCESS;
}

static void handle_error(const char *msg)
{
    perror(msg);
    exit(EXIT_FAILURE);
}

