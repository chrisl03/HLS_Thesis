#include <stdio.h>  
#include "ap_int.h"    
#include "ap_fixed.h" 
#include "hls_stream.h"
#include "hls_math.h"  
#include "hls_vector.h"

typedef float data_t;
typedef hls::vector<data_t, 2> data_vec_t; // 2 floats in one input bcs k=2

const int ROWS = 16;
const int COLUMNS = 1024;

const int VECTORS_PER_ROW = COLUMNS / 2;     // 512
const int TOTAL_VECTORS = (ROWS * COLUMNS) / 2; // 8192

// Fifo depth = 1024/2   (W/k)
const int FIFO_ROW_DEPTH = 512;

// FW are same as in SODA_no_lcs.cpp only dataaa typesare diff, comments are in that file

template <int T_DEPTH, int T_TOTAL_VECTORS>
void forwarding_module_vec(hls::stream<data_vec_t>& in, 
                           hls::stream<data_vec_t>& out_to_next_fw,
                           hls::stream<data_vec_t>& out_to_pe) { 
    if (T_DEPTH == 0) {
        for (int i = 0; i < T_TOTAL_VECTORS; i++) {
            #pragma HLS PIPELINE II=1
            data_vec_t val = in.read();
            out_to_next_fw.write(val);
            out_to_pe.write(val);
        }
    } else {
        data_vec_t buffer[T_DEPTH == 0 ? 1 : T_DEPTH];
        #pragma HLS BIND_STORAGE variable=buffer type=ram_2p impl=bram
        int ptr = 0;
        
        data_vec_t zero_vec;
        zero_vec[0] = 0.0f; zero_vec[1] = 0.0f;

        for (int i = 0; i < T_TOTAL_VECTORS; i++) {
            #pragma HLS PIPELINE II=1
            data_vec_t new_val = in.read();
            
            data_vec_t out_val = (i < T_DEPTH) ? zero_vec : buffer[ptr]; 
            
            out_to_next_fw.write(out_val);
            out_to_pe.write(out_val);
            
            buffer[ptr] = new_val;
            ptr = (ptr == T_DEPTH - 1) ? 0 : ptr + 1;
        }
    }
}

template <int T_DEPTH, int T_TOTAL_VECTORS>
void terminal_forwarding_module_vec(hls::stream<data_vec_t>& in, 
                                    hls::stream<data_vec_t>& out_to_pe) {
    data_vec_t buffer[T_DEPTH == 0 ? 1 : T_DEPTH];
    #pragma HLS BIND_STORAGE variable=buffer type=ram_2p impl=bram
    int ptr = 0;
    
    data_vec_t zero_vec;
    zero_vec[0] = 0.0f; zero_vec[1] = 0.0f;
    
    for (int i = 0; i < T_TOTAL_VECTORS; i++) {
        #pragma HLS PIPELINE II=1
        data_vec_t new_val = in.read();
        
        data_vec_t out_val = (i < T_DEPTH) ? zero_vec : buffer[ptr];
        
        out_to_pe.write(out_val);
        
        buffer[ptr] = new_val;
        ptr = (ptr == T_DEPTH - 1) ? 0 : ptr + 1;
    }
}

