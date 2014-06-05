/**
 *  futures are used to wait for values asynchronously
 * */
#include <stdlib.h>
#include <string.h>
#include "abti.h"
#include "futures.h"

typedef struct ABT_thread_entry_t {
    ABT_thread *current;
    struct ABT_thread_entry_t *next;
} ABT_thread_entry;

typedef struct {
    ABT_thread_entry *head;
    ABT_thread_entry *tail;
} ABT_threads_list;

typedef struct {
    ABT_stream *stream;
    int ready;
    void *value;
    int nbytes;
    ABT_threads_list waiters;
} ABT_future_data;



void *ABT_future_wait(ABT_future *fut)
{
    ABT_future_data *data = (ABT_future_data*)fut->data;
    if (!data->ready) {
        ABT_thread_entry *cur = (ABT_thread_entry*) malloc(sizeof(ABT_thread_entry));
        cur->current = ABTI_thread_current();
        cur->next = NULL;
        if(data->waiters.tail != NULL)
            data->waiters.tail->next = cur;
        data->waiters.tail = cur;
        if(data->waiters.head == NULL)
            data->waiters.head = cur;
        ABTI_thread_suspend();
    }
    return data->value;
}

void ABT_future_signal(ABT_future_data *fut)
{
    ABT_thread_entry *cur = fut->waiters.head;
    while(cur!=NULL)
    {
        ABT_thread *mythread = cur->current;
        ABTI_thread_set_ready(mythread);
        ABT_thread_entry *tmp = cur;
        cur=cur->next;
        free(tmp);
    }
}

void ABT_future_set(ABT_future *fut, void *value, int nbytes)
{
    ABT_future_data *data = (ABT_future_data*)fut->data;
    data->ready = 1;
    memcpy(data->value, value, nbytes);
    ABT_future_signal(data);
}

ABT_future *future_create(int n, ABT_stream *stream)
{
    ABT_future_data *data = (ABT_future_data *) malloc(sizeof(ABT_future_data));
    data->stream = stream;
    data->ready = 0;
    data->nbytes = n;
    data->value = malloc(n);;
    data->waiters.head = data->waiters.tail = NULL;
    ABT_future *fut = (ABT_future*)malloc(sizeof(ABT_future));
    fut->data = data;
    return fut;
}
