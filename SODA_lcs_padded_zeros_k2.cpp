#include "ap_int.h"    
#include "ap_fixed.h" 
#include "hls_stream.h"
#include "hls_math.h"  

const int k = 2;
typedef float data_t;

// needed for axi to read/write 2 floats in 1 cycle
struct float16 {
    data_t data[16];
};

struct float2 {
    data_t f0;
    data_t f1;
};

// Original sizes
const int ORIG_ROWS = 16;
const int ORIG_COLS = 1024;
const int ORIG_VEC_PER_ROW = ORIG_COLS / k;             // 512 for k=2
const int ORIG_TOTAL_VEC = (ORIG_ROWS * ORIG_COLS) / k; // 8192

// Sizes after padding
const int PADDED_ROWS = ORIG_ROWS + 2;                  // 18
const int PADDED_COLS = ORIG_COLS + 2;                  // 1026
const int PADDED_VEC_PER_ROW = PADDED_COLS / k;         // 513
const int PADDED_TOTAL_VEC = (PADDED_ROWS * PADDED_COLS) / k; // 9234

// final sizes after discarding
const int KERNEL_ROWS = ORIG_ROWS; 
const int KERNEL_COLS = ORIG_COLS; 
const int KERNEL_ITERATIONS = KERNEL_ROWS * KERNEL_COLS; // 16384 pixels

// pipeline flush constants, without them i get errors bcs it is trying to store when no data is available
// SODA now runs on PADDED dimensions
const int SODA_DELAY = PADDED_VEC_PER_ROW + 1;          // 514
const int COMPUTE_ITER = PADDED_TOTAL_VEC + SODA_DELAY; // 9748


// LOAD MODULE
void load_input(float16* in_mem, hls::stream<data_t>& out_0, hls::stream<data_t>& out_1) {
    float16 chunk; // register that holds 16 floats
    
    // Runs exactly for the original 8192 vectors
    for (int i = 0; i < ORIG_TOTAL_VEC; i++) {
        #pragma HLS PIPELINE II=1
        
        int burst_index = i / 8;  // in which pack of 16 floats are we
        int word_index  = i % 8;  // which pair of floats inside the pack are we using
        
        // loading from mem only once every 8 cycles
        if (word_index == 0) {
            chunk = in_mem[burst_index]; 
        }
        
        // writing 2 floats
        out_0.write(chunk.data[word_index * 2]);
        out_1.write(chunk.data[word_index * 2 + 1]);
    }
}

// PADDER MODULE
void padder(hls::stream<data_t>& in_0, hls::stream<data_t>& in_1,
            hls::stream<data_t>& out_0, hls::stream<data_t>& out_1) {
    
    data_t leftover = 0.0f;

    // Padder generates extra data and Flush cycles
    for (int i = 0; i < COMPUTE_ITER; i++) {
        #pragma HLS PIPELINE II=1

        if (i < PADDED_TOTAL_VEC) {
            int row = i / PADDED_VEC_PER_ROW;       //row
            int col_vec = i % PADDED_VEC_PER_ROW;   //vector of columns

            data_t val0 = 0.0f;
            data_t val1 = 0.0f;

            // Row 0 and last row are purely zeros
            if (row == 0 || row == PADDED_ROWS - 1) {
                val0 = 0.0f; 
                val1 = 0.0f;
            } else {
                // Internal rows: Shift logic for padding
                if (col_vec == 0) {
                    val0 = 0.0f; // Left border
                    val1 = in_0.read(); //first real stencil value
                    leftover = in_1.read();
                } else if (col_vec == PADDED_VEC_PER_ROW - 1) {
                    val0 = leftover; // second input of previous pair
                    val1 = 0.0f; // Right border
                } else {
                    val0 = leftover;
                    val1 = in_0.read();
                    leftover = in_1.read();
                }
            }
            out_0.write(val0);
            out_1.write(val1);
        } else {
            // Flush cycles
            out_0.write(0.0f);
            out_1.write(0.0f);
        }
    }
}

