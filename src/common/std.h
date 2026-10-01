#ifndef STD_H
#define STD_H

#include <stdbool.h>
#include <stddef.h>

#include "raii.h"
#include "unlikely.h"

static inline bool cleanup_is_valid_ptr(void *const *ptr) {
    return ptr && *ptr != NULL;
}

static inline void nullify_ptr(void **ptr) {
    *ptr = NULL;
}

#endif //STD_H
