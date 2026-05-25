//  store_from_fifos.hpp
#ifndef STORE_FROM_FIFOS_HPP
#define STORE_FROM_FIFOS_HPP

#include "soda_common.h"

void filter_and_pack(hls::stream<data_t> in[SODA_K], hls::stream<float_pack>& out_stream) {
    #pragma HLS INLINE off
    data_t buffer[SODA_K];
    #pragma HLS ARRAY_PARTITION variable=buffer complete
    for(int i = 0; i < SODA_K; i++) buffer[i] = 0.0f;

    int buf_count = 0;
    bool done = false;
    int dummy_count = 0;

    for (int i = 0; i < TOTAL_ITERATIONS; i++) {
        #pragma HLS PIPELINE II=1

        data_t curr_val[SODA_K];
        #pragma HLS ARRAY_PARTITION variable=curr_val complete
        for (int k_idx = 0; k_idx < SODA_K; k_idx++) {
            #pragma HLS UNROLL
            curr_val[k_idx] = in[k_idx].read();
        }

        int true_cycle = i - SODA_DELAY;

        if (true_cycle >= 0 && true_cycle < TOTAL_VECTORS) {
            int row = true_cycle / VECTORS_PER_ROW;
            int col_cycle = true_cycle % VECTORS_PER_ROW;

            if (row >= 1 && row < SODA_ROWS - 1) {
                int start_idx = (col_cycle == 0) ? 1 : 0;
                int valid_count = (col_cycle == 0 || col_cycle == VECTORS_PER_ROW - 1) ? SODA_K - 1 : SODA_K;

                data_t valid_pixels[SODA_K];
                #pragma HLS ARRAY_PARTITION variable=valid_pixels complete
                for(int j = 0; j < SODA_K; j++) {
                    #pragma HLS UNROLL
                    valid_pixels[j] = (j < valid_count) ? curr_val[start_idx + j] : 0.0f;
                }

                data_t merged[2 * SODA_K];
                #pragma HLS ARRAY_PARTITION variable=merged complete
                
                for (int j = 0; j < SODA_K; j++) {
                    #pragma HLS UNROLL
                    merged[j] = buffer[j];
                    merged[SODA_K + j] = 0.0f; 
                }
                for (int j = 0; j < SODA_K; j++) {
                    #pragma HLS UNROLL
                    merged[buf_count + j] = valid_pixels[j];
                }

                int new_total = buf_count + valid_count;

                if (new_total >= SODA_K) {
                    float_pack pack;
                    for (int j = 0; j < SODA_K; j++) pack[j] = merged[j];
                    
                    out_stream.write(pack);

                    buf_count = new_total - SODA_K;
                    for (int j = 0; j < SODA_K; j++) buffer[j] = merged[SODA_K + j];
                } else {
                    buf_count = new_total;
                    for (int j = 0; j < SODA_K; j++) buffer[j] = merged[j];
                }

                if (row == SODA_ROWS - 2 && col_cycle == VECTORS_PER_ROW - 1) done = true;
            }
        }
        else {
            if (done && dummy_count < 16) {
                float_pack dummy_pack;

                for (int j = 0; j < SODA_K; j++) {
                    #pragma HLS UNROLL
                    dummy_pack[j] = (dummy_count == 0 && j < buf_count) ? buffer[j] : 0.0f;
                }

                if (dummy_count == 0) buf_count = 0; 

                out_stream.write(dummy_pack);
                dummy_count++;
            }
        }
    }
}

void store_output(hls::stream<float_pack>& in_stream, hls::burst_maxi<float_pack>& out_mem) {
    #pragma HLS INLINE off
    out_mem.write_request(0, SODA_TOTAL_PACKETS_OUT);
    
    for (int i = 0; i < SODA_TOTAL_PACKETS_OUT; i++) {
        #pragma HLS PIPELINE II=1
        out_mem.write(in_stream.read());
    }

    out_mem.write_response();
}

#endif // STORE_FROM_FIFOS_HPP