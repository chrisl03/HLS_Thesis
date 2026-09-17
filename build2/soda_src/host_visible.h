// host_visible.h  (K=64, 8 read + 8 write channels, paired interleaved)
#ifndef HOST_VISIBLE_H
#define HOST_VISIBLE_H

#define MIN(a, b)  ((a) < (b) ? (a) : (b))
#define MAX(a, b)  ((a) > (b) ? (a) : (b))
#define CEIL_DIV(X, Y) (((X) + (Y) - 1) / (Y))

#define SODA_K 64
#define SODA_ROWS 1024
#define SODA_COLS 1024

#define NUM_READ_CH  8
#define NUM_WRITE_CH 8
#define NUM_PAIRS    4        // 8 channels / 2 = 4 pairs

#define SODA_TOTAL_PIXELS (SODA_ROWS * SODA_COLS)
#define SODA_KERNEL_ROWS (SODA_ROWS - 2)
#define SODA_KERNEL_COLS (SODA_COLS - 2)
#define SODA_KERNEL_ITER (SODA_KERNEL_ROWS * SODA_KERNEL_COLS)

// K-wide 
#define TOTAL_VECTORS (SODA_TOTAL_PIXELS / SODA_K)

// each channel keeps half bursts of the total of its pair
#define BURSTS_PER_CH_EVEN ((TOTAL_VECTORS + 1) / 2)
#define BURSTS_PER_CH_ODD  (TOTAL_VECTORS / 2)

#endif // HOST_VISIBLE_H