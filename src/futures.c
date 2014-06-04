/**
 *  futures are used to wait for values asynchronously
 * */
#include <stdlib.h>
#include <string.h>
#include "abti.h"
#include "futures.h"

typedef struct ABT_Thread_entry_t {
    ABT_Thread *current;
    struct ABT_Thread_entry_t *next;
} ABT_Thread_entry;

typedef struct {
    ABT_Thread_entry *head;
    ABT_Thread_entry *tail;
} ABT_Threads_list;

typedef struct {
    ABT_Stream *stream;
    int ready;
    void *value;
    int nbytes;
    ABT_Threads_list waiters;
} ABT_Future_data;



void *ABT_Future_wait(ABT_Future *fut)
{
    ABT_Future_data *data = (ABT_Future_data*)fut->data;
    if (!data->ready) {
        ABT_Thread_entry *cur = (ABT_Thread_entry*) malloc(sizeof(ABT_Thread_entry));
        cur->current = ABTI_Thread_current();
        cur->next = NULL;
        if(data->waiters.tail != NULL)
            data->waiters.tail->next = cur;
        data->waiters.tail = cur;
        if(data->waiters.head == NULL)
            data->waiters.head = cur;
        ABTI_Thread_suspend();
    }
    return data->value;
}

void ABT_Future_signal(ABT_Future_data *fut)
{
    ABT_Thread_entry *cur = fut->waiters.head;
    while(cur!=NULL)
    {
        ABT_Thread *mythread = cur->current;
        ABTI_Thread_set_ready(mythread);
        ABT_Thread_entry *tmp = cur;
        cur=cur->next;
        free(tmp);
    }
}

void ABT_Future_set(ABT_Future *fut, void *value, int nbytes)
{
    ABT_Future_data *data = (ABT_Future_data*)fut->data;
    data->ready = 1;
    memcpy(data->value, value, nbytes);
    ABT_Future_signal(data);
}

ABT_Future *future_create(int n, ABT_Stream *stream)
{
    ABT_Future_data *data = (ABT_Future_data *) malloc(sizeof(ABT_Future_data));
    data->stream = stream;
    data->ready = 0;
    data->nbytes = n;
    data->value = malloc(n);;
    data->waiters.head = data->waiters.tail = NULL;
    ABT_Future *fut = (ABT_Future*)malloc(sizeof(ABT_Future));
    fut->data = data;
    return fut;
}
