// soda_kernel.hpp  —  COALESCED reuse buffer (SASA-style) for SODA denoise2d, K=16.
//
// Instead of 16 per-lane line buffers + pack/unpack,
// one 512-bit-wide sliding window (5 word-registers + 2 wide reuse FIFOs of depth VECTORS_PER_ROW).
//
// Data stays 512-bit (float16) end-to-end -> no unpack_and_feed, no filter_and_pack.
#ifndef SODA_KERNEL_HPP
#define SODA_KERNEL_HPP

#include "soda_common.h"

// compute + emit one 512-bit output word
static inline void emit_word(int row, int col_vec,
        const float16& w_up, const float16& w_prev, const float16& w_cur,
        const float16& w_next, const float16& w_down,
        hls::stream<float_pack>& out) {
    #pragma HLS INLINE
    float_pack ob;
    #pragma HLS ARRAY_PARTITION variable=ob complete
    for (int i = 0; i < 16; i++) {
        #pragma HLS UNROLL
        float center = w_cur[i];
        float up     = w_up[i];
        float down   = w_down[i];
        float left   = (i == 0)  ? w_prev[15] : w_cur[i - 1];
        float right  = (i == 15) ? w_next[0]  : w_cur[i + 1];
        float r0 = center - left;
        float r1 = center - right;
        float r2 = center - up;
        float r3 = center - down;
        float val = (r0 * r0) + (r1 * r1) + (r2 * r2) + (r3 * r3);
        int gcol = col_vec * 16 + i;
        bool border = (row == 0) || (row == SODA_ROWS - 1) ||
                      (gcol == 0) || (gcol == SODA_COLS - 1);
        ob[i] = border ? 0.0f : val;
    }
    out.write(ob);
}

// one wide PE: reads 512-bit words, keeps a coalesced reuse window, writes 512-bit words
void soda_compute_coalesced(hls::stream<float16>& in, hls::stream<float_pack>& out) {
    const int N = TOTAL_VECTORS;      // total 512-bit words
    const int R = VECTORS_PER_ROW;    // words per row

    float16 zero;
    #pragma HLS ARRAY_PARTITION variable=zero complete
    for (int i = 0; i < 16; i++) {
        #pragma HLS UNROLL
        zero[i] = 0.0f;
    }

    // sliding window of 5 words:  -R (up), -1 (prev), 0 (cur), +1 (next), +R (down)
    float16 w_up = zero, w_prev = zero, w_cur = zero, w_next = zero, w_down = zero;
    #pragma HLS ARRAY_PARTITION variable=w_up   complete
    #pragma HLS ARRAY_PARTITION variable=w_prev complete
    #pragma HLS ARRAY_PARTITION variable=w_cur  complete
    #pragma HLS ARRAY_PARTITION variable=w_next complete
    #pragma HLS ARRAY_PARTITION variable=w_down complete

    // the two WIDE reuse FIFOs (replace the 32 narrow per-lane line buffers)
    hls::stream<float16, VECTORS_PER_ROW> fifo_up;    // holds offsets -(R-1)..-2
    hls::stream<float16, VECTORS_PER_ROW> fifo_down;  // holds offsets +2..+(R-1)

    // FIll window
    //  above row 0 -> stay zero (top border)
    for (int i = 0; i < R - 2; i++) {   // fill fifo_up with R-2 zeros
        #pragma HLS PIPELINE II=1
        fifo_up.write(zero);
    }
    w_cur  = in.read();                           // input[0]
    w_next = in.read();                           // input[1]
    for (int i = 0; i < R - 2; i++) { // input[2..R-1] -> fill fifo_down
        #pragma HLS PIPELINE II=1
        fifo_down.write(in.read());
    }
    w_down = in.read();                           // input[R]

    const int V_SPLIT = N - (R + 1);   // Check in inputs ended

    // Moving through stencil
    for (int v = 0; v < V_SPLIT; v++) {
        #pragma HLS PIPELINE II=1
        emit_word(v / R, v % R, w_up, w_prev, w_cur, w_next, w_down, out);
        w_up   = fifo_up.read();   fifo_up.write(w_prev);
        w_prev = w_cur;            w_cur = w_next;
        w_next = fifo_down.read(); fifo_down.write(w_down);
        w_down = in.read();                       // UNCONDITIONAL
    }
    // Pad zeros in last row
    for (int v = V_SPLIT; v < N; v++) {
        #pragma HLS PIPELINE II=1
        emit_word(v / R, v % R, w_up, w_prev, w_cur, w_next, w_down, out);
        w_up   = fifo_up.read();   fifo_up.write(w_prev);
        w_prev = w_cur;            w_cur = w_next;
        w_next = fifo_down.read(); fifo_down.write(w_down);
        w_down = zero;                            // no read
    }

    // Flush in the end
    DRAIN: for (int i = 0; i < R - 2; i++) {
        #pragma HLS PIPELINE II=1
        fifo_up.read();
        fifo_down.read();
    }
}

#endif // SODA_KERNEL_HPP