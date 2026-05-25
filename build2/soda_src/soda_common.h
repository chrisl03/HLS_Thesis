// soda_common.h
#ifndef SODA_COMMON_H
#define SODA_COMMON_H

#include <hls_stream.h>
#include <ap_int.h>
#include <hls_vector.h>
#include "hls_burst_maxi.h"
#include "hls_math.h"

// params header
#include "host_visible.h"

typedef float data_t;

// input pack (16 floats = 512 bit)
typedef hls::vector<data_t, 16> float16;

// output pack (Generic size K)
typedef hls::vector<data_t, SODA_K> float_pack;

// total vector chunks
#define VECTORS_PER_ROW  (SODA_COLS / SODA_K)
#define TOTAL_VECTORS    (SODA_TOTAL_PIXELS / SODA_K)

#define SODA_DELAY       (VECTORS_PER_ROW + 1)

// +64 to flush the last packets
#define TOTAL_ITERATIONS (TOTAL_VECTORS + SODA_DELAY + 16)


// Top-Level Module 
void architecture_top_level(hls::burst_maxi<float16> A_in_mem, hls::burst_maxi<float_pack> B_out_mem);

#endif // SODA_COMMON_H