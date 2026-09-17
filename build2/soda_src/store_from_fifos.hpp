// store_from_fifos.hpp  (K=64, 8 write channels, paired interleaved)
#ifndef STORE_FROM_FIFOS_HPP
#define STORE_FROM_FIFOS_HPP

#include "soda_common.h"

// collect_and_split: K lanes -> 8 write streams (paired interleaved)
void collect_and_split(hls::stream<data_t> in[SODA_K],
    hls::stream<float16>& ws0,
    hls::stream<float16>& ws1,
    hls::stream<float16>& ws2,
    hls::stream<float16>& ws3,
    hls::stream<float16>& ws4,
    hls::stream<float16>& ws5,
    hls::stream<float16>& ws6,
    hls::stream<float16>& ws7) {
    #pragma HLS INLINE off
    for (int i = 0; i < TOTAL_ITERATIONS; i++) {
        #pragma HLS PIPELINE II=1
        data_t v[SODA_K];
        #pragma HLS ARRAY_PARTITION variable=v complete
        for (int k = 0; k < SODA_K; k++) {
            #pragma HLS UNROLL
            v[k] = in[k].read();
        }
        int true_cycle = i - SODA_DELAY;
        if (true_cycle >= 0 && true_cycle < TOTAL_VECTORS) {
            if ((true_cycle & 1) == 0) {
                // αρτιο vector -> even channels
                {
                    float16 chunk;
                    for (int j = 0; j < 16; j++) {
                        #pragma HLS UNROLL
                        chunk[j] = v[0 + j];
                    }
                    ws0.write(chunk);  // pair 0 even
                }
                {
                    float16 chunk;
                    for (int j = 0; j < 16; j++) {
                        #pragma HLS UNROLL
                        chunk[j] = v[16 + j];
                    }
                    ws2.write(chunk);  // pair 2 even
                }
                {
                    float16 chunk;
                    for (int j = 0; j < 16; j++) {
                        #pragma HLS UNROLL
                        chunk[j] = v[32 + j];
                    }
                    ws4.write(chunk);  // pair 4 even
                }
                {
                    float16 chunk;
                    for (int j = 0; j < 16; j++) {
                        #pragma HLS UNROLL
                        chunk[j] = v[48 + j];
                    }
                    ws6.write(chunk);  // pair 6 even
                }
            } else {
                // περιττο vector -> odd channels
                {
                    float16 chunk;
                    for (int j = 0; j < 16; j++) {
                        #pragma HLS UNROLL
                        chunk[j] = v[0 + j];
                    }
                    ws1.write(chunk);  // pair 1 odd
                }
                {
                    float16 chunk;
                    for (int j = 0; j < 16; j++) {
                        #pragma HLS UNROLL
                        chunk[j] = v[16 + j];
                    }
                    ws3.write(chunk);  // pair 3 odd
                }
                {
                    float16 chunk;
                    for (int j = 0; j < 16; j++) {
                        #pragma HLS UNROLL
                        chunk[j] = v[32 + j];
                    }
                    ws5.write(chunk);  // pair 5 odd
                }
                {
                    float16 chunk;
                    for (int j = 0; j < 16; j++) {
                        #pragma HLS UNROLL
                        chunk[j] = v[48 + j];
                    }
                    ws7.write(chunk);  // pair 7 odd
                }
            }
        }
    }
}

// even: BURSTS_PER_CH_EVEN, odd: _ODD
void write_channel_even(hls::stream<float16>& s, hls::burst_maxi<float16>& out_mem) {
    #pragma HLS INLINE off
    out_mem.write_request(0, BURSTS_PER_CH_EVEN);
    for (int i = 0; i < BURSTS_PER_CH_EVEN; i++) {
        #pragma HLS PIPELINE II=1
        out_mem.write(s.read());
    }
    out_mem.write_response();
}
void write_channel_odd(hls::stream<float16>& s, hls::burst_maxi<float16>& out_mem) {
    #pragma HLS INLINE off
    out_mem.write_request(0, BURSTS_PER_CH_ODD);
    for (int i = 0; i < BURSTS_PER_CH_ODD; i++) {
        #pragma HLS PIPELINE II=1
        out_mem.write(s.read());
    }
    out_mem.write_response();
}

#endif // STORE_FROM_FIFOS_HPP