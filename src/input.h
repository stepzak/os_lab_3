#ifndef INPUT_H
#define INPUT_H

#include <stddef.h>
#include <stdio.h>

#include "matrix.h"

typedef enum {
    INPUT_OK = 0,
    INPUT_INVALID_ARGUMENT,
    INPUT_DESTINATION_NOT_EMPTY,
    INPUT_UNEXPECTED_END,
    INPUT_INVALID_NUMBER,
    INPUT_INVALID_WINDOW_SIZE,
    INPUT_MATRIX_SIZE_ERROR,
    INPUT_ALLOCATION_FAILED,
    INPUT_IO_ERROR
} input_error_t;

/*
Expected format (whitespace is insignificant):

     rows cols
     window_size repeat_count
     <rows * cols matrix elements>
     <window_size * window_size kernel elements>

source and kernel must be distinct matrices in their reset state. On failure they remain in that state, and repeat_count is not modified.
 */
input_error_t input_read(
    FILE *stream,
    matrix_t *source,
    matrix_t *kernel,
    size_t *repeat_count);

const char *input_error_string(input_error_t error);

#endif