template <int T_ITERATIONS>
void compute_kernel_k2(hls::stream<data_vec_t>& in_down, 
                       hls::stream<data_vec_t>& in_mid,  
                       hls::stream<data_vec_t>& in_up,   
                       hls::stream<data_vec_t>& out_B) {

    //Whole point is that we will be calculating each result with a delay of one cycle
    //because PE1 requires the element on the right whch hasnt entered yet

    // register for current data containing the middle column of the cross
    data_vec_t v_mid_reg, v_down_reg, v_up_reg;
    // reg for data on the left (P0 requires n-1 element), no need for up/down they aree useless
    data_vec_t v_mid_prev; 

    v_mid_reg[0] = 0; v_mid_reg[1] = 0;
    v_mid_prev[0] = 0; v_mid_prev[1] = 0;

    for (int i = 0; i < T_ITERATIONS; i++) {
        #pragma HLS PIPELINE II=1

        // reg that will be used for the data on the right (P1 requires n+2 that comes in the next cycle)
        data_vec_t v_down_new = in_down.read();
        data_vec_t v_mid_new  = in_mid.read();
        data_vec_t v_up_new   = in_up.read();

        // current data is actually the data from the prev cycle
        data_vec_t v_down_curr = v_down_reg;
        data_vec_t v_mid_curr  = v_mid_reg;
        data_vec_t v_up_curr   = v_up_reg;

        // PE0 for even pixels
        data_t a00_0  = v_mid_curr[0];      // Center
        data_t a01_0  = v_mid_curr[1];      // Right  
        data_t a0m1_0 = v_mid_prev[1];      // Left   (from prev cycle)
        data_t a10_0  = v_down_curr[0];     // Down
        data_t am10_0 = v_up_curr[0];       // Up

        data_t res_0_0 = a00_0 - a0m1_0;
        data_t res_1_0 = a00_0 - a01_0;
        data_t res_2_0 = a00_0 - am10_0;
        data_t res_3_0 = a00_0 - a10_0;
        data_t b_val_0 = (res_0_0 * res_0_0) + (res_1_0 * res_1_0) +
                         (res_2_0 * res_2_0) + (res_3_0 * res_3_0);

        // PE1 for odd pixels
        data_t a00_1  = v_mid_curr[1];      // Center
        data_t a01_1  = v_mid_new[0];       // Right  (from next cycle)
        data_t a0m1_1 = v_mid_curr[0];      // Left 
        data_t a10_1  = v_down_curr[1];     // Down
        data_t am10_1 = v_up_curr[1];       // Up

        data_t res_0_1 = a00_1 - a0m1_1;
        data_t res_1_1 = a00_1 - a01_1;
        data_t res_2_1 = a00_1 - am10_1;
        data_t res_3_1 = a00_1 - a10_1;
        data_t b_val_1 = (res_0_1 * res_0_1) + (res_1_1 * res_1_1) +
                         (res_2_1 * res_2_1) + (res_3_1 * res_3_1);

        // combining 2 vectors into one
        data_vec_t out_vec;
        out_vec[0] = b_val_0;
        out_vec[1] = b_val_1;
        out_B.write(out_vec);

        // regs switch to next value
        v_mid_prev = v_mid_curr;
        v_mid_reg  = v_mid_new;
        v_down_reg = v_down_new;
        v_up_reg   = v_up_new;
    }
}

void architecture_top_level(hls::stream<data_vec_t> &A_in, 
                            hls::stream<data_vec_t> &B_out) {
    #pragma HLS DATAFLOW

    // streams between FW
    hls::stream<data_vec_t> fw_direct_to_mid, fw_mid_to_up;
    #pragma HLS STREAM variable=fw_direct_to_mid depth=4
    #pragma HLS STREAM variable=fw_mid_to_up depth=4

    // Streams to kernel
    hls::stream<data_vec_t> pe_in_down, pe_in_mid, pe_in_up;
    #pragma HLS STREAM variable=pe_in_down depth=4
    #pragma HLS STREAM variable=pe_in_mid depth=4
    #pragma HLS STREAM variable=pe_in_up depth=4


    // Direct Forward  (Down)
    forwarding_module_vec<0, TOTAL_VECTORS+1>(A_in, fw_direct_to_mid, pe_in_down);

    // Middle row - latency 512
    forwarding_module_vec<FIFO_ROW_DEPTH, TOTAL_VECTORS+1>(fw_direct_to_mid, fw_mid_to_up, pe_in_mid);

    // upper row - latency 512
    terminal_forwarding_module_vec<FIFO_ROW_DEPTH, TOTAL_VECTORS+1>(fw_mid_to_up, pe_in_up);

    // comp kernel
    compute_kernel_k2<TOTAL_VECTORS+1>(pe_in_down, pe_in_mid, pe_in_up, B_out);
}
//everything is TOTALVECTORS+1 to flush the last values inside the registers