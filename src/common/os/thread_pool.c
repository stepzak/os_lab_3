#include "thread_pool.h"

#include <pthread.h>
#include <stdlib.h>

#include "posix/mutex.h"

struct thread_pool_impl_t {
    pthread_t *threads;
    size_t thread_count;

    thread_pool_task_t *queue;
    size_t queue_capacity;
    size_t queue_head;
    size_t queue_tail;
    size_t queued_count;
    size_t active_count;

    bool stopping;
    mutex_t mutex;
    pthread_cond_t task_available;
    pthread_cond_t queue_space_available;
    pthread_cond_t idle;
};

void thread_pool_reset(thread_pool_t *pool) {
    if (pool == NULL) return;
    pool->impl = NULL;
}

bool thread_pool_is_valid(const thread_pool_t *pool) {
    return pool != NULL && pool->impl != NULL;
}

static void *thread_pool_worker(void *argument) {
    thread_pool_impl_t *pool = argument;

    for (;;) {
        thread_pool_task_t task;
        {
            int lock_error;
            raii_stack(scoped_lock) _lock = new_stack(scoped_lock, &lock_error, &pool->mutex);
            if (unlikely(lock_error != 0)) return NULL;

            while (pool->queued_count == 0 && !pool->stopping) {
                pthread_cond_wait(&pool->task_available, mutex_native(&pool->mutex));
            }

            if (pool->queued_count == 0 && pool->stopping) return NULL;

            task = pool->queue[pool->queue_head];
            pool->queue_head = (pool->queue_head + 1) % pool->queue_capacity;
            --pool->queued_count;
            ++pool->active_count;
            pthread_cond_signal(&pool->queue_space_available);
        }

        task.func(task.arg);

        {
            int lock_error;
            raii_stack(scoped_lock) _lock = new_stack(scoped_lock, &lock_error, &pool->mutex);
            if (unlikely(lock_error != 0)) return NULL;

            --pool->active_count;
            if (pool->queued_count == 0 && pool->active_count == 0) {
                pthread_cond_broadcast(&pool->idle);
            }
        }
    }
}

thread_pool_error_t thread_pool_init(
    thread_pool_t *pool,
    size_t thread_count,
    size_t queue_capacity
) {
    if (unlikely(pool == NULL)) return THREAD_POOL_INVALID_DESTINATION;
    if (unlikely(thread_pool_is_valid(pool))) return THREAD_POOL_INVALID_DESTINATION;
    if (unlikely(thread_count == 0 || queue_capacity == 0)) return THREAD_POOL_INVALID_SIZE;

    thread_pool_impl_t *impl = calloc(1, sizeof(*impl));
    if (unlikely(impl == NULL)) return THREAD_POOL_ALLOCATION_FAILED;
    thread_pool_error_t error = THREAD_POOL_ALLOCATION_FAILED;
    size_t created_thread_count = 0;

    impl->threads = calloc(thread_count, sizeof(*impl->threads));
    impl->queue = calloc(queue_capacity, sizeof(*impl->queue));
    if (unlikely(impl->threads == NULL || impl->queue == NULL)) goto allocation_failed;

    impl->thread_count = thread_count;
    impl->queue_capacity = queue_capacity;
    mutex_reset(&impl->mutex);
    error = THREAD_POOL_SYSTEM_ERROR;
    if (unlikely(mutex_init(&impl->mutex) != 0)) goto allocation_failed;
    if (unlikely(pthread_cond_init(&impl->task_available, NULL) != 0)) goto mutex_initialized;
    if (unlikely(pthread_cond_init(&impl->queue_space_available, NULL) != 0)) goto task_condition_initialized;
    if (unlikely(pthread_cond_init(&impl->idle, NULL) != 0)) goto space_condition_initialized;

    for (; created_thread_count < thread_count; ++created_thread_count) {
        if (unlikely(pthread_create(&impl->threads[created_thread_count], NULL,
                thread_pool_worker, impl) != 0)) {
            goto thread_creation_failed;
        }
    }

    pool->impl = impl;
    return THREAD_POOL_OK;

thread_creation_failed:
    mutex_lock(&impl->mutex);
    impl->stopping = true;
    pthread_cond_broadcast(&impl->task_available);
    mutex_unlock(&impl->mutex);

    for (size_t index = 0; index < created_thread_count; ++index) {
        pthread_join(impl->threads[index], NULL);
    }

    pthread_cond_destroy(&impl->idle);
space_condition_initialized:
    pthread_cond_destroy(&impl->queue_space_available);
task_condition_initialized:
    pthread_cond_destroy(&impl->task_available);
mutex_initialized:
    mutex_destroy(&impl->mutex);
allocation_failed:
    free(impl->queue);
    free(impl->threads);
    free(impl);
    return error;
}

