#ifndef CONVOLUTION_H
#define CONVOLUTION_H
#include "common/os/thread_pool.h"
#include "matrix.h"

typedef enum {
    CONVOLUTION_OK = 0,
    CONVOLUTION_INVALID_SOURCE,
    CONVOLUTION_INVALID_DESTINATION,
    CONVOLUTION_INVALID_KERNEL,
    CONVOLUTION_SIZE_MISMATCH,
    CONVOLUTION_IN_PLACE,
    CONVOLUTION_ALLOCATION_FAILED,
    CONVOLUTION_THREAD_POOL_ERROR
} convolution_error_t;

void convolution_apply_rows(matrix_t *destination, const matrix_t *source, const matrix_t *kernel,
    size_t row_begin, size_t row_end);

convolution_error_t convolution_apply(matrix_t *destination, const matrix_t *source, const matrix_t *kernel);

convolution_error_t convolution_apply_repeated(matrix_t *destination, const matrix_t *source,
    const matrix_t *kernel,
    size_t repeat_count);

convolution_error_t convolution_apply_parallel(matrix_t *destination, const matrix_t *source,
    const matrix_t *kernel, thread_pool_t *pool);

convolution_error_t convolution_apply_repeated_parallel(matrix_t *destination, const matrix_t *source,
    const matrix_t *kernel, size_t repeat_count, thread_pool_t *pool);

const char *convolution_error_string(convolution_error_t error);

#endif //CONVOLUTION_H
