#ifndef MATRIX_H
#define MATRIX_H

#include <assert.h>
#include <stddef.h>
#include <stdbool.h>
#include "common/raii.h"

typedef enum {
    MATRIX_OK = 0,
    MATRIX_INVALID_SIZE,
    MATRIX_SIZE_OVERFLOW,
    MATRIX_ALLOCATION_FAILED,
    MATRIX_SIZE_MISMATCH,
    MATRIX_INVALID_SOURCE,
    MATRIX_INVALID_DEST
} matrix_error_t;

typedef struct {
    double *data;
    size_t rows;
    size_t cols;
    size_t stride;
} matrix_t;

void matrix_reset(matrix_t *matrix);
bool matrix_is_valid(const matrix_t *matrix);
matrix_error_t matrix_init(matrix_t *matrix, size_t rows, size_t cols);

static bool matrix_init_is_valid(matrix_error_t err) {
    return err == MATRIX_OK;
}

void matrix_destroy(matrix_t *matrix);
void matrix_fill(matrix_t *matrix, double value);
matrix_error_t matrix_copy(matrix_t *destination, const matrix_t *source);
matrix_error_t matrix_clone(matrix_t *destination, const matrix_t *source);
void matrix_swap(matrix_t *left, matrix_t *right);

static matrix_error_t matrix_heap_clone(matrix_t **destination, matrix_t *const *source) {
    if (unlikely(destination == NULL || *destination == NULL)) {
        return MATRIX_ALLOCATION_FAILED;
    }

    if (unlikely(source == NULL || !matrix_is_valid(*source))) {
        return MATRIX_INVALID_SOURCE;
    }

    return matrix_clone(*destination, *source);
}

static double *matrix_row(matrix_t *matrix, size_t row) {
    assert(matrix != NULL);
    assert(matrix_is_valid(matrix));
    assert(row < matrix->rows);
    return matrix->data + row * matrix->stride;
}

static const double *matrix_const_row(const matrix_t *matrix, size_t row) {
    assert(matrix != NULL);
    assert(matrix_is_valid(matrix));
    assert(row < matrix->rows);
    return matrix->data + row * matrix->stride;
}

static double *matrix_at(matrix_t *matrix, size_t row, size_t col) {
    assert(matrix != NULL);
    assert(matrix_is_valid(matrix));
    assert(row < matrix->rows);
    assert(col < matrix->cols);
    return matrix_row(matrix, row) + col;
}

static const double *matrix_const_at(const matrix_t *matrix, size_t row, size_t col) {
    assert(matrix != NULL);
    assert(matrix_is_valid(matrix));
    assert(row < matrix->rows);
    assert(col < matrix->cols);
    return matrix_const_row(matrix, row) + col;
}

DECLARE_SHARED_RAII_TYPES(
    matrix, matrix_t, matrix_init, matrix_destroy, matrix_is_valid, matrix_reset)

DEFINE_RAII_TRIVIAL_MOVE(RAII_STACK_TYPE(matrix), matrix_is_valid, matrix_reset)
DEFINE_RAII_INIT_VALIDATION(matrix, matrix_init_is_valid)
DEFINE_RAII_CLONE(RAII_STACK_TYPE(matrix), matrix_clone)
DEFINE_RAII_CLONE(RAII_HEAP_TYPE(matrix), matrix_heap_clone)

#endif