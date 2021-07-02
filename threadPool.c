// Shir Zituni 
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#include "osqueue.h"
#include "threadPool.h"

typedef struct {
    void (*function)(void*);
    void *argument;
} Task;

static void *workerThreads(void *thread_pool)
{
    //casting backwards
    ThreadPool *threadPool = (ThreadPool *)thread_pool;

    for(;;) {
        pthread_mutex_lock(&(threadPool->lock));
        //while no tasks
        while((threadPool->numOfTasks == 0) && (!threadPool->shutdown)) {
            pthread_cond_wait(&(threadPool->notify), &(threadPool->lock));
        }

        if((threadPool->numOfTasks == 0)&&(threadPool->shutdown == 1)) {
            break;
        }
    }

    pthread_mutex_unlock(&(threadPool->lock));
    pthread_exit(NULL);
    return(NULL);
}

int tpFree(ThreadPool *threadPool){
    if(threadPool == NULL) {    
        return -1;
    }
    if(threadPool->threads) {
        free(threadPool->threads);
        osDestroyQueue(threadPool->queue);
        pthread_mutex_lock(&(threadPool->lock));
        pthread_mutex_destroy(&(threadPool->lock));
        pthread_cond_destroy(&(threadPool->notify));
    }
    free(threadPool);    
    return 0;
}

ThreadPool* tpCreate(int numOfThreads){
        ThreadPool *threadPool;
        int i;
        // check numOfThreads
        if(numOfThreads <= 0) {
            return NULL;
        }
        // check if malloc Succeeded
        if((threadPool = (ThreadPool *)malloc(sizeof(ThreadPool))) == NULL) {
            return NULL;
        }

        //initialize 
        threadPool->numOfThreads = numOfThreads;
        threadPool->head = threadPool->tail = threadPool->numOfTasks = 0;
        threadPool->shutdown =  0;

        //Allocate thread and Create Queue
        threadPool->threads = (pthread_t *)malloc(sizeof(pthread_t) * numOfThreads);
        threadPool->queue = osCreateQueue();

        //initialize mutex and conditional variable
        if((pthread_mutex_init(&(threadPool->lock), NULL) != 0) ||
        (pthread_cond_init(&(threadPool->notify), NULL) != 0) ||
        (threadPool->threads == NULL) ||
        (threadPool->queue == NULL)) {
            if(threadPool) {
                tpFree(threadPool);
            }
            return NULL;
        }
        //start worker threads
        for(i = 0; i < numOfThreads; i++) {
            // creat the thread! , use workerThreads function
            if(pthread_create(&(threadPool->threads[i]), NULL, workerThreads, (void*)threadPool) != 0) {
                return NULL;
            }
            threadPool->numOfThreads++;
        }
        return threadPool;
    }
void tpDestroy(ThreadPool* threadPool, int shouldWaitForTasks){
     int i, is_err = 0;
     if(threadPool == NULL) {
        perror("invalid threadpool");
    }
    threadPool->shutdown = 1;

    if(shouldWaitForTasks == 0){
        // tell all the threads to stop!
        threadPool->numOfTasks=0;
        if(pthread_cond_broadcast(&(threadPool->notify)) != 0) {
            is_err = 1;
        }
        // empty the queue
        while(!osIsQueueEmpty){
            osDequeue(threadPool->queue);
        }
    }
    //if shouldWaitForTasks != 0, we should wait *also* for the tasks in the queue
    else{
        //try to lock 
        if(pthread_mutex_lock(&(threadPool->lock)) != 0) {
            is_err = 1;
            perror("couldn't lock");
        }
        //pthread_cond_wait -  only if I should wait for task
        while((threadPool->numOfTasks == 0) && (!threadPool->shutdown)) {
            pthread_cond_wait(&(threadPool->notify), &(threadPool->lock));
        }

        //unlock
        if(pthread_mutex_unlock(&(threadPool->lock)) != 0) {
            is_err = 1;
            perror("couldn't unlock");
        }
    }
        // pthread_cond_broadcast unblock all threads currently blocked on the specified condition variable cond
        if(pthread_cond_broadcast(&(threadPool->notify)) != 0) {
            is_err = 1;
        }

       //join
        for(i = 0; i < threadPool->numOfTasks; i++) {
            if(pthread_join(threadPool->threads[i], NULL) != 0) {
                is_err = 1;
            }
        } 
    if(is_err == 0) {
        tpFree(threadPool);
    }
}
//computeFunc-mission , param-args for the mission
int tpInsertTask(ThreadPool* threadPool, void (*computeFunc)(void *), void* param){
    if(threadPool == NULL || computeFunc == NULL) {
        perror("invalid threadpool");
        return -1;
    }
    do {
        if(threadPool->shutdown) {
            perror("queue is shouting down");
            return 0;
        }

        //add task to queue
        Task* task = (Task*) malloc(sizeof(Task));
        task->function = computeFunc;
        task->argument = param;
        if(pthread_mutex_lock(&(threadPool->lock)) != 0) {
            perror("couldn't lock");
            return -1;
        } 
        osEnqueue(threadPool->queue, task);
         threadPool->numOfTasks -= 1;
        //make it work
        void (*funcPtr)(void *) = task->function;
        void *parameter = task->argument;
        pthread_mutex_unlock(&threadPool->lock);
        (*funcPtr)(parameter);

        threadPool->tail += 1;
        threadPool->numOfTasks += 1;
        
        //broadcast 
        if(pthread_cond_signal(&(threadPool->notify)) != 0) {
            perror("couldn't lock");
            return -1;
        }
        free(task);
    } while(0);

    if(pthread_mutex_unlock(&threadPool->lock) != 0) {
        perror("couldn't lock");
        return -1;
    }
    return 0;
}

