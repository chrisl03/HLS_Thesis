#include "ap_int.h"    
#include "ap_fixed.h" 
#include "hls_stream.h"
#include "hls_math.h"  

const int k = 2;
typedef float data_t;

// needed for axi to read/write 2 floats in 1 cycle
struct float2 {
    data_t f0;
    data_t f1;
};

const int ROWS = 16;
const int COLUMNS = 1024;
const int VECTORS_PER_ROW = COLUMNS / k;        // 512 for k=2
const int TOTAL_VECTORS = (ROWS * COLUMNS) / k; // 8192

// final sizes after discarding
const int KERNEL_ROWS = ROWS - 2; 
const int KERNEL_COLS = COLUMNS - 2; 
const int KERNEL_ITERATIONS = KERNEL_ROWS * KERNEL_COLS; // 14308 pixels

// ipeline flush constants, without them i get errors bcs it is trying to store when no data is available
const int SODA_DELAY = VECTORS_PER_ROW + 1;              // 513
const int TOTAL_ITERATIONS = TOTAL_VECTORS + SODA_DELAY; // 8705


// LOAD MODULE
void load_input(float2* in_mem, hls::stream<data_t>& out_0, hls::stream<data_t>& out_1) {
    // Τotal vectors + delay for first
    for (int i = 0; i < TOTAL_ITERATIONS; i++) {
        #pragma HLS PIPELINE II=1
        float2 temp;
        
        if (i < TOTAL_VECTORS) {
            temp = in_mem[i]; 
        } else {
            temp.f0 = 0.0f; temp.f1 = 0.0f; // Dummy data for flushing the pipeline
        }
        
        out_0.write(temp.f0);
        out_1.write(temp.f1);
    }
}

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


// SODA top level, now is compute module
void soda_compute(hls::stream<data_t> &A_in_0, hls::stream<data_t> &A_in_1, 
                  hls::stream<data_t> &B_out_0, hls::stream<data_t> &B_out_1) {
    #pragma HLS DATAFLOW
    
    const int ITER = TOTAL_ITERATIONS;

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

// STORE module
void store_output(hls::stream<data_t>& in_0, hls::stream<data_t>& in_1, float2* out_mem) {
    int write_idx = 0;
    data_t prev_val1 = 0.0f; 

    for (int i = 0; i < TOTAL_ITERATIONS; i++) {
        #pragma HLS PIPELINE II=1
        
        data_t curr_val0 = in_0.read(); 
        data_t curr_val1 = in_1.read(); 

        int true_cycle = i - SODA_DELAY;

        if (true_cycle >= 0 && true_cycle < TOTAL_VECTORS) {
            int row = true_cycle / VECTORS_PER_ROW;
            int col_cycle = true_cycle % VECTORS_PER_ROW; 

            if (row >= 1 && row < ROWS - 1) {
                if (col_cycle >= 1) {
                    float2 pack = {prev_val1, curr_val0};
                    out_mem[write_idx++] = pack;
                }
            }
            // odd values show up 1 cycle later, bcs for example at cycle 1 the result of the odd PE is trash, while the even is good. So odd results show up one cycle later
            prev_val1 = curr_val1; 
        }
    }
}

// Top level
void architecture_top_level(float2* A_in_mem, float2* B_out_mem) {
    
    // depth of B_out_mem is KERNEL_ITERATIONS / 2 because we store structs of 2 floats (float2)
    #pragma HLS INTERFACE m_axi port=A_in_mem bundle=gmem0 depth=TOTAL_VECTORS
    #pragma HLS INTERFACE m_axi port=B_out_mem bundle=gmem1 depth=(KERNEL_ITERATIONS/2)
    #pragma HLS INTERFACE s_axilite port=return

    #pragma HLS DATAFLOW

    hls::stream<data_t> in_0("in_0"), in_1("in_1");
    hls::stream<data_t> out_0("out_0"), out_1("out_1");
    
    #pragma HLS STREAM variable=in_0 depth=8 
    #pragma HLS STREAM variable=in_1 depth=8 
    #pragma HLS STREAM variable=out_0 depth=8 
    #pragma HLS STREAM variable=out_1 depth=8 
    
    load_input(A_in_mem, in_0, in_1);
    soda_compute(in_0, in_1, out_0, out_1);
    store_output(out_0, out_1, B_out_mem);
}