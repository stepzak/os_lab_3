#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "convolution.h"
#include "input.h"

typedef struct {
    size_t thread_count;
    const char *input_path;
    bool has_thread_count;
    bool suppress_output;
} program_options_t;

typedef enum {
    OPTIONS_OK = 0,
    OPTIONS_HELP,
    OPTIONS_ERROR
} options_result_t;

static void print_usage(FILE *stream, const char *program_name) {
    fprintf(stream,
        "Usage: %s --threads COUNT [--input FILE]\n"
        "       %s -t COUNT [-i FILE]\n"
        "\n"
        "Options:\n"
        "  -t, --threads COUNT  maximum number of worker threads\n"
        "  -i, --input FILE     read input from FILE instead of stdin\n"
        "  -q, --no-output      do not print the resulting matrix\n"
        "  -h, --help           show this help\n",
        program_name, program_name);
}

static bool parse_positive_size(const char *text, size_t *value) {
    if (text == NULL || value == NULL || text[0] == '\0' || text[0] == '-') {
        return false;
    }

    char *end = NULL;
    errno = 0;
    const uintmax_t parsed = strtoumax(text, &end, 10);
    if (end == text || *end != '\0' || errno == ERANGE
        || parsed == 0 || parsed > SIZE_MAX) {
        return false;
    }

    *value = (size_t) parsed;
    return true;
}

static options_result_t parse_options(
    int argc,
    char **argv,
    program_options_t *options
) {
    *options = (program_options_t) {0};

    for (int index = 1; index < argc; ++index) {
        const char *argument = argv[index];

        if (strcmp(argument, "-h") == 0 || strcmp(argument, "--help") == 0) {
            return OPTIONS_HELP;
        }

        if (strcmp(argument, "-t") == 0 || strcmp(argument, "--threads") == 0) {
            if (options->has_thread_count) {
                fprintf(stderr, "Thread count was specified more than once\n");
                return OPTIONS_ERROR;
            }
            if (++index == argc) {
                fprintf(stderr, "Missing value after %s\n", argument);
                return OPTIONS_ERROR;
            }
            if (!parse_positive_size(argv[index], &options->thread_count)) {
                fprintf(stderr, "Invalid thread count: %s\n", argv[index]);
                return OPTIONS_ERROR;
            }
            options->has_thread_count = true;
            continue;
        }

        if (strcmp(argument, "-i") == 0 || strcmp(argument, "--input") == 0) {
            if (options->input_path != NULL) {
                fprintf(stderr, "Input file was specified more than once\n");
                return OPTIONS_ERROR;
            }
            if (++index == argc) {
                fprintf(stderr, "Missing value after %s\n", argument);
                return OPTIONS_ERROR;
            }
            options->input_path = argv[index];
            continue;
        }

        if (strcmp(argument, "-q") == 0 || strcmp(argument, "--no-output") == 0) {
            options->suppress_output = true;
            continue;
        }

        fprintf(stderr, "Unknown argument: %s\n", argument);
        return OPTIONS_ERROR;
    }

    if (!options->has_thread_count) {
        fprintf(stderr, "The --threads option is required\n");
        return OPTIONS_ERROR;
    }
    return OPTIONS_OK;
}

static bool matrix_print(FILE *stream, const matrix_t *matrix) {
    if (!matrix_is_valid(matrix)) return false;

    if (fprintf(stream, "%zu %zu\n", matrix->rows, matrix->cols) < 0) {
        return false;
    }

    for (size_t row = 0; row < matrix->rows; ++row) {
        const double *values = matrix_const_row(matrix, row);
        for (size_t col = 0; col < matrix->cols; ++col) {
            const char *separator = col + 1 == matrix->cols ? "\n" : " ";
            if (fprintf(stream, "%.17g%s", values[col], separator) < 0) {
                return false;
            }
        }
    }
    return true;
}

static const double NANO_TO_SEC = 1 / 1e9;

static double elapsed_seconds(struct timespec begin, struct timespec end) {
    const time_t seconds = end.tv_sec - begin.tv_sec;
    const long nanoseconds = end.tv_nsec - begin.tv_nsec;
    return (double) seconds + (double) nanoseconds * NANO_TO_SEC;
}

int main(int argc, char **argv) {
    program_options_t options;
    const options_result_t options_result = parse_options(argc, argv, &options);
    if (options_result == OPTIONS_HELP) {
        print_usage(stdout, argv[0]);
        return EXIT_SUCCESS;
    }
    if (options_result == OPTIONS_ERROR) {
        print_usage(stderr, argv[0]);
        return EXIT_FAILURE;
    }

    FILE *input_stream = stdin;
    if (options.input_path != NULL) {
        input_stream = fopen(options.input_path, "r");
        if (input_stream == NULL) {
            fprintf(stderr, "Cannot open input file '%s': %s\n",
                options.input_path, strerror(errno));
            return EXIT_FAILURE;
        }
    }

    raii_stack(matrix) source = create_stack(matrix);
    raii_stack(matrix) kernel = create_stack(matrix);
    size_t repeat_count;
    const input_error_t input_error = input_read(input_stream, &source, &kernel, &repeat_count);

    if (input_stream != stdin && fclose(input_stream) != 0) {
        fprintf(stderr, "Cannot close input file '%s': %s\n",
            options.input_path, strerror(errno));
        return EXIT_FAILURE;
    }
    if (input_error != INPUT_OK) {
        fprintf(stderr, "Input error: %s\n", input_error_string(input_error));
        return EXIT_FAILURE;
    }

    matrix_error_t matrix_error;
    raii_stack(matrix) result = new_stack(matrix, &matrix_error, source.rows, source.cols);
    if (matrix_error != MATRIX_OK) {
        fprintf(stderr, "Cannot allocate result matrix (error %d)\n", matrix_error);
        return EXIT_FAILURE;
    }

    thread_pool_error_t pool_error;
    raii_stack(thread_pool) pool = new_stack(
        thread_pool, &pool_error, options.thread_count, options.thread_count);
    if (pool_error != THREAD_POOL_OK) {
        fprintf(stderr, "Thread pool error: %s\n",
            thread_pool_error_string(pool_error));
        return EXIT_FAILURE;
    }

    struct timespec begin;
    struct timespec end;
    if (clock_gettime(CLOCK_MONOTONIC, &begin) != 0) {
        perror("clock_gettime");
        return EXIT_FAILURE;
    }

    const convolution_error_t convolution_error =
        convolution_apply_repeated_parallel(
            &result, &source, &kernel, repeat_count, &pool);

    if (clock_gettime(CLOCK_MONOTONIC, &end) != 0) {
        perror("clock_gettime");
        return EXIT_FAILURE;
    }
    if (convolution_error != CONVOLUTION_OK) {
        fprintf(stderr, "Convolution error: %s\n",
            convolution_error_string(convolution_error));
        return EXIT_FAILURE;
    }

    if (!options.suppress_output && !matrix_print(stdout, &result)) {
        fprintf(stderr, "Output error\n");
        return EXIT_FAILURE;
    }

    fprintf(stderr,
        "threads=%zu repetitions=%zu elapsed_seconds=%.9f\n",
        thread_pool_thread_count(&pool), repeat_count,
        elapsed_seconds(begin, end));
    return EXIT_SUCCESS;
}