bool thread_pool_add_task(thread_pool_t *pool, thread_pool_task_t task) {
    return thread_pool_add_tasks(pool, &task, 1);
}

bool thread_pool_add_tasks(
    thread_pool_t *pool,
    const thread_pool_task_t *tasks,
    size_t task_count
) {
    if (!thread_pool_is_valid(pool)) return false;
    if (task_count == 0) return true;
    if (tasks == NULL) return false;

    thread_pool_impl_t *impl = pool->impl;
    for (size_t index = 0; index < task_count; ++index) {
        if (tasks[index].func == NULL) return false;
    }

    size_t submitted_count = 0;
    while (submitted_count < task_count) {
        int lock_error;
        raii_stack(scoped_lock) _lock = new_stack(scoped_lock, &lock_error, &impl->mutex);
        if (unlikely(lock_error != 0)) return false;

        while (impl->queued_count == impl->queue_capacity && !impl->stopping) {
            pthread_cond_wait(&impl->queue_space_available, mutex_native(&impl->mutex));
        }

        if (impl->stopping) return false;

        const size_t available_count = impl->queue_capacity - impl->queued_count;
        const size_t remaining_count = task_count - submitted_count;
        const size_t batch_count = available_count < remaining_count ? available_count : remaining_count;

        for (size_t index = 0; index < batch_count; ++index) {
            impl->queue[impl->queue_tail] = tasks[submitted_count + index];
            impl->queue_tail = (impl->queue_tail + 1) % impl->queue_capacity;
        }
        impl->queued_count += batch_count;
        submitted_count += batch_count;

        if (batch_count == 1) {
            pthread_cond_signal(&impl->task_available);
        } else {
            if (batch_count <= impl->thread_count) {
                pthread_cond_broadcast(&impl->task_available);
            } else {
                for (size_t i = 0; i < batch_count; ++i) {
                    pthread_cond_signal(&impl->task_available);
                }
            }
        }
    }

    return true;
}

void thread_pool_wait(thread_pool_t *pool) {
    if (!thread_pool_is_valid(pool)) return;
    thread_pool_impl_t *impl = pool->impl;

    int lock_error;
    raii_stack(scoped_lock) _lock = new_stack(scoped_lock, &lock_error, &impl->mutex);
    if (unlikely(lock_error != 0)) return;

    while (impl->queued_count != 0 || impl->active_count != 0) {
        pthread_cond_wait(&impl->idle, mutex_native(&impl->mutex));
    }
}

size_t thread_pool_thread_count(const thread_pool_t *pool) {
    return thread_pool_is_valid(pool) ? pool->impl->thread_count : 0;
}

void thread_pool_destroy(thread_pool_t *pool) {
    if (!thread_pool_is_valid(pool)) return;
    thread_pool_impl_t *impl = pool->impl;

    {
        int lock_error;
        raii_stack(scoped_lock) _lock = new_stack(scoped_lock, &lock_error, &impl->mutex);
        if (unlikely(lock_error != 0)) return;

        impl->stopping = true;
        pthread_cond_broadcast(&impl->task_available);
        pthread_cond_broadcast(&impl->queue_space_available);
    }

    for (size_t index = 0; index < impl->thread_count; ++index) {
        pthread_join(impl->threads[index], NULL);
    }

    pthread_cond_destroy(&impl->idle);
    pthread_cond_destroy(&impl->queue_space_available);
    pthread_cond_destroy(&impl->task_available);
    mutex_destroy(&impl->mutex);
    free(impl->queue);
    free(impl->threads);
    free(impl);
    thread_pool_reset(pool);
}

const char *thread_pool_error_string(thread_pool_error_t error) {
    switch (error) {
        case THREAD_POOL_OK:
            return "success";
        case THREAD_POOL_INVALID_SIZE:
            return "thread and queue sizes must be positive";
        case THREAD_POOL_INVALID_DESTINATION:
            return "invalid thread pool destination";
        case THREAD_POOL_ALLOCATION_FAILED:
            return "thread pool allocation failed";
        case THREAD_POOL_SYSTEM_ERROR:
            return "operating system thread error";
    }
    return "unknown thread pool error";
}
