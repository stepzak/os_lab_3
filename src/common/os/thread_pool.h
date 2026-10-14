#ifndef THREAD_POOL_H
#define THREAD_POOL_H
#include <stdbool.h>
#include <stddef.h>

#include "common/raii.h"

typedef void (*thread_func)(void* arg);

typedef struct {
    thread_func func;
    void* arg;
} thread_pool_task_t;

typedef enum {
    THREAD_POOL_OK = 0,
    THREAD_POOL_INVALID_SIZE,
    THREAD_POOL_INVALID_DESTINATION,
    THREAD_POOL_ALLOCATION_FAILED,
    THREAD_POOL_SYSTEM_ERROR
} thread_pool_error_t;

typedef struct thread_pool_impl_t thread_pool_impl_t;

typedef struct {
    thread_pool_impl_t *impl;
} thread_pool_t;

void thread_pool_reset(thread_pool_t *pool);
bool thread_pool_is_valid(const thread_pool_t *pool);
thread_pool_error_t thread_pool_init(thread_pool_t *pool, size_t thread_count, size_t queue_capacity);
bool thread_pool_add_task(thread_pool_t *pool, thread_pool_task_t task);
bool thread_pool_add_tasks(thread_pool_t *pool, const thread_pool_task_t *tasks, size_t task_count);
void thread_pool_wait(thread_pool_t *pool);
size_t thread_pool_thread_count(const thread_pool_t *pool);
void thread_pool_destroy(thread_pool_t *pool);
const char *thread_pool_error_string(thread_pool_error_t error);

static bool thread_pool_init_succeeded(thread_pool_error_t error) {
    return error == THREAD_POOL_OK;
}

DECLARE_SHARED_RAII_TYPES(
    thread_pool,
    thread_pool_t,
    thread_pool_init,
    thread_pool_destroy,
    thread_pool_is_valid,
    thread_pool_reset
)
DEFINE_RAII_TRIVIAL_MOVE(
    RAII_STACK_TYPE(thread_pool),
    thread_pool_is_valid,
    thread_pool_reset
)
DEFINE_RAII_INIT_VALIDATION(thread_pool, thread_pool_init_succeeded)

#endif //THREAD_POOL_H
