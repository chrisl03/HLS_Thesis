// soda_common.h  (shared by both kernels
#ifndef SODA_COMMON_H
#define SODA_COMMON_H

#include <hls_stream.h>
#include <ap_int.h>
#include <hls_vector.h>
#include "hls_burst_maxi.h"
#include "hls_math.h"
#include "host_visible.h"

typedef float data_t;
typedef hls::vector<data_t, 16> float16;

#define VECTORS_PER_ROW  (SODA_COLS / SODA_K)         // 16
#define SODA_DELAY       (VECTORS_PER_ROW + 1)        // 17
#define TOTAL_ITERATIONS (TOTAL_VECTORS + SODA_DELAY + 16)  // 8208+17+16 = 8241

#endif // SODA_COMMON_H