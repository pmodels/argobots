#ifndef FUTURES_H_INCLUDED
#define FUTURES_H_INCLUDED

typedef struct{
   int pe;
   void *data;
} ABT_future;

ABT_future* ABT_future_create(int n, ABT_stream *stream);

void ABT_future_set(ABT_future *fut, void *value, int nbytes);

void *ABT_future_wait(ABT_future *fut);

#endif /* FUTURES_H_INCLUDED */
