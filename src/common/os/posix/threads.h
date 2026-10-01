#ifndef THREAD_H
#define THREAD_H

#include <pthread.h>
#include <stdbool.h>
#include "../../std.h"

typedef struct {
    pthread_t id;
    bool is_alive;
} ThreadCtx;

static inline void thread_reset(ThreadCtx *ctx) {
    ctx->is_alive = false;
    ctx->id = 0;
}

static inline bool thread_is_alive(ThreadCtx *ctx) {
    return ctx->is_alive;
}

static inline void thread_dtor(ThreadCtx *thread) {
    if (unlikely(!thread)) return;
    pthread_join(*thread, NULL);
}



DEFINE_CLEANUP_TYPE(thread_cleaned, pthread_t, thread_dtor, )


#endif //THREAD_H
