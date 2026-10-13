#include "matrix.h"

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

matrix_error_t matrix_init(matrix_t *matrix, size_t rows, size_t cols) {
    if (unlikely(matrix == NULL)) return MATRIX_ALLOCATION_FAILED;

    if (unlikely(matrix_is_valid(matrix))) return MATRIX_INVALID_DEST;

    if (rows == 0 || cols == 0) return MATRIX_INVALID_SIZE;

    if (rows > SIZE_MAX / cols) return MATRIX_SIZE_OVERFLOW;

    size_t element_count = rows * cols;

    if (element_count > SIZE_MAX / sizeof(double)) return MATRIX_SIZE_OVERFLOW;

    double* data = (double*) malloc(element_count* sizeof(double));
    if (unlikely(data == NULL)) return MATRIX_ALLOCATION_FAILED;
    matrix->rows = rows;
    matrix->cols = cols;
    matrix->data = data;
    matrix->stride = cols;
    matrix_fill(matrix, 0.0);
    return MATRIX_OK;
}

bool matrix_is_valid(const matrix_t *matrix) {
    return matrix != NULL
        && matrix->data != NULL
        && matrix->rows > 0
        && matrix->cols > 0
        && matrix->stride >= matrix->cols;
}

void matrix_reset(matrix_t *matrix) {
    if (unlikely(matrix == NULL)) return;

    matrix->rows = 0;
    matrix->cols = 0;
    matrix->data = NULL;
    matrix->stride = 0;
}

void matrix_destroy(matrix_t *matrix) {
    if (unlikely(matrix == NULL)) return;

    free(matrix->data);
    matrix_reset(matrix);
}

matrix_error_t matrix_copy(matrix_t* dest, const matrix_t* src) {
    if (unlikely(!matrix_is_valid(src))) return MATRIX_INVALID_SOURCE;
    if (unlikely(!matrix_is_valid(dest))) return MATRIX_INVALID_DEST;
    if (dest == src) return MATRIX_OK;
    if (dest->rows != src->rows) return MATRIX_SIZE_MISMATCH;
    if (dest->cols != src->cols) return MATRIX_SIZE_MISMATCH;

    for (size_t i = 0; i < src->rows; ++i) {
        memcpy(matrix_row(dest, i), matrix_const_row(src, i), src->cols*sizeof(double));
    }
    return MATRIX_OK;
}

matrix_error_t matrix_clone(matrix_t *destination, const matrix_t *source) {
    if (unlikely(!matrix_is_valid(source))) return MATRIX_INVALID_SOURCE;
    if (destination == NULL || destination == source) {
        return MATRIX_INVALID_DEST;
    }

    if (unlikely(matrix_is_valid(destination))) {
        return MATRIX_INVALID_DEST;
    }

    matrix_error_t err = matrix_init(destination, source->rows, source->cols);
    if (unlikely(err != MATRIX_OK)) {
        matrix_destroy(destination);
        return err;
    }

    err = matrix_copy(destination, source);
    if (unlikely(err != MATRIX_OK)) {
        matrix_destroy(destination);
        return err;
    }
    return MATRIX_OK;
}

void matrix_swap(matrix_t *left, matrix_t *right) {
    assert(left != NULL);
    assert(right != NULL);
    if (left == right) return;

    matrix_t temp = *left;
    *left = *right;
    *right = temp;
}

void matrix_fill(matrix_t *matrix, double value) {
    if (unlikely(!matrix_is_valid(matrix))) return;

    for (size_t i = 0; i < matrix->rows; ++i) {
        for (size_t j = 0; j < matrix->cols; ++j) {
            *matrix_at(matrix, i, j) = value;
        }
    }
}