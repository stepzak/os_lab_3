#include "convolution.h"

#include <stdint.h>
#include <stdlib.h>

typedef struct {
    matrix_t *destination;
    const matrix_t *source;
    const matrix_t *kernel;
    size_t row_begin;
    size_t row_end;
} convolution_task_context_t;

static const size_t convolution_min_operations_per_task = 64 * 1024;

static convolution_error_t convolution_validate_common(
    const matrix_t *destination,
    const matrix_t *source,
    const matrix_t *kernel
) {
    if (unlikely(!matrix_is_valid(destination))) return CONVOLUTION_INVALID_DESTINATION;
    if (unlikely(!matrix_is_valid(source))) return CONVOLUTION_INVALID_SOURCE;
    if (unlikely(!matrix_is_valid(kernel))) return CONVOLUTION_INVALID_KERNEL;

    if (unlikely(destination->rows != source->rows ||
                 destination->cols != source->cols)) {
        return CONVOLUTION_SIZE_MISMATCH;
    }

    if (unlikely(kernel->rows != kernel->cols || kernel->rows % 2 == 0)) {
        return CONVOLUTION_INVALID_KERNEL;
    }

    return CONVOLUTION_OK;
}

static convolution_error_t convolution_validate(
    const matrix_t *destination,
    const matrix_t *source,
    const matrix_t *kernel
) {
    const convolution_error_t error = convolution_validate_common(destination, source, kernel);
    if (unlikely(error != CONVOLUTION_OK)) return error;

    if (unlikely(destination->data == source->data || destination->data == kernel->data)) {
        return CONVOLUTION_IN_PLACE;
    }

    return CONVOLUTION_OK;
}

static void convolution_accumulate_row(
    double *restrict destination,
    const double *restrict source,
    size_t element_count,
    double weight
) {
    for (size_t index = 0; index < element_count; ++index) {
        destination[index] += source[index] * weight;
    }
}

void convolution_apply_rows(
    matrix_t *destination,
    const matrix_t *source,
    const matrix_t *kernel,
    size_t row_begin,
    size_t row_end
) {
    assert(convolution_validate(destination, source, kernel) == CONVOLUTION_OK);
    assert(row_begin <= row_end);
    assert(row_end <= source->rows);

    const size_t radius = kernel->rows / 2;

    for (size_t row = row_begin; row < row_end; ++row) {
        double *destination_row = matrix_row(destination, row);
        for (size_t col = 0; col < destination->cols; ++col) {
            destination_row[col] = 0.0;
        }

        for (size_t kernel_row = 0; kernel_row < kernel->rows; ++kernel_row) {
            size_t source_row_index;
            if (kernel_row < radius) {
                const size_t offset = radius - kernel_row;
                if (row < offset) continue;
                source_row_index = row - offset;
            } else {
                const size_t offset = kernel_row - radius;
                if (offset >= source->rows - row) continue;
                source_row_index = row + offset;
            }

            const double *source_row = matrix_const_row(source, source_row_index);
            const double *reflected_kernel_row = matrix_const_row(kernel, kernel->rows - 1 - kernel_row);

            for (size_t kernel_col = 0; kernel_col < kernel->cols; ++kernel_col) {
                size_t destination_col_begin;
                size_t source_col_begin;
                size_t element_count;

                if (kernel_col < radius) {
                    const size_t offset = radius - kernel_col;
                    if (offset >= source->cols) continue;
                    destination_col_begin = offset;
                    source_col_begin = 0;
                    element_count = source->cols - offset;
                } else {
                    const size_t offset = kernel_col - radius;
                    if (offset >= source->cols) continue;
                    destination_col_begin = 0;
                    source_col_begin = offset;
                    element_count = source->cols - offset;
                }

                const double weight = reflected_kernel_row[kernel->cols - 1 - kernel_col];
                convolution_accumulate_row(
                    destination_row + destination_col_begin,
                    source_row + source_col_begin,
                    element_count,
                    weight);
            }
        }
    }
}

convolution_error_t convolution_apply(matrix_t *destination, const matrix_t *source, const matrix_t *kernel) {
    const convolution_error_t error = convolution_validate(destination, source, kernel);
    if (unlikely(error != CONVOLUTION_OK)) return error;

    convolution_apply_rows(destination, source, kernel, 0, source->rows);
    return CONVOLUTION_OK;
}

