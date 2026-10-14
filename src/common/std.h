#ifndef STD_H
#define STD_H

#include <stdbool.h>

static inline bool cleanup_is_valid_ptr(void *const *ptr) {
    return ptr && *ptr != NULL;
}

static inline void nullify_ptr(void **ptr) {
    *ptr = NULL;
}

static inline bool always_valid(void *const *ptr) {
    (void)ptr;
    return true;
}

#endif //STD_H
