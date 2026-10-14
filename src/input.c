#include "input.h"

#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <stdlib.h>

typedef enum {
    TOKEN_OK = 0,
    TOKEN_END,
    TOKEN_INVALID,
    TOKEN_IO_ERROR
} token_error_t;

static token_error_t read_token(FILE *stream, char token[128]) {
    int result = fscanf(stream, "%127s", token);
    if (result == 1) return TOKEN_OK;
    if (ferror(stream)) return TOKEN_IO_ERROR;
    return TOKEN_END;
}

static token_error_t read_size(FILE *stream, size_t *value) {
    char token[128];
    token_error_t error = read_token(stream, token);
    if (error != TOKEN_OK) return error;

    if (token[0] == '-') return TOKEN_INVALID;

    char *end = NULL;
    errno = 0;
    uintmax_t parsed = strtoumax(token, &end, 10);
    if (end == token || *end != '\0' || errno == ERANGE || parsed > SIZE_MAX) {
        return TOKEN_INVALID;
    }

    *value = (size_t) parsed;
    return TOKEN_OK;
}

static token_error_t read_double(FILE *stream, double *value) {
    char token[128];
    token_error_t error = read_token(stream, token);
    if (error != TOKEN_OK) return error;

    char *end = NULL;
    errno = 0;
    double parsed = strtod(token, &end);
    if (end == token || *end != '\0' || errno == ERANGE || !isfinite(parsed)) {
        return TOKEN_INVALID;
    }

    *value = parsed;
    return TOKEN_OK;
}

static input_error_t token_to_input_error(token_error_t error) {
    switch (error) {
        case TOKEN_OK:
            return INPUT_OK;
        case TOKEN_END:
            return INPUT_UNEXPECTED_END;
        case TOKEN_INVALID:
            return INPUT_INVALID_NUMBER;
        case TOKEN_IO_ERROR:
            return INPUT_IO_ERROR;
    }
    return INPUT_INVALID_NUMBER;
}

static input_error_t matrix_to_input_error(matrix_error_t error) {
    if (error == MATRIX_OK) return INPUT_OK;
    if (error == MATRIX_ALLOCATION_FAILED) return INPUT_ALLOCATION_FAILED;
    return INPUT_MATRIX_SIZE_ERROR;
}

static input_error_t read_matrix_elements(FILE *stream, matrix_t *matrix) {
    for (size_t row = 0; row < matrix->rows; ++row) {
        double *values = matrix_row(matrix, row);
        for (size_t col = 0; col < matrix->cols; ++col) {
            token_error_t error = read_double(stream, &values[col]);
            if (error != TOKEN_OK) return token_to_input_error(error);
        }
    }
    return INPUT_OK;
}

input_error_t input_read(
    FILE *stream,
    matrix_t *source,
    matrix_t *kernel,
    size_t *repeat_count) {
    if (stream == NULL || source == NULL || kernel == NULL
        || repeat_count == NULL || source == kernel) {
        return INPUT_INVALID_ARGUMENT;
    }
    if (matrix_is_valid(source) || matrix_is_valid(kernel)) {
        return INPUT_DESTINATION_NOT_EMPTY;
    }

    size_t rows;
    size_t cols;
    size_t window_size;
    size_t parsed_repeat_count;

    token_error_t token_error = read_size(stream, &rows);
    if (token_error != TOKEN_OK) return token_to_input_error(token_error);
    token_error = read_size(stream, &cols);
    if (token_error != TOKEN_OK) return token_to_input_error(token_error);
    token_error = read_size(stream, &window_size);
    if (token_error != TOKEN_OK) return token_to_input_error(token_error);
    token_error = read_size(stream, &parsed_repeat_count);
    if (token_error != TOKEN_OK) return token_to_input_error(token_error);

    if (window_size == 0 || window_size % 2 == 0) {
        return INPUT_INVALID_WINDOW_SIZE;
    }

    raii_stack(matrix) parsed_source = create_stack(matrix);
    raii_stack(matrix) parsed_kernel = create_stack(matrix);

    matrix_error_t matrix_error = init(matrix, parsed_source, rows, cols);
    if (matrix_error != MATRIX_OK) return matrix_to_input_error(matrix_error);

    matrix_error = init(matrix, parsed_kernel, window_size, window_size);
    if (matrix_error != MATRIX_OK) return matrix_to_input_error(matrix_error);

    input_error_t input_error = read_matrix_elements(stream, &parsed_source);
    if (input_error != INPUT_OK) return input_error;

    input_error = read_matrix_elements(stream, &parsed_kernel);
    if (input_error != INPUT_OK) return input_error;

    *source = move_stack(matrix, parsed_source);
    *kernel = move_stack(matrix, parsed_kernel);
    *repeat_count = parsed_repeat_count;
    return INPUT_OK;
}

const char *input_error_string(input_error_t error) {
    switch (error) {
        case INPUT_OK:
            return "success";
        case INPUT_INVALID_ARGUMENT:
            return "invalid input_read argument";
        case INPUT_DESTINATION_NOT_EMPTY:
            return "destination matrix is not empty";
        case INPUT_UNEXPECTED_END:
            return "unexpected end of input";
        case INPUT_INVALID_NUMBER:
            return "invalid numeric value";
        case INPUT_INVALID_WINDOW_SIZE:
            return "window size must be a positive odd number";
        case INPUT_MATRIX_SIZE_ERROR:
            return "invalid or excessively large matrix size";
        case INPUT_ALLOCATION_FAILED:
            return "memory allocation failed";
        case INPUT_IO_ERROR:
            return "input/output error";
    }
    return "unknown input error";
}
