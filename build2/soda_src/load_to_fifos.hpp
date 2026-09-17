// load_to_fifos.hpp  (K=64, 8 read channels, paired interleaved)
#ifndef LOAD_TO_FIFOS_HPP
#define LOAD_TO_FIFOS_HPP

#include "soda_common.h"

// even channels: BURSTS_PER_CH_EVEN, odd: _ODD.
void read_channel_even(hls::burst_maxi<float16>& in_mem, hls::stream<float16>& s) {
    #pragma HLS INLINE off
    in_mem.read_request(0, BURSTS_PER_CH_EVEN);
    for (int i = 0; i < BURSTS_PER_CH_EVEN; i++) {
        #pragma HLS PIPELINE II=1
        s.write(in_mem.read());
    }
}
void read_channel_odd(hls::burst_maxi<float16>& in_mem, hls::stream<float16>& s) {
    #pragma HLS INLINE off
    in_mem.read_request(0, BURSTS_PER_CH_ODD);
    for (int i = 0; i < BURSTS_PER_CH_ODD; i++) {
        #pragma HLS PIPELINE II=1
        s.write(in_mem.read());
    }
}

// unpack_and_feed: 8 streams -> 64 lanes (4 pairs εναλλαξ)
void unpack_and_feed(
    hls::stream<float16>& s0,
    hls::stream<float16>& s1,
    hls::stream<float16>& s2,
    hls::stream<float16>& s3,
    hls::stream<float16>& s4,
    hls::stream<float16>& s5,
    hls::stream<float16>& s6,
    hls::stream<float16>& s7,
    hls::stream<data_t> out[SODA_K]) {
    #pragma HLS INLINE off
    for (int i = 0; i < TOTAL_ITERATIONS; i++) {
        #pragma HLS PIPELINE II=1
        if (i < TOTAL_VECTORS) {
            if ((i & 1) == 0) {
                // αρτιο vector: καθε pair διαβαζει απο το EVEN channel του
                {
                    float16 b = s0.read();  // pair 0, even vector
                    for (int k = 0; k < 16; k++) {
                        #pragma HLS UNROLL
                        out[0 + k].write(b[k]);
                    }
                }
                {
                    float16 b = s2.read();  // pair 2, even vector
                    for (int k = 0; k < 16; k++) {
                        #pragma HLS UNROLL
                        out[16 + k].write(b[k]);
                    }
                }
                {
                    float16 b = s4.read();  // pair 4, even vector
                    for (int k = 0; k < 16; k++) {
                        #pragma HLS UNROLL
                        out[32 + k].write(b[k]);
                    }
                }
                {
                    float16 b = s6.read();  // pair 6, even vector
                    for (int k = 0; k < 16; k++) {
                        #pragma HLS UNROLL
                        out[48 + k].write(b[k]);
                    }
                }
            } else {
                // περιττο vector: καθε pair διαβαζει απο το ODD channel του
                {
                    float16 b = s1.read();  // pair 1, odd vector
                    for (int k = 0; k < 16; k++) {
                        #pragma HLS UNROLL
                        out[0 + k].write(b[k]);
                    }
                }
                {
                    float16 b = s3.read();  // pair 3, odd vector
                    for (int k = 0; k < 16; k++) {
                        #pragma HLS UNROLL
                        out[16 + k].write(b[k]);
                    }
                }
                {
                    float16 b = s5.read();  // pair 5, odd vector
                    for (int k = 0; k < 16; k++) {
                        #pragma HLS UNROLL
                        out[32 + k].write(b[k]);
                    }
                }
                {
                    float16 b = s7.read();  // pair 7, odd vector
                    for (int k = 0; k < 16; k++) {
                        #pragma HLS UNROLL
                        out[48 + k].write(b[k]);
                    }
                }
            }
        } else {
            for (int k = 0; k < SODA_K; k++) {
                #pragma HLS UNROLL
                out[k].write(0.0f);
            }
        }
    }
}

#endif // LOAD_TO_FIFOS_HPP