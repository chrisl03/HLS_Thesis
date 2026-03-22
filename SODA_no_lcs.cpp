#include <stdio.h>  
#include "ap_int.h"    
#include "ap_fixed.h" 
#include "hls_stream.h"
#include "hls_math.h"  

typedef float data_t;

const int ORIG_ROWS = 16;
const int ORIG_COLS = 1024;
const int ORIG_TOTAL = ORIG_ROWS * ORIG_COLS;


const int FIFO_0_DEPTH = 1023; //instead of 1023, due to the padding
const int FIFO_1_DEPTH = 1;
const int FIFO_2_DEPTH = 1;
const int FIFO_3_DEPTH = 1023;




template <int T_DEPTH, int T_TOTAL_ELEMENTS>
void forwarding_module(hls::stream<data_t>& in, 
                       hls::stream<data_t>& out_to_next_fw,
                       hls::stream<data_t>& out_to_pe) { 
    // Direct forwarding 
    if (T_DEPTH == 0) {
        for (int i = 0; i < T_TOTAL_ELEMENTS; i++) {
            #pragma HLS PIPELINE II=1
            data_t val = in.read();
            out_to_next_fw.write(val);
            out_to_pe.write(val);
        }
    } 
    // Fifo or ff with reuse buff
    else {
        // if depth is 0 make it 1
        data_t buffer[T_DEPTH == 0 ? 1 : T_DEPTH];
        #pragma HLS BIND_STORAGE variable=buffer type=ram_2p impl=bram
        
        int ptr = 0;
        
        for (int i = 0; i < T_TOTAL_ELEMENTS; i++) {
            #pragma HLS PIPELINE II=1
            data_t new_val = in.read();
            
            // Το μυστικό: Αν δεν έχουν περάσει T_DEPTH κύκλοι, βγάλε 0 (dummy data).
            // Αλλιώς, βγάλε το καθυστερημένο δεδομένο από τον κυκλικό buffer.
            data_t out_val = (i < T_DEPTH) ? (data_t)0.0f : buffer[ptr]; 
            
            out_to_next_fw.write(out_val);
            out_to_pe.write(out_val);
            
            buffer[ptr] = new_val;
            ptr = (ptr == T_DEPTH - 1) ? 0 : ptr + 1;
        }
    }
}


template <int T_DEPTH, int T_TOTAL_ELEMENTS>
void terminal_forwarding_module(hls::stream<data_t>& in, 
                                hls::stream<data_t>& out_to_pe) {
    data_t buffer[T_DEPTH];
    #pragma HLS BIND_STORAGE variable=buffer type=ram_2p impl=bram
    int ptr = 0;
    
    for (int i = 0; i < T_TOTAL_ELEMENTS; i++) {
        #pragma HLS PIPELINE II=1
        data_t new_val = in.read();
        
        data_t out_val = (i < T_DEPTH) ? (data_t)0.0f : buffer[ptr];
        
        out_to_pe.write(out_val);
        
        buffer[ptr] = new_val;
        ptr = (ptr == T_DEPTH - 1) ? 0 : ptr + 1;
    }
}


template <int T_ITERATIONS>
void compute_kernel(hls::stream<data_t>& in_down,   // A[i+1][j]  (direct pass-through FW)
                    hls::stream<data_t>& in_right,  // A[i][j+1]  ( FW0)
                    hls::stream<data_t>& in_center, // A[i][j]    ( FW1)
                    hls::stream<data_t>& in_left,   // A[i][j-1]  ( FW2)
                    hls::stream<data_t>& in_up,     // A[i-1][j]  (Terminal FW3)
                    hls::stream<data_t>& out_B) {

    for (int i = 0; i < T_ITERATIONS; i++) {
        #pragma HLS PIPELINE II=1

        data_t a10  = in_down.read();   // +1, 0
        data_t a01  = in_right.read();  // 0, +1
        data_t a00  = in_center.read(); // 0, 0
        data_t a0m1 = in_left.read();   // 0, -1
        data_t am10 = in_up.read();     // -1, 0

        data_t res_0 = a00 - a0m1;
        data_t res_1 = a00 - a01;
        data_t res_2 = a00 - am10;
        data_t res_3 = a00 - a10;

        data_t b_val = (res_0 * res_0) + (res_1 * res_1) +
                       (res_2 * res_2) + (res_3 * res_3);

        out_B.write(b_val);
    }
}


void architecture_top_level(hls::stream<data_t> &A_in, 
                            hls::stream<data_t> &B_out) {
    #pragma HLS DATAFLOW

    // Ενδιάμεσα streams για την αλυσίδα των FWs
    hls::stream<data_t> fw_direct_to_0, fw0_to_fw1, fw1_to_fw2, fw2_to_fw3;
    #pragma HLS STREAM variable=fw_direct_to_0 depth=4
    #pragma HLS STREAM variable=fw0_to_fw1 depth=4
    #pragma HLS STREAM variable=fw1_to_fw2 depth=4
    #pragma HLS STREAM variable=fw2_to_fw3 depth=4

    // Τα streams που μπαίνουν στον Compute Kernel
    hls::stream<data_t> pe_in_down, pe_in_right, pe_in_center, pe_in_left, pe_in_up;
    #pragma HLS STREAM variable=pe_in_down depth=4
    #pragma HLS STREAM variable=pe_in_right depth=4
    #pragma HLS STREAM variable=pe_in_center depth=4
    #pragma HLS STREAM variable=pe_in_left depth=4
    #pragma HLS STREAM variable=pe_in_up depth=4

    // Direct Forward (0) -> A[i+1][j] (Down) - first loaded
    forwarding_module<0, ORIG_TOTAL>(A_in, fw_direct_to_0, pe_in_down);

    // FW0 (1023) ->  A[i][j+1] (Right) -loaded 1023 cycles later
    forwarding_module<FIFO_0_DEPTH, ORIG_TOTAL>(fw_direct_to_0, fw0_to_fw1, pe_in_right);

    // FW1 (1) ->  A[i][j] (Center) - +1 cycle
    forwarding_module<FIFO_1_DEPTH, ORIG_TOTAL>(fw0_to_fw1, fw1_to_fw2, pe_in_center);

    // FW2 (1) -> A[i][j-1] (Left) - +1 cycle
    forwarding_module<FIFO_2_DEPTH, ORIG_TOTAL>(fw1_to_fw2, fw2_to_fw3, pe_in_left);

    // Terminal FW (1023) -> 1023 cycles later (prev row, next column)
    terminal_forwarding_module<FIFO_3_DEPTH, ORIG_TOTAL>(fw2_to_fw3, pe_in_up);

    // comp kernel (PE)
    compute_kernel<ORIG_TOTAL>(pe_in_down, pe_in_right, pe_in_center, pe_in_left, pe_in_up, B_out);
}