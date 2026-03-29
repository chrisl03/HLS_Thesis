#include "ap_int.h"    
#include "ap_fixed.h" 
#include "hls_stream.h"
#include "hls_math.h"  

const int k = 2;

typedef float data_t;

const int ROWS = 16;
const int COLUMNS = 1024;

const int VECTORS_PER_ROW = COLUMNS / k;        // 512 for k=2
const int TOTAL_VECTORS = (ROWS * COLUMNS) / k; // 8192


template <int T_DEPTH, int T_ITER>
void forwarding_module(hls::stream<data_t>& in, hls::stream<data_t>& out_next_fw, hls::stream<data_t>& out_pe) { 
    // if depth is 0 make it 1
    data_t buffer[T_DEPTH == 0 ? 1 : T_DEPTH];
    #pragma HLS BIND_STORAGE variable=buffer type=ram_2p 
    int ptr = 0;
    
    // Fifo or ff with reuse buff
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

//
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
void compute_pe(hls::stream<data_t>& in_down, 
                hls::stream<data_t>& in_right,  
                hls::stream<data_t>& in_center,  
                hls::stream<data_t>& in_left,   
                hls::stream<data_t>& in_up,     
                hls::stream<data_t>& out_res) {
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





void architecture_top_level(hls::stream<data_t> &A_in_0, 
                            hls::stream<data_t> &A_in_1, 
                            hls::stream<data_t> &B_out_0,
                            hls::stream<data_t> &B_out_1) {
    #pragma HLS DATAFLOW

    const int ITER = TOTAL_VECTORS + 1;

    // Reuse chain 0 streams
    hls::stream<data_t> c0_fw1_next("c0_fw1_next"), pe0_down("pe0_down");
    hls::stream<data_t> c0_fw2_next("c0_fw2_next"), pe1_right("pe1_right");
    hls::stream<data_t> c0_fw3_next("c0_fw3_next"), c0_split("c0_split");
    hls::stream<data_t> pe0_center("pe0_center"), pe1_left("pe1_left");
    hls::stream<data_t> pe0_up("pe0_up");
    
    #pragma HLS STREAM variable=c0_fw1_next depth=4
    #pragma HLS STREAM variable=c0_fw2_next depth=4
    #pragma HLS STREAM variable=c0_fw3_next depth=4
    #pragma HLS STREAM variable=c0_split depth=4

    // Reuse chain 1 streams
    hls::stream<data_t> c1_fw1_next("c1_fw1_next"), pe1_down("pe1_down");
    hls::stream<data_t> c1_fw2_next("c1_fw2_next"), c1_split("c1_split");
    hls::stream<data_t> pe1_center("pe1_center"), pe0_right("pe0_right");
    hls::stream<data_t> c1_fw3_next("c1_fw3_next"), pe0_left("pe0_left");
    hls::stream<data_t> pe1_up("pe1_up");

    #pragma HLS STREAM variable=c1_fw1_next depth=4
    #pragma HLS STREAM variable=c1_fw2_next depth=4
    #pragma HLS STREAM variable=c1_fw3_next depth=4
    #pragma HLS STREAM variable=c1_split depth=4

    // Reuse chain 0
    forwarding_module<1, ITER>(A_in_0, c0_fw1_next, pe0_down);
    forwarding_module<VECTORS_PER_ROW - 1, ITER>(c0_fw1_next, c0_fw2_next, pe1_right);
    forwarding_module<1, ITER>(c0_fw2_next, c0_fw3_next, c0_split);
    split_1_to_2<ITER>(c0_split, pe0_center, pe1_left); 
    terminal_forwarding_module<VECTORS_PER_ROW, ITER>(c0_fw3_next, pe0_up);

    // Reuse chain 1
    forwarding_module<1, ITER>(A_in_1, c1_fw1_next, pe1_down);
    forwarding_module<VECTORS_PER_ROW, ITER>(c1_fw1_next, c1_fw2_next, c1_split);
    split_1_to_2<ITER>(c1_split, pe1_center, pe0_right); 
    forwarding_module<1, ITER>(c1_fw2_next, c1_fw3_next, pe0_left);
    terminal_forwarding_module<VECTORS_PER_ROW - 1, ITER>(c1_fw3_next, pe1_up);

    // PEs
    compute_pe<ITER>(pe0_down, pe0_right, pe0_center, pe0_left, pe0_up, B_out_0);
    compute_pe<ITER>(pe1_down, pe1_right, pe1_center, pe1_left, pe1_up, B_out_1);
}