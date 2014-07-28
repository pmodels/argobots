/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <abt.h>

#define EVENTUAL_SIZE 10

ABT_thread th1, th2, th3;
ABT_eventual myeventual;

#define LOOP_CNT  10
void fn1()
{
    int i = 0;
    void *data =  malloc(EVENTUAL_SIZE);
    printf("Thread 1 iteration %d waiting for eventual \n", i);
    ABT_eventual_wait(myeventual,&data);
    printf("Thread 1 continue iteration %d returning from eventual \n", i);
}

void fn2()
{
    int i = 0;
    void *data = malloc(EVENTUAL_SIZE);
    printf("Thread 2 iteration %d waiting from eventual \n", i);
    ABT_eventual_wait(myeventual,&data);
    printf("Thread 2 continue iteration %d returning from eventual \n", i);
}

void fn3()
{
    int i = 0;
    printf("Thread 3 iteration %d signal eventual \n", i);
    char *data = (char *) malloc(EVENTUAL_SIZE);
    ABT_eventual_set(myeventual, data, EVENTUAL_SIZE);
    printf("Thread 3 continue iteration %d \n", i);
}

int main(int argc, char *argv[])
{
    ABT_xstream xstream;

    /* init and thread creation */
    ABT_init(argc, argv);
    ABT_xstream_self(&xstream);
    ABT_thread_create(xstream, fn1, NULL, ABT_THREAD_ATTR_NULL, &th1);
    ABT_thread_create(xstream, fn2, NULL, ABT_THREAD_ATTR_NULL, &th2);
    ABT_thread_create(xstream, fn3, NULL, ABT_THREAD_ATTR_NULL, &th3);
    ABT_eventual_create(EVENTUAL_SIZE,&myeventual);
    printf("START \n");

    /* switch to other user-level threads */
    ABT_thread_yield();

    /* join other threads */
    ABT_thread_join(th1);
    ABT_thread_join(th2);
    ABT_thread_join(th3);

    printf("END \n");
    ABT_finalize();
    return 0;
}
