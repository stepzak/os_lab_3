#ifndef MUTEX_H
#define MUTEX_H

#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>

#include "common/raii.h"

typedef struct {
    pthread_mutex_t native;
    bool initialized;
} mutex_t;

static void mutex_reset(mutex_t *mutex) {
    if (mutex == NULL) return;
    mutex->initialized = false;
}

static bool mutex_is_valid(const mutex_t *mutex) {
    return mutex != NULL && mutex->initialized;
}

static int mutex_init(mutex_t *mutex) {
    if (mutex == NULL) return EINVAL;
    if (mutex_is_valid(mutex)) return EBUSY;

    const int error = pthread_mutex_init(&mutex->native, NULL);
    if (error == 0) mutex->initialized = true;
    return error;
}

static void mutex_destroy(mutex_t *mutex) {
    if (!mutex_is_valid(mutex)) return;

    const int error = pthread_mutex_destroy(&mutex->native);
    assert(error == 0);
    (void)error;
    mutex->initialized = false;
}

static int mutex_lock(mutex_t *mutex) {
    if (!mutex_is_valid(mutex)) return EINVAL;
    return pthread_mutex_lock(&mutex->native);
}

static int mutex_unlock(mutex_t *mutex) {
    if (!mutex_is_valid(mutex)) return EINVAL;
    return pthread_mutex_unlock(&mutex->native);
}

static pthread_mutex_t *mutex_native(mutex_t *mutex) {
    return mutex_is_valid(mutex) ? &mutex->native : NULL;
}

static bool mutex_init_succeeded(int error) {
    return error == 0;
}

DECLARE_SHARED_RAII_TYPES(
    mutex,
    mutex_t,
    mutex_init,
    mutex_destroy,
    mutex_is_valid,
    mutex_reset
)
DEFINE_RAII_INIT_VALIDATION(mutex, mutex_init_succeeded)

typedef struct {
    mutex_t *mutex;
    bool owns_lock;
} scoped_lock_t;

static void scoped_lock_reset(scoped_lock_t *lock) {
    if (lock == NULL) return;
    lock->mutex = NULL;
    lock->owns_lock = false;
}

static bool scoped_lock_is_valid(const scoped_lock_t *lock) {
    return lock != NULL && lock->owns_lock && mutex_is_valid(lock->mutex);
}

static int scoped_lock_init(scoped_lock_t *lock, mutex_t *mutex) {
    if (lock == NULL || !mutex_is_valid(mutex)) return EINVAL;
    if (scoped_lock_is_valid(lock)) return EBUSY;

    const int error = mutex_lock(mutex);
    if (error == 0) {
        lock->mutex = mutex;
        lock->owns_lock = true;
    }
    return error;
}

static void scoped_lock_destroy(scoped_lock_t *lock) {
    if (!scoped_lock_is_valid(lock)) return;

    const int error = mutex_unlock(lock->mutex);
    assert(error == 0);
    (void)error;
    scoped_lock_reset(lock);
}

static bool scoped_lock_init_succeeded(int error) {
    return error == 0;
}

DECLARE_SHARED_RAII_TYPES(
    scoped_lock,
    scoped_lock_t,
    scoped_lock_init,
    scoped_lock_destroy,
    scoped_lock_is_valid,
    scoped_lock_reset
)
DEFINE_RAII_TRIVIAL_MOVE(
    RAII_STACK_TYPE(scoped_lock),
    scoped_lock_is_valid,
    scoped_lock_reset
)
DEFINE_RAII_INIT_VALIDATION(scoped_lock, scoped_lock_init_succeeded)

#endif //MUTEX_H
