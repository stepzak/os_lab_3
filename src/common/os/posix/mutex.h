#ifndef MUTEX_H
#define MUTEX_H

#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "../../std.h"

static inline bool stack_mutex_valid(pthread_mutex_t *mutex) {
    if (unlikely(!mutex)) return false;
    
    static const pthread_mutex_t zero_mutex = {0}; 
    return memcmp(mutex, &zero_mutex, sizeof(pthread_mutex_t)) != 0;
}

static inline bool mutex_valid(pthread_mutex_t **mutex) {
    if (unlikely(!mutex || !*mutex)) return false;
    return stack_mutex_valid(*mutex);
}

static inline void stack_mutex_destroy(pthread_mutex_t *mutex) {
    if (unlikely(!mutex)) return;
    pthread_mutex_destroy(mutex);
}

static inline void stack_mutex_reset(pthread_mutex_t *mutex) {
    if (unlikely(!mutex)) return;

    if (unlikely(pthread_mutex_destroy(mutex) != 0)) return;

    
    if (unlikely(pthread_mutex_init(mutex, NULL) != 0)) {
        memset(mutex, 0, sizeof(pthread_mutex_t));
    }
}

DEFINE_RAII_TYPE(stack_mutex, pthread_mutex_t, NULL, NULL, stack_mutex_destroy, stack_mutex_valid, stack_mutex_reset)

static inline pthread_mutex_t* mutex_create(void) {
    pthread_mutex_t* mutex = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
    if (unlikely(!mutex)) return NULL;

    if (unlikely(pthread_mutex_init(mutex, NULL) != 0)) {
        free(mutex);
        return NULL;
    }
    return mutex;
}

static inline void mutex_destroy(pthread_mutex_t **mutex) {
    if (unlikely(!mutex || !*mutex)) return;
    pthread_mutex_destroy(*mutex);
    free(*mutex);
    *mutex = NULL;
}

static inline void mutex_reset(pthread_mutex_t **mutex) {
    if (unlikely(!mutex || !*mutex)) return;

    pthread_mutex_destroy(*mutex);

    if (unlikely(pthread_mutex_init(*mutex, NULL) != 0)) {
        free(*mutex);
        *mutex = NULL;
    }
}

DEFINE_RAII_TYPE(raii_mutex, pthread_mutex_t*, mutex_create, NULL, mutex_destroy, mutex_valid, mutex_reset)

static inline pthread_mutex_t* scoped_lock_init(pthread_mutex_t *mutex) {
    if (unlikely(!mutex)) return NULL;
    pthread_mutex_lock(mutex);
    return mutex;
}

static inline void scoped_lock_destroy(pthread_mutex_t **mutex) {
    if (unlikely(!mutex || !*mutex)) return;
    pthread_mutex_unlock(*mutex);
}

static inline void scoped_lock_reset(pthread_mutex_t **mutex) {
    if (unlikely(!mutex || !*mutex)) return;
    pthread_mutex_unlock(*mutex);
    pthread_mutex_lock(*mutex);
}

DEFINE_RAII_TYPE(scoped_lock, pthread_mutex_t*, scoped_lock_init, NULL, scoped_lock_destroy, cleanup_is_valid_ptr, scoped_lock_reset)

#define __MUTEX_PTR_GENERIC(mtx) _Generic(&(mtx), \
    pthread_mutex_t**: (mtx), \
    pthread_mutex_t*:  &(mtx)  \
)

#define WITH_LOCK(mtx) raii(scoped_lock) __lock_guard__##__LINE__ = create(scoped_lock, __MUTEX_PTR_GENERIC(mtx))

#endif 
