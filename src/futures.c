/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#include <stdlib.h>
#include <string.h>
#include "abti.h"

/** @defgroup Future Future
 * Futures are used to wait for values asynchronously.
 */

typedef struct ABT_thread_entry_t {
    ABT_thread current;
    struct ABT_thread_entry_t *next;
} ABT_thread_entry;

typedef struct {
    ABT_thread_entry *head;
    ABT_thread_entry *tail;
} ABT_threads_list;

typedef struct {
    ABT_stream stream;
    int ready;
    void *value;
    int nbytes;
    ABT_threads_list waiters;
} ABT_future_data;

/**
 * @ingroup Future
 * @brief   Blocks current thread until the feature has finished its computation.
 * 
 * @param[in]  future       Reference to the future
 * @param[out] value        Reference to value of future
 * @return Error code
 */
int ABT_future_wait(ABT_future future, void **value)
{
	int abt_errno = ABT_SUCCESS;
	ABTI_future *p_future = ABTI_future_get_ptr(future);	
    ABT_future_data *data = (ABT_future_data*)p_future->data;
    if (!data->ready) {
        ABT_thread_entry *cur = (ABT_thread_entry*) malloc(sizeof(ABT_thread_entry));
        cur->current = ABTI_thread_current();
        cur->next = NULL;
        if(data->waiters.tail != NULL)
            data->waiters.tail->next = cur;
        data->waiters.tail = cur;
        if(data->waiters.head == NULL)
            data->waiters.head = cur;
        ABT_thread_yield();
    }
    *value = data->value;
	return abt_errno;
}

/**
 * @ingroup Future
 * @brief   Signals all threads blocking on a future once the result has been calculated.
 * 
 * @param[in]  data       Pointer to future's data
 * @return No valud returned
 */
void ABTI_future_signal(ABT_future_data *data)
{
    ABT_thread_entry *cur = data->waiters.head;
    while(cur!=NULL)
    {
        ABT_thread mythread = cur->current;
        ABTI_thread_set_ready(mythread);
        ABT_thread_entry *tmp = cur;
        cur=cur->next;
        free(tmp);
    }
}

/**
 * @ingroup Future
 * @brief   Sets a nbytes-value into a future for all threads blocking on the future to resume.
 * 
 * @param[in]  future       Reference to the future
 * @param[in]  value        Pointer to the buffer containing the result
 * @param[in]  nbytes       Number of bytes in the buffer
 * @return Error code
 */
int ABT_future_set(ABT_future future, void *value, int nbytes)
{
	int abt_errno = ABT_SUCCESS;
	ABTI_future *p_future = ABTI_future_get_ptr(future);	
    ABT_future_data *data = (ABT_future_data*)p_future->data;
    data->ready = 1;
    memcpy(data->value, value, nbytes);
    ABTI_future_signal(data);
	return abt_errno;
}

/**
 * @ingroup Future
 * @brief   Creates a future.
 * 
 * @param[in]  n            Number of bytes in the buffer containing the result of the future
 * @param[in]  stream       Stream on which this future will run
 * @param[out] future       Reference to the newly created future
 * @return Error code
 */
int ABT_future_create(int n, ABT_stream stream, ABT_future *future)
{
	int abt_errno = ABT_SUCCESS;
    ABT_future_data *data = (ABT_future_data *) malloc(sizeof(ABT_future_data));
    data->stream = stream;
    data->ready = 0;
    data->nbytes = n;
    data->value = malloc(n);;
    data->waiters.head = data->waiters.tail = NULL;
    ABTI_future *p_future = (ABTI_future*)malloc(sizeof(ABTI_future));
    p_future->data = data;
    *future = p_future;
	return abt_errno;
}

/* Private API */
ABTI_future *ABTI_future_get_ptr(ABT_future future)
{
    ABTI_future *p_future;
    if (future == ABT_FUTURE_NULL) {
        p_future = NULL;
    } else {
        p_future = (ABTI_future *)future;
    }
    return p_future;
}
