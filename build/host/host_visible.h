#ifndef HOST_VISIBLE_H
#define HOST_VISIBLE_H

#include <cstddef>

constexpr int K = 8;

constexpr int ROWS = 16;
constexpr int COLUMNS = 1024;

constexpr int TOTAL_PIXELS = ROWS * COLUMNS;

constexpr int KERNEL_ROWS = ROWS - 2;
constexpr int KERNEL_COLS = COLUMNS - 2;
constexpr int KERNEL_ITERATIONS = KERNEL_ROWS * KERNEL_COLS;

constexpr int BURSTS_IN = TOTAL_PIXELS / 16;

// Must match the kernel:
// const int TOTAL_PACKETS_OUT = (KERNEL_ITERATIONS / K) + 16;
constexpr int TOTAL_PACKETS_OUT = (KERNEL_ITERATIONS / K) + 16;

constexpr std::size_t INPUT_BYTES =
    static_cast<std::size_t>(TOTAL_PIXELS) * sizeof(float);

constexpr std::size_t OUTPUT_FLOATS =
    static_cast<std::size_t>(TOTAL_PACKETS_OUT) * K;

constexpr std::size_t OUTPUT_BYTES =
    OUTPUT_FLOATS * sizeof(float);

#endif