// load_to_fifos.hpp  (float16 stream + word_index - παλιο working για K=16, + K=32)
#ifndef LOAD_TO_FIFOS_HPP
#define LOAD_TO_FIFOS_HPP

#include "soda_common.h"

// -------------------------------------------------------------------------
// load_input: διαβαζει εναλλαξ απο 2 HBM channels -> ΕΝΑ float16 stream.
// (float16 = raw 512-bit burst, το φυσικο width που δουλευε στο hardware)
//
//   Και για K=16 και K=32 το output ειναι float16 bursts στη σωστη σειρα:
//   burst 0,1,2,3,... (channel 0 τα αρτια, channel 1 τα περιττα).
// -------------------------------------------------------------------------
void load_input(hls::burst_maxi<float16>& in_mem_0,
                hls::burst_maxi<float16>& in_mem_1,
                hls::stream<float16>& out_stream) {
    #pragma HLS INLINE off

    const int total_bursts = TOTAL_VECTORS * BURSTS_PER_VEC;
    const int bursts_0 = (total_bursts + 1) / 2;
    const int bursts_1 = total_bursts / 2;

    in_mem_0.read_request(0, bursts_0);
    in_mem_1.read_request(0, bursts_1);

    // Εναλλαξ read: burst i -> channel (i%2). Output float16 (raw burst).
    for (int i = 0; i < total_bursts; i++) {
        #pragma HLS PIPELINE II=1
        float16 b = ((i & 1) == 0) ? in_mem_0.read() : in_mem_1.read();
        out_stream.write(b);
    }
}

// -------------------------------------------------------------------------
// unpack_and_feed: παιρνει float16 bursts και ταιζει τα K lanes.
//
//   K=16 (16/K = 1): word_index = i%1 = 0. Νεο burst καθε κυκλο, 16 lanes.
//                    (ΑΚΡΙΒΩΣ το παλιο working μονοπατι)
//   K=32 (16/K = 0): word_index θα εσπαγε (i%0). Ξεχωριστο branch:
//                    2 bursts/vector -> 32 lanes.
// -------------------------------------------------------------------------
void unpack_and_feed(hls::stream<float16>& in_stream, hls::stream<data_t> out[SODA_K]) {
    #pragma HLS INLINE off

    if (SODA_K <= 16) {
        // ---- ΠΑΛΙΟ WORKING μονοπατι (K<=16) με word_index ----
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
        // ---- K=32: 2 float16 bursts ανα vector -> 32 lanes ----
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