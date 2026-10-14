BEGIN {
    rows = 512
    cols = 512
    window_size = 9
    if (repeat_count == "") repeat_count = 10

    print rows, cols
    print window_size, repeat_count

    for (row = 0; row < rows; ++row) {
        for (col = 0; col < cols; ++col) {
            value = (row * 37 + col * 17 + row * col * 3) % 256 - 128
            printf "%d%s", value, col + 1 == cols ? "\n" : " "
        }
    }

    kernel_value = 1.0 / (window_size * window_size)
    for (row = 0; row < window_size; ++row) {
        for (col = 0; col < window_size; ++col) {
            printf "%.17g%s", kernel_value, col + 1 == window_size ? "\n" : " "
        }
    }
}
