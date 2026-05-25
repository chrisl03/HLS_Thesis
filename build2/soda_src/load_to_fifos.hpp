// load_to_fifos.hpp
#ifndef LOAD_TO_FIFOS_HPP
#define LOAD_TO_FIFOS_HPP

#include "soda_common.h"

void load_input(hls::burst_maxi<float16>& in_mem, hls::stream<float16>& out_stream) {
    #pragma HLS INLINE off
    in_mem.read_request(0, SODA_BURSTS_IN);
    
    for (int i = 0; i < SODA_BURSTS_IN; i++) {
        #pragma HLS PIPELINE II=1
        out_stream.write(in_mem.read());
    }
}

void unpack_and_feed(hls::stream<float16>& in_stream, hls::stream<data_t> out[SODA_K]) {
    #pragma HLS INLINE off
    float16 chunk; 
    #pragma HLS ARRAY_PARTITION variable=chunk complete

    for (int i = 0; i < TOTAL_ITERATIONS; i++) {
        #pragma HLS PIPELINE II=1
        
        if (i < TOTAL_VECTORS) {
            int word_index = i % (16 / SODA_K);  
            if (word_index == 0) {
                chunk = in_stream.read(); 
            }
            
            //  unroll the loop and write simultaneously to all cables
            for (int k_idx = 0; k_idx < SODA_K; k_idx++) {
                #pragma HLS UNROLL
                out[k_idx].write(chunk[word_index * SODA_K + k_idx]);
            }
        } else {
            // Pipeline Flushing (zeros to empty PEs)
            for (int k_idx = 0; k_idx < SODA_K; k_idx++) {
                #pragma HLS UNROLL
                out[k_idx].write(0.0f);
            }
        }
    }
}

#endif // LOAD_TO_FIFOS_HPP