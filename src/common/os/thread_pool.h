#ifndef THREADS_H
#define THREADS_H
#include <stdbool.h>
#include <stddef.h>


typedef void (*thread_func)(void* arg);

typedef struct {
    thread_func func;
    void* arg;
} thread_pool_task_t;

typedef struct thread_pool_t thread_pool_t;

thread_pool_t* thread_pool_create(unsigned int thread_count, unsigned int queue_count);
bool thread_pool_add_task(thread_pool_t *pool, thread_pool_task_t task);
void thread_pool_destroy(thread_pool_t *pool);

#endif //THREADS_H
