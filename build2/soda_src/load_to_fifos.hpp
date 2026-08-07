// load_to_fifos.hpp
#ifndef LOAD_TO_FIFOS_HPP
#define LOAD_TO_FIFOS_HPP

#include "soda_common.h"

//reads serially from 2 hbms with float16 output (0 even 1 odd)
void load_input(hls::burst_maxi<float16>& in_mem_0,
                hls::burst_maxi<float16>& in_mem_1,
                hls::stream<float16>& out_stream) {
    #pragma HLS INLINE off

    const int total_bursts = TOTAL_VECTORS * BURSTS_PER_VEC;
    const int bursts_0 = (total_bursts + 1) / 2;
    const int bursts_1 = total_bursts / 2;

    in_mem_0.read_request(0, bursts_0);
    in_mem_1.read_request(0, bursts_1);

    // Εναλλαξ read
    for (int i = 0; i < total_bursts; i++) {
        #pragma HLS PIPELINE II=1
        float16 b = ((i & 1) == 0) ? in_mem_0.read() : in_mem_1.read();
        out_stream.write(b);
    }
}


void unpack_and_feed(hls::stream<float16>& in_stream, hls::stream<data_t> out[SODA_K]) {
    #pragma HLS INLINE off

    if (SODA_K <= 16) {
        // K=16
        float16 chunk;
        #pragma HLS ARRAY_PARTITION variable=chunk complete

        for (int i = 0; i < TOTAL_ITERATIONS; i++) {
            #pragma HLS PIPELINE II=1
            if (i < TOTAL_VECTORS) {
                int word_index = i % (16 / SODA_K);   // K=16 -> i%1 = 0
                if (word_index == 0) {
                    chunk = in_stream.read();
                }
                for (int k_idx = 0; k_idx < SODA_K; k_idx++) {
                    #pragma HLS UNROLL
                    out[k_idx].write(chunk[word_index * SODA_K + k_idx]);
                }
            } else {
                for (int k_idx = 0; k_idx < SODA_K; k_idx++) {
                    #pragma HLS UNROLL
                    out[k_idx].write(0.0f);
                }
            }
        }
    } else {
        //  K=32
        for (int i = 0; i < TOTAL_ITERATIONS; i++) {
            #pragma HLS PIPELINE II=1
            if (i < TOTAL_VECTORS) {
                float16 b0 = in_stream.read();   // lanes 0..15
                float16 b1 = in_stream.read();   // lanes 16..31
                for (int k = 0; k < 16; k++) {
                    #pragma HLS UNROLL
                    out[k].write(b0[k]);
                    out[16 + k].write(b1[k]);
                }
            } else {
                for (int k = 0; k < SODA_K; k++) {
                    #pragma HLS UNROLL
                    out[k].write(0.0f);
                }
            }
        }
    }
}

#endif // LOAD_TO_FIFOS_HPP