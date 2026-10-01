#include "../thread_pool.h"
#include <pthread.h>
#include <stdlib.h>

#include "dtors.h"
#include "../../unlikely.h"

struct thread_pool_t {
    pthread_mutex_t mutex;
    pthread_cond_t cond;

    pthread_t* threads;
    thread_pool_task_t* queue;

    unsigned thread_count;
    unsigned queue_size;

    unsigned head;
    unsigned tail;
    unsigned count;

    bool shutdown;
};

static void* pool_worker(void* threadpool) {
    thread_pool_t* pool = (thread_pool_t*)threadpool;

    for (;;) {
        thread_pool_task_t task;
        {
            LOCK_GUARD(pool->mutex);
            while (pool->count == 0 && pool->shutdown) {
                pthread_cond_wait(&pool->cond, &pool->mutex);
            }
            if (pool->shutdown && pool->count == 0) {
                break;
            }

            task = pool->queue[pool->head];
            pool->head = (pool->head + 1) % pool->queue_size;
            pool->count--;
        }
        task.func(task.arg);
    }
    return NULL;
}

thread_pool_t* thread_pool_create(unsigned queue_size, unsigned thread_count) {
    if (queue_size == 0 || thread_count == 0) {
        return NULL;
    }
    thread_pool_t* pool = (thread_pool_t*)malloc(sizeof(thread_pool_t));
    if (unlikely(!pool)) return NULL;
    pool->threads = (pthread_t*)malloc(sizeof(pthread_t) * thread_count);
    if (unlikely(!pool->threads)) {

    }

    pool->thread_count = thread_count;
    pool->queue_size = queue_size;
    pool->head = pool->tail = pool->count = 0;
    pool->shutdown = false;


}
