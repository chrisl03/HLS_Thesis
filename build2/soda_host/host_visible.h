// host_visible.h  (2-SLR split: 2x K=64 kernels, 8R+8W paired, per kernel)
//   Full  1024x1024 split in rows in two parts + 1 halo row each.
//   Each kernel has SODA_ROWS = 513 rows (512 + 1 halo), COLS = 1024.
#ifndef HOST_VISIBLE_H
#define HOST_VISIBLE_H

#define MIN(a, b)  ((a) < (b) ? (a) : (b))
#define MAX(a, b)  ((a) > (b) ? (a) : (b))
#define CEIL_DIV(X, Y) (((X) + (Y) - 1) / (Y))

#define SODA_K     64
#define SODA_COLS  1024

#define SODA_ROWS_FULL 1024
#define SPLIT_ROW      (SODA_ROWS_FULL / 2)   // 512 : split line
#define HALO           1                      // 5-point cross needs 1 extra row/side

//   Kernel A  rows 0-512
//   Kernel B  rows 511-1023
//   local row 0 and SODA_ROWS-1 trash (0 is actuall border, the other is the halo)
//   
#define SODA_ROWS   (SPLIT_ROW + HALO)         // 513  <-- the kernel's rows
#define A_ROW_START 0                
#define B_ROW_START (SPLIT_ROW - HALO)     

// valid local output rows (same for both kernels)
#define VALID_LOCAL_FIRST 1
#define VALID_LOCAL_LAST  (SODA_ROWS - 2)      // 511

#define NUM_READ_CH  8
#define NUM_WRITE_CH 8
#define NUM_PAIRS    4        // 8 channels / 2

// per-kernel vector/burst counts
#define SODA_TOTAL_PIXELS (SODA_ROWS * SODA_COLS)   // 513*1024 = 525312
#define TOTAL_VECTORS     (SODA_TOTAL_PIXELS / SODA_K)   // 8208

// each channel of a pair carries half the vectors (even/odd interleave)
#define BURSTS_PER_CH_EVEN ((TOTAL_VECTORS + 1) / 2)     // 4104
#define BURSTS_PER_CH_ODD  (TOTAL_VECTORS / 2)           // 4104


#endif // HOST_VISIBLE_H