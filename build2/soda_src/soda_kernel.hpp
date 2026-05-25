// soda_kernel.hpp
#ifndef SODA_KERNEL_HPP
#define SODA_KERNEL_HPP

#include "soda_common.h"

// Compute modules, exactly same as before
template <int T_DEPTH, int T_ITER>
void forwarding_module(hls::stream<data_t>& in, hls::stream<data_t>& out_next_fw, hls::stream<data_t>& out_pe) { 
    // if depth is 0 make it 1
    data_t buffer[T_DEPTH == 0 ? 1 : T_DEPTH];
    #pragma HLS BIND_STORAGE variable=buffer type=ram_2p 
    int ptr = 0;
    
    for (int i = 0; i < T_ITER; i++) {
        #pragma HLS PIPELINE II=1
        data_t new_val = in.read();
        data_t out_val = (i < T_DEPTH) ? 0.0f : buffer[ptr]; 
        
        out_next_fw.write(out_val);
        out_pe.write(out_val);
        
        buffer[ptr] = new_val;
        // if ptr is tdepth-1 then make it 0, else increment
        ptr = (ptr == T_DEPTH - 1) ? 0 : ptr + 1;
    }
}

// New FW module used for all the intermediate PEs (same but writes to 3 different PEs)
template <int T_DEPTH, int T_ITER>
void forward_and_split_3(hls::stream<data_t>& in, hls::stream<data_t>& out_next_fw, 
                         hls::stream<data_t>& pe_center, hls::stream<data_t>& pe_left, hls::stream<data_t>& pe_right) {
    data_t buffer[T_DEPTH == 0 ? 1 : T_DEPTH];
    #pragma HLS BIND_STORAGE variable=buffer type=ram_2p 
    int ptr = 0;

    for (int i = 0; i < T_ITER; i++) {
        #pragma HLS PIPELINE II=1
        data_t new_val = in.read();
        data_t out_val = (i < T_DEPTH) ? 0.0f : buffer[ptr];

        out_next_fw.write(out_val);
        pe_center.write(out_val);
        pe_left.write(out_val);
        pe_right.write(out_val);

        buffer[ptr] = new_val;
        ptr = (ptr == T_DEPTH - 1) ? 0 : ptr + 1;
    }
}

//terminal module same as regular, but it only writes to one output
template <int T_DEPTH, int T_ITER>
void terminal_forwarding_module(hls::stream<data_t>& in, hls::stream<data_t>& out_pe) {
    data_t buffer[T_DEPTH == 0 ? 1 : T_DEPTH];
    #pragma HLS BIND_STORAGE variable=buffer type=ram_2p
    int ptr = 0;
    
    for (int i = 0; i < T_ITER; i++) {
        #pragma HLS PIPELINE II=1
        data_t new_val = in.read();
        data_t out_val = (i < T_DEPTH) ? 0.0f : buffer[ptr];
        
        out_pe.write(out_val);
        
        buffer[ptr] = new_val;
        ptr = (ptr == T_DEPTH - 1) ? 0 : ptr + 1;
    }
}

template <int T_ITER>
void split_1_to_2(hls::stream<data_t>& in, hls::stream<data_t>& out1, hls::stream<data_t>& out2) {
    for (int i = 0; i < T_ITER; i++) {
        #pragma HLS PIPELINE II=1
        data_t val = in.read();
        out1.write(val);
        out2.write(val);
    }
}

template <int T_ITER>
void compute_pe(hls::stream<data_t>& in_down, hls::stream<data_t>& in_right,  
                hls::stream<data_t>& in_center, hls::stream<data_t>& in_left,   
                hls::stream<data_t>& in_up, hls::stream<data_t>& out_res) {
    for (int i = 0; i < T_ITER; i++) {
        #pragma HLS PIPELINE II=1
        
        data_t down   = in_down.read();
        data_t right  = in_right.read();
        data_t center = in_center.read();
        data_t left   = in_left.read();
        data_t up     = in_up.read();

        data_t res_0 = center - left;
        data_t res_1 = center - right;
        data_t res_2 = center - up;
        data_t res_3 = center - down;
        data_t b_val = (res_0 * res_0) + (res_1 * res_1) +
                       (res_2 * res_2) + (res_3 * res_3);

        out_res.write(b_val);
    }
}