convolution_error_t convolution_apply_repeated(matrix_t *destination, const matrix_t *source,
    const matrix_t *kernel, size_t repeat_count) {
    const convolution_error_t common_error = convolution_validate_common(destination, source, kernel);
    if (unlikely(common_error != CONVOLUTION_OK)) return common_error;
    if (unlikely(destination->data == kernel->data)) return CONVOLUTION_IN_PLACE;

    if (repeat_count == 0) {
        if (destination->data == source->data) return CONVOLUTION_OK;
        return matrix_copy(destination, source) == MATRIX_OK
            ? CONVOLUTION_OK
            : CONVOLUTION_INVALID_DESTINATION;
    }

    if (unlikely(destination->data == source->data)) return CONVOLUTION_IN_PLACE;

    raii_stack(matrix) temporary = create_stack(matrix);
    if (repeat_count > 1) {
        const matrix_error_t init_error = init(matrix, temporary, source->rows, source->cols);
        if (unlikely(init_error != MATRIX_OK)) return CONVOLUTION_ALLOCATION_FAILED;
    }

    const matrix_t *current_source = source;
    matrix_t *current_destination = repeat_count % 2 == 0 ? &temporary : destination;

    for (size_t iteration = 0; iteration < repeat_count; ++iteration) {
        convolution_apply_rows(current_destination, current_source, kernel, 0, source->rows);
        current_source = current_destination;
        current_destination = current_destination == destination ? &temporary : destination;
    }

    return CONVOLUTION_OK;
}

static void convolution_task(void *argument) {
    convolution_task_context_t *context = argument;
    convolution_apply_rows(context->destination, context->source, context->kernel,
        context->row_begin, context->row_end);
}

static size_t convolution_task_count(
    const matrix_t *source,
    const matrix_t *kernel,
    const thread_pool_t *pool
) {
    const size_t thread_count = thread_pool_thread_count(pool);
    if (thread_count == 0) return 0;

    size_t minimum_rows_per_task = 1;
    if (kernel->rows <= convolution_min_operations_per_task / kernel->cols) {
        const size_t kernel_element_count = kernel->rows * kernel->cols;
        if (source->cols <= convolution_min_operations_per_task / kernel_element_count) {
            const size_t operations_per_row = source->cols * kernel_element_count;
            minimum_rows_per_task =
                convolution_min_operations_per_task / operations_per_row
                + (convolution_min_operations_per_task % operations_per_row != 0);
        }
    }

    const size_t useful_task_count = source->rows / minimum_rows_per_task
        + (source->rows % minimum_rows_per_task != 0);
    return thread_count < useful_task_count ? thread_count : useful_task_count;
}

static convolution_task_context_t *convolution_create_contexts(size_t task_count) {
    if (task_count == 0 || task_count > SIZE_MAX / sizeof(convolution_task_context_t)) {
        return NULL;
    }
    return malloc(task_count * sizeof(convolution_task_context_t));
}

static convolution_error_t convolution_dispatch(
    matrix_t *destination,
    const matrix_t *source,
    const matrix_t *kernel,
    thread_pool_t *pool,
    convolution_task_context_t *contexts,
    thread_pool_task_t *tasks,
    size_t task_count
) {
    if (task_count == 1) {
        convolution_apply_rows(destination, source, kernel, 0, source->rows);
        return CONVOLUTION_OK;
    }

    const size_t rows_per_task = source->rows / task_count;
    const size_t remainder = source->rows % task_count;
    size_t row_begin = 0;

    for (size_t index = 0; index < task_count; ++index) {
        const size_t row_count = rows_per_task + (index < remainder ? 1 : 0);
        contexts[index] = (convolution_task_context_t) {
            .destination = destination,
            .source = source,
            .kernel = kernel,
            .row_begin = row_begin,
            .row_end = row_begin + row_count
        };
        row_begin += row_count;

        tasks[index] = (thread_pool_task_t) {
            .func = convolution_task,
            .arg = &contexts[index]
        };
    }

    if (unlikely(!thread_pool_add_tasks(pool, tasks, task_count))) {
        thread_pool_wait(pool);
        return CONVOLUTION_THREAD_POOL_ERROR;
    }
    thread_pool_wait(pool);
    return CONVOLUTION_OK;
}

