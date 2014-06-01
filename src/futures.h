#ifndef _ABT_FUTURE_H_  
#define _ABT_FUTURE_H_ 

typedef struct{
   int pe;
   void *data;
} ABT_Future;

ABT_Future* ABT_Future_create(int n, ABT_Stream *stream);

void ABT_Future_set(ABT_Future *fut, void *value, int nbytes);

void *ABT_Future_wait(ABT_Future *fut);

#endif
