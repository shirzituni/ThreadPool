// Shir Zituni 
#ifndef __THREAD_POOL__
#define __THREAD_POOL__

#include <pthread.h>
#include "osqueue.h"

typedef struct thread_pool
{
  pthread_mutex_t lock;
  pthread_cond_t notify; 
  pthread_t *threads; //array with threads
  OSQueue *queue;
  int numOfThreads;
  int numOfTasks; 
  int head;
  int tail;
  int shutdown; //says if the pool is shutting down
}ThreadPool;

ThreadPool* tpCreate(int numOfThreads);

void tpDestroy(ThreadPool* threadPool, int shouldWaitForTasks);

int tpInsertTask(ThreadPool* threadPool, void (*computeFunc) (void *), void* param);

int threadpool_free(ThreadPool* pool);

#endif