convolution_error_t convolution_apply_parallel(matrix_t *destination, const matrix_t *source,
    const matrix_t *kernel, thread_pool_t *pool) {
    const convolution_error_t validation_error = convolution_validate(destination, source, kernel);
    if (unlikely(validation_error != CONVOLUTION_OK)) return validation_error;
    if (unlikely(thread_pool_thread_count(pool) == 0)) return CONVOLUTION_THREAD_POOL_ERROR;

    const size_t task_count = convolution_task_count(source, kernel, pool);
    if (task_count == 1) {
        convolution_apply_rows(destination, source, kernel, 0, source->rows);
        return CONVOLUTION_OK;
    }

    convolution_task_context_t *contexts = convolution_create_contexts(task_count);
    if (unlikely(contexts == NULL)) return CONVOLUTION_ALLOCATION_FAILED;
    thread_pool_task_t *tasks = malloc(task_count * sizeof(*tasks));
    if (unlikely(tasks == NULL)) {
        free(contexts);
        return CONVOLUTION_ALLOCATION_FAILED;
    }

    const convolution_error_t error = convolution_dispatch(destination, source, kernel, pool,
        contexts, tasks, task_count);
    free(tasks);
    free(contexts);
    return error;
}

convolution_error_t convolution_apply_repeated_parallel(matrix_t *destination, const matrix_t *source,
    const matrix_t *kernel, size_t repeat_count, thread_pool_t *pool) {
    const convolution_error_t common_error = convolution_validate_common(destination, source, kernel);
    if (unlikely(common_error != CONVOLUTION_OK)) return common_error;
    if (unlikely(thread_pool_thread_count(pool) == 0)) return CONVOLUTION_THREAD_POOL_ERROR;
    if (unlikely(destination->data == kernel->data)) return CONVOLUTION_IN_PLACE;

    if (repeat_count == 0) {
        if (destination->data == source->data) return CONVOLUTION_OK;
        return matrix_copy(destination, source) == MATRIX_OK
            ? CONVOLUTION_OK
            : CONVOLUTION_INVALID_DESTINATION;
    }

    if (unlikely(destination->data == source->data)) return CONVOLUTION_IN_PLACE;

    raii_stack(matrix) temporary = create_stack(matrix);
    if (repeat_count > 1) {
        const matrix_error_t init_error = init(matrix, temporary, source->rows, source->cols);
        if (unlikely(init_error != MATRIX_OK)) {
            return CONVOLUTION_ALLOCATION_FAILED;
        }
    }

    const size_t task_count = convolution_task_count(source, kernel, pool);
    convolution_task_context_t *contexts = NULL;
    thread_pool_task_t *tasks = NULL;
    if (task_count > 1) {
        contexts = convolution_create_contexts(task_count);
        if (unlikely(contexts == NULL)) return CONVOLUTION_ALLOCATION_FAILED;

        tasks = malloc(task_count * sizeof(*tasks));
        if (unlikely(tasks == NULL)) {
            free(contexts);
            return CONVOLUTION_ALLOCATION_FAILED;
        }
    }

    const matrix_t *current_source = source;
    matrix_t *current_destination = repeat_count % 2 == 0 ? &temporary : destination;
    convolution_error_t error = CONVOLUTION_OK;

    for (size_t iteration = 0; iteration < repeat_count; ++iteration) {
        error = convolution_dispatch(current_destination, current_source, kernel, pool,
            contexts, tasks, task_count);
        if (unlikely(error != CONVOLUTION_OK)) break;

        current_source = current_destination;
        current_destination = current_destination == destination ? &temporary : destination;
    }

    free(tasks);
    free(contexts);
    return error;
}

const char *convolution_error_string(convolution_error_t error) {
    switch (error) {
        case CONVOLUTION_OK:
            return "success";
        case CONVOLUTION_INVALID_SOURCE:
            return "invalid source matrix";
        case CONVOLUTION_INVALID_DESTINATION:
            return "invalid destination matrix";
        case CONVOLUTION_INVALID_KERNEL:
            return "kernel must be a valid odd square matrix";
        case CONVOLUTION_SIZE_MISMATCH:
            return "source and destination sizes do not match";
        case CONVOLUTION_IN_PLACE:
            return "in-place convolution is not supported";
        case CONVOLUTION_ALLOCATION_FAILED:
            return "convolution allocation failed";
        case CONVOLUTION_THREAD_POOL_ERROR:
            return "thread pool failed while executing convolution";
    }
    return "unknown convolution error";
}
