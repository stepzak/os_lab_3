#ifndef MUTEX_H
#define MUTEX_H

#include <pthread.h>
#include "../../std.h"

static inline void mutex_unlock(pthread_mutex_t **mutex) {
    if (likely(mutex && *mutex)) {
        pthread_mutex_unlock(*mutex);
    }
}

DEFINE_CLEANUP_TYPE(
    lock_guard_t,
    pthread_mutex_t*,
    mutex_unlock,
    NULL
)

#define LOCK_GUARD(mtx) \
    pthread_mutex_lock(&(mtx)); \
    CLEANUP(lock_guard_t) __guard_##__LINE__ = &(mtx);


#endif //MUTEX_H
