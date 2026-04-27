#include "ap_int.h"    
#include "ap_fixed.h" 
#include "hls_stream.h"
#include "hls_math.h"  

const int K = 8; // Το κεντρικό μας parameter
typedef float data_t;

// needed for axi to read/write in bursts
struct float16 {
    data_t data[16];
};

//Output pack
struct float_pack {
    data_t data[K];
};

const int ROWS = 16;
const int COLUMNS = 1024;
const int VECTORS_PER_ROW = COLUMNS / K;        // 512 for K=2
const int TOTAL_VECTORS = (ROWS * COLUMNS) / K; // 8192

// final sizes after discarding
const int KERNEL_ROWS = ROWS - 2; 
const int KERNEL_COLS = COLUMNS - 2; 
const int KERNEL_ITERATIONS = KERNEL_ROWS * KERNEL_COLS; // 14308 pixels

const int SODA_DELAY = VECTORS_PER_ROW + 1;              
// extra 16 cycles to flush the last data, fow k =8 and 16 for example, because 1022 cant be divided by 8 and 16
const int TOTAL_ITERATIONS = TOTAL_VECTORS + SODA_DELAY + 16;

// Load, pretty much the same but parametric
void load_input(float16* in_mem, hls::stream<data_t> out[K]) {
    float16 chunk; 
    
    for (int i = 0; i < TOTAL_ITERATIONS; i++) {
        #pragma HLS PIPELINE II=1
        
        if (i < TOTAL_VECTORS) {
            int burst_index = i / (16 / K); 
            int word_index  = i % (16 / K);  
            
            if (word_index == 0) {
                chunk = in_mem[burst_index]; 
            }
            
            // unroll the loop and write simultaneously to all cables
            for (int k_idx = 0; k_idx < K; k_idx++) {
                #pragma HLS UNROLL
                out[k_idx].write(chunk.data[word_index * K + k_idx]);
            }
        } else {
            // Pipeline Flushing
            for (int k_idx = 0; k_idx < K; k_idx++) {
                #pragma HLS UNROLL
                out[k_idx].write(0.0f);
            }
        }
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

    #pragma HLS STREAM variable=pe_down depth=4
    #pragma HLS STREAM variable=pe_right depth=4
    #pragma HLS STREAM variable=pe_center depth=4
    #pragma HLS STREAM variable=pe_left depth=4
    #pragma HLS STREAM variable=pe_up depth=4

    hls::stream<data_t> fw1[K_FACTOR], fw2[K_FACTOR], fw3[K_FACTOR], split[K_FACTOR];

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

// STORE MODULE
void store_output(hls::stream<data_t> in[K], float_pack* out_mem) {
    int write_idx = 0;

    data_t buffer[K];
    #pragma HLS ARRAY_PARTITION variable=buffer complete
    for(int i = 0; i < K; i++) buffer[i] = 0.0f;

    int buf_count = 0;
    bool done = false;
    int dummy_count = 0;

    for (int i = 0; i < TOTAL_ITERATIONS; i++) {
        #pragma HLS PIPELINE II=1

        // 1. Διάβασμα εισόδου με πλήρες Array Partitioning για αποφυγή Port Bottleneck
        data_t curr_val[K];
        #pragma HLS ARRAY_PARTITION variable=curr_val complete
        for (int k_idx = 0; k_idx < K; k_idx++) {
            #pragma HLS UNROLL
            curr_val[k_idx] = in[k_idx].read();
        }

        int true_cycle = i - SODA_DELAY;

        if (true_cycle >= 0 && true_cycle < TOTAL_VECTORS) {
            int row = true_cycle / VECTORS_PER_ROW;
            int col_cycle = true_cycle % VECTORS_PER_ROW;

            if (row >= 1 && row < ROWS - 1) {

                // 2. Εξαγωγή valid pixels με ternary operators (πιο "ρηχό" logic από if-else)
                int start_idx = (col_cycle == 0) ? 1 : 0;
                int valid_count = (col_cycle == 0 || col_cycle == VECTORS_PER_ROW - 1) ? K - 1 : K;

                data_t valid_pixels[K];
                #pragma HLS ARRAY_PARTITION variable=valid_pixels complete
                for(int j = 0; j < K; j++) {
                    #pragma HLS UNROLL
                    valid_pixels[j] = (j < valid_count) ? curr_val[start_idx + j] : 0.0f;
                }

                // 3. Merging: Γρήγορη ολίσθηση (Shifting) αντί για Nested Loops!
                data_t merged[2 * K];
                #pragma HLS ARRAY_PARTITION variable=merged complete
                
                // α) Αντιγραφή του παλιού buffer στην αρχή του πάγκου
                for (int j = 0; j < K; j++) {
                    #pragma HLS UNROLL
                    merged[j] = buffer[j];
                    merged[K + j] = 0.0f; 
                }
                
                // β) Απευθείας τοποθέτηση των νέων pixels με offset το buf_count
                for (int j = 0; j < K; j++) {
                    #pragma HLS UNROLL
                    merged[buf_count + j] = valid_pixels[j];
                }

                // 4. Πακετάρισμα και ενημέρωση buffer
                int new_total = buf_count + valid_count;

                if (new_total >= K) {
                    float_pack pack;
                    for (int j = 0; j < K; j++) pack.data[j] = merged[j];
                    out_mem[write_idx++] = pack;

                    buf_count = new_total - K;
                    for (int j = 0; j < K; j++) buffer[j] = merged[K + j];
                } else {
                    buf_count = new_total;
                    for (int j = 0; j < K; j++) buffer[j] = merged[j];
                }

                // Σηκώνουμε σημαία στο τελευταίο valid vector
                if (row == ROWS - 2 && col_cycle == VECTORS_PER_ROW - 1) done = true;
            }
        }
        else {
            // 5. Το καθαρό 16-cycle Dummy Flush που ενσωματώνει και το leftover
            if (done && dummy_count < 16) {
                float_pack dummy_pack;

                for (int j = 0; j < K; j++) {
                    #pragma HLS UNROLL
                    // Στον 1ο dummy κύκλο βάζουμε ό,τι έμεινε, αλλιώς μηδέν
                    dummy_pack.data[j] = (dummy_count == 0 && j < buf_count) ? buffer[j] : 0.0f;
                }

                if (dummy_count == 0) buf_count = 0; // Αδειάσαμε με επιτυχία!

                out_mem[write_idx++] = dummy_pack;
                dummy_count++;
            }
        }
    }
}

// TOP LEVEL
void architecture_top_level(float16* A_in_mem, float_pack* B_out_mem) {
    #pragma HLS INTERFACE m_axi port=A_in_mem bundle=gmem0 depth=(TOTAL_VECTORS/(16/K))
    #pragma HLS INTERFACE m_axi port=B_out_mem bundle=gmem1 depth=(KERNEL_ITERATIONS/K + 16)
    #pragma HLS INTERFACE s_axilite port=return
    #pragma HLS AGGREGATE variable=A_in_mem compact=auto
    #pragma HLS AGGREGATE variable=B_out_mem compact=auto

    #pragma HLS DATAFLOW

    hls::stream<data_t> in_streams[K];
    hls::stream<data_t> out_streams[K];
    
    #pragma HLS STREAM variable=in_streams depth=8 
    #pragma HLS STREAM variable=out_streams depth=8 
    
    load_input(A_in_mem, in_streams);
    soda_compute<K, TOTAL_ITERATIONS>(in_streams, out_streams);
    store_output(out_streams, B_out_mem);
}