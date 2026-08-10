// host_visible.h
#ifndef HOST_VISIBLE_H
#define HOST_VISIBLE_H

// Helper macros
#define MIN(a, b)  ((a) < (b) ? (a) : (b))
#define MAX(a, b)  ((a) > (b) ? (a) : (b))
#define CEIL_DIV(X, Y) (((X) + (Y) - 1) / (Y))

// basic params
#define SODA_K 8
#define SODA_ROWS 1024
#define SODA_COLS 1024

// derived parameters 
#define SODA_TOTAL_PIXELS (SODA_ROWS * SODA_COLS)
#define SODA_KERNEL_ROWS (SODA_ROWS - 2)
#define SODA_KERNEL_COLS (SODA_COLS - 2)
#define SODA_KERNEL_ITER (SODA_KERNEL_ROWS * SODA_KERNEL_COLS)

// amt of bursts of 16 floats - buffer sizes
#define SODA_BURSTS_IN CEIL_DIV(SODA_TOTAL_PIXELS, 16)
#define SODA_BURSTS_IN_0   ((SODA_BURSTS_IN + 1) / 2)
#define SODA_BURSTS_IN_1   (SODA_BURSTS_IN / 2)
#define SODA_PACKETS_OUT_0 ((SODA_TOTAL_PACKETS_OUT + 1) / 2)
#define SODA_PACKETS_OUT_1 (SODA_TOTAL_PACKETS_OUT / 2)

#define SODA_TOTAL_PACKETS_OUT ((SODA_KERNEL_ITER / SODA_K) + 16)
#define BURSTS_PER_VEC (SODA_K / 16)
#define TOTAL_VECTORS    (SODA_TOTAL_PIXELS / SODA_K)

#endif //HOST_VISIBLE_H