template <int K_FACTOR, int ITER>
void soda_compute(hls::stream<data_t> A_in[K_FACTOR], hls::stream<data_t> B_out[K_FACTOR]) {
    #pragma HLS DATAFLOW

    hls::stream<data_t> pe_down[K_FACTOR], pe_right[K_FACTOR], pe_center[K_FACTOR], pe_left[K_FACTOR], pe_up[K_FACTOR];

    #pragma HLS ARRAY_PARTITION variable=pe_down complete
    #pragma HLS ARRAY_PARTITION variable=pe_right complete
    #pragma HLS ARRAY_PARTITION variable=pe_center complete
    #pragma HLS ARRAY_PARTITION variable=pe_left complete
    #pragma HLS ARRAY_PARTITION variable=pe_up complete

    #pragma HLS STREAM variable=pe_down depth=16
    #pragma HLS STREAM variable=pe_right depth=16
    #pragma HLS STREAM variable=pe_center depth=16
    #pragma HLS STREAM variable=pe_left depth=16
    #pragma HLS STREAM variable=pe_up depth=16

    hls::stream<data_t> fw1[K_FACTOR], fw2[K_FACTOR], fw3[K_FACTOR], split[K_FACTOR];
    
    #pragma HLS ARRAY_PARTITION variable=fw1 complete
    #pragma HLS ARRAY_PARTITION variable=fw2 complete
    #pragma HLS ARRAY_PARTITION variable=fw3 complete
    #pragma HLS ARRAY_PARTITION variable=split complete

    for (int k = 0; k < K_FACTOR; k++) {
        #pragma HLS UNROLL

        if (K_FACTOR == 1) {
            // if k=1, then we dont need any forwarding modules and thats why this is empty
        }
        else if (k == 0) {
            // for first PE
            forwarding_module<1, ITER>(A_in[k], fw1[k], pe_down[k]);
            // this one is the right element of the last calculation of previous cycle, thats why there is 1 less delay
            forwarding_module<VECTORS_PER_ROW - 1, ITER>(fw1[k], fw2[k], pe_right[K_FACTOR - 1]); 
            forwarding_module<1, ITER>(fw2[k], fw3[k], split[k]);
            split_1_to_2<ITER>(split[k], pe_center[k], pe_left[k+1]);
            terminal_forwarding_module<VECTORS_PER_ROW, ITER>(fw3[k], pe_up[k]);
        }
        else if (k == K_FACTOR - 1) {
            // for last PE
            forwarding_module<1, ITER>(A_in[k], fw1[k], pe_down[k]);
            forwarding_module<VECTORS_PER_ROW, ITER>(fw1[k], fw2[k], split[k]);
            split_1_to_2<ITER>(split[k], pe_center[k], pe_right[k-1]); 
            // element to the left of k=0 comes for the previous cycle
            forwarding_module<1, ITER>(fw2[k], fw3[k], pe_left[0]);
            terminal_forwarding_module<VECTORS_PER_ROW - 1, ITER>(fw3[k], pe_up[k]);
        }
        else {
            // all the intermediate ones. each one feeds symmetrically adjacent PEs in same cycle
            forwarding_module<1, ITER>(A_in[k], fw1[k], pe_down[k]);
            forward_and_split_3<VECTORS_PER_ROW, ITER>(fw1[k], fw2[k], pe_center[k], pe_left[k+1], pe_right[k-1]);
            terminal_forwarding_module<VECTORS_PER_ROW, ITER>(fw2[k], pe_up[k]);
        }
    }

    // all PEs
    for (int k = 0; k < K_FACTOR; k++) {
        #pragma HLS UNROLL
        compute_pe<ITER>(pe_down[k], pe_right[k], pe_center[k], pe_left[k], pe_up[k], B_out[k]);
    }
}

#endif // SODA_KERNEL_HPP