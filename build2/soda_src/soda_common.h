// soda_common.h  (K=64, 8 read + 8 write channels)
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

#define VECTORS_PER_ROW  (SODA_COLS / SODA_K)
#define SODA_DELAY       (VECTORS_PER_ROW + 1)
#define TOTAL_ITERATIONS (TOTAL_VECTORS + SODA_DELAY + 16)

void architecture_top_level(
    hls::burst_maxi<float16> A_in_mem_0,
    hls::burst_maxi<float16> A_in_mem_1,
    hls::burst_maxi<float16> A_in_mem_2,
    hls::burst_maxi<float16> A_in_mem_3,
    hls::burst_maxi<float16> A_in_mem_4,
    hls::burst_maxi<float16> A_in_mem_5,
    hls::burst_maxi<float16> A_in_mem_6,
    hls::burst_maxi<float16> A_in_mem_7,
    hls::burst_maxi<float16> B_out_mem_0,
    hls::burst_maxi<float16> B_out_mem_1,
    hls::burst_maxi<float16> B_out_mem_2,
    hls::burst_maxi<float16> B_out_mem_3,
    hls::burst_maxi<float16> B_out_mem_4,
    hls::burst_maxi<float16> B_out_mem_5,
    hls::burst_maxi<float16> B_out_mem_6,
    hls::burst_maxi<float16> B_out_mem_7);

#endif // SODA_COMMON_H