// Compute modules
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
    
    const int ITER = COMPUTE_ITER;

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
    // Forwarding modules use PADDED_VEC_PER_ROW now
    forwarding_module<1, ITER>(A_in_0, c0_fw1_next, pe0_down);
    forwarding_module<PADDED_VEC_PER_ROW - 1, ITER>(c0_fw1_next, c0_fw2_next, pe1_right);
    forwarding_module<1, ITER>(c0_fw2_next, c0_fw3_next, c0_split);
    split_1_to_2<ITER>(c0_split, pe0_center, pe1_left); 
    terminal_forwarding_module<PADDED_VEC_PER_ROW, ITER>(c0_fw3_next, pe0_up);

    // Reuse chain 1
    forwarding_module<1, ITER>(A_in_1, c1_fw1_next, pe1_down);
    forwarding_module<PADDED_VEC_PER_ROW, ITER>(c1_fw1_next, c1_fw2_next, c1_split);
    split_1_to_2<ITER>(c1_split, pe1_center, pe0_right); 
    forwarding_module<1, ITER>(c1_fw2_next, c1_fw3_next, pe0_left);
    terminal_forwarding_module<PADDED_VEC_PER_ROW - 1, ITER>(c1_fw3_next, pe1_up);

    // PEs
    compute_pe<ITER>(pe0_down, pe0_right, pe0_center, pe0_left, pe0_up, B_out_0);
    compute_pe<ITER>(pe1_down, pe1_right, pe1_center, pe1_left, pe1_up, B_out_1);
}

// STORE module
void store_output(hls::stream<data_t>& in_0, hls::stream<data_t>& in_1, float2* out_mem) {
    int write_idx = 0;
    data_t prev_val1 = 0.0f; 

    for (int i = 0; i < COMPUTE_ITER; i++) {
        #pragma HLS PIPELINE II=1
        
        data_t curr_val0 = in_0.read(); 
        data_t curr_val1 = in_1.read(); 

        int true_cycle = i - SODA_DELAY;

        if (true_cycle >= 0 && true_cycle < PADDED_TOTAL_VEC) {
            int row = true_cycle / PADDED_VEC_PER_ROW;
            int col_cycle = true_cycle % PADDED_VEC_PER_ROW; 

            // Extract inner core (ignoring padded rows and columns)
            if (row >= 1 && row < PADDED_ROWS - 1) {
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
void architecture_top_level(float16* A_in_mem, float2* B_out_mem) {
    
    // depth of B_out_mem is KERNEL_ITERATIONS / 2 because we store structs of 2 floats (float2)
    // depth of input is vectors/8 because by loading 512 bits we load 8 pairs of inputs
    #pragma HLS INTERFACE m_axi port=A_in_mem bundle=gmem0 depth=ORIG_TOTAL_VEC/8
    #pragma HLS INTERFACE m_axi port=B_out_mem bundle=gmem1 depth=(KERNEL_ITERATIONS/2)
    #pragma HLS INTERFACE s_axilite port=return
    #pragma HLS AGGREGATE variable=A_in_mem compact=auto

    #pragma HLS DATAFLOW

    hls::stream<data_t> in_raw_0("in_raw_0"), in_raw_1("in_raw_1");
    hls::stream<data_t> pad_0("pad_0"), pad_1("pad_1");
    hls::stream<data_t> out_pad_0("out_pad_0"), out_pad_1("out_pad_1");
    
    #pragma HLS STREAM variable=in_raw_0 depth=8 
    #pragma HLS STREAM variable=in_raw_1 depth=8 
    #pragma HLS STREAM variable=pad_0 depth=8 
    #pragma HLS STREAM variable=pad_1 depth=8 
    #pragma HLS STREAM variable=out_pad_0 depth=8 
    #pragma HLS STREAM variable=out_pad_1 depth=8 
    
    load_input(A_in_mem, in_raw_0, in_raw_1);
    padder(in_raw_0, in_raw_1, pad_0, pad_1);
    soda_compute(pad_0, pad_1, out_pad_0, out_pad_1);
    store_output(out_pad_0, out_pad_1, B_out_mem);
}