#include <stdio.h>  
#include "ap_int.h"    
#include "ap_fixed.h" 
#include "hls_stream.h"
#include "hls_math.h"  

typedef float data_t;

const int ROWS = 16;
const int COLUMNS = 1024; //II A (408)
const int TOTAL_ELEMENTS = ROWS * COLUMNS;

const int FIFO_0_DEPTH = 1023;
const int FIFO_1_DEPTH = 4;
const int FIFO_2_DEPTH = 4;
const int FIFO_3_DEPTH = 1023; //TABLE 3 (413)

const int KERNEL_ROWS = ROWS - 2; // 0+1 til 767-1 instead of 0-768
const int KERNEL_COLUMNS = COLUMNS - 2; // 0+1 til 1023-1 instead of 0-1024
const int KERNEL_ITERATIONS = KERNEL_ROWS * KERNEL_COLUMNS;

template <int T_TOTAL_ELEMENTS>
void data_splitter(hls::stream<data_t> &in,
                   hls::stream<data_t> &out_to_fifo,
                   hls::stream<data_t> &out_to_filter) { //FIG 5 (411)


    for (int i = 0; i < T_TOTAL_ELEMENTS; i++) {
        #pragma HLS PIPELINE II=1
        data_t temp = in.read();
        out_to_fifo.write(temp);
        out_to_filter.write(temp);
    }
}

template <int T_ROWS, int T_COLUMNS,
          int T_ROW_START, int T_ROW_END,
          int T_COL_START, int T_COL_END>
void data_filter(hls::stream<data_t>& in,
                 hls::stream<data_t>& out) { //FIG 5 (411)

    for (int i = 0; i < T_ROWS; i++) {
        for (int j = 0; j < T_COLUMNS; j++) {
            #pragma HLS PIPELINE II=1
            data_t data_in = in.read();

            // Έλεγχος αν το (i, j) ανήκει στο D_Ax αυτού του φίλτρου
            bool in_domain = (i >= T_ROW_START && i <= T_ROW_END &&
                              j >= T_COL_START && j <= T_COL_END);

            if (in_domain) {
                out.write(data_in);
            }
        }
    }
}

template <int T_ITERATIONS>
void compute_kernel(hls::stream<data_t>& in_1, // A[i+1][j]
                    hls::stream<data_t>& in_2, // A[i][j+1]
                    hls::stream<data_t>& in_3, // A[i][j]
                    hls::stream<data_t>& in_4, // A[i][j-1]
                    hls::stream<data_t>& in_5, // A[i-1][j]
                    hls::stream<data_t>& out_B) {

    for (int i = 0; i < T_ITERATIONS; i++) {
        #pragma HLS PIPELINE II=1

        //counters for current row/column
        int r = 0;
        int c = 0;

        // Reading the needed inputs
        data_t a10 = in_1.read(); //+1 0
        data_t a01 = in_2.read(); //0 +1
        data_t a00 = in_3.read(); //0 0
        data_t a0m1 = in_4.read(); //0 -1
        data_t am10 = in_5.read(); //-1 0

        data_t val_down, val_right, val_left, val_up;

        //For the padding, the extra rows/columns will be duplicates of the first/last rows/columns, so that the padded elements minus the center adds 0 to the result

        //bottom row filling
        if (r == ROWS - 1) a10 = a00;

        // right column filling
        if (c == COLUMNS - 1) a01 = a00;

        // left column filling
        if (c == 0) a0m1 = a00;

        // top row filling
        if (r == 0) am10 = a00;

        // Calculations in Listing 1 (408), Listing 2 (409)
        data_t res_0 = a00 - a0m1;
        data_t res_1 = a00 - a01;
        data_t res_2 = a00 - am10;
        data_t res_3 = a00 - a10;

        data_t b_val = (res_0 * res_0) + (res_1 * res_1) +
                       (res_2 * res_2) + (res_3 * res_3);

        out_B.write(b_val);

        if (c == COLUMNS - 1) {
            c = 0;
            r++;
        } else {
            c++;
        }
    }
}

template <int T_TOTAL_ELEMENTS>
void last_splitter_emptying(hls::stream<data_t>& in) { //Needed so that the daata forwarded from last splitter to non-existent FIFO is emptied
    for (int i = 0; i < T_TOTAL_ELEMENTS; i++) {
        #pragma HLS PIPELINE II=1
        in.read();
    }
}

void architecture_top_level(hls::stream<data_t> &A_in,
                         hls::stream<data_t> &B_out) {
    #pragma HLS DATAFLOW

    // FIFO initialisation
    hls::stream<data_t> fifo_0;
    #pragma HLS STREAM variable=fifo_0 depth=FIFO_0_DEPTH
    #pragma HLS BIND_STORAGE variable=fifo_0 type=fifo impl=bram
    hls::stream<data_t> fifo_1;
    #pragma HLS STREAM variable=fifo_1 depth=FIFO_1_DEPTH
    hls::stream<data_t> fifo_2;
    #pragma HLS STREAM variable=fifo_2 depth=FIFO_2_DEPTH
    hls::stream<data_t> fifo_3;
    #pragma HLS STREAM variable=fifo_3 depth=FIFO_3_DEPTH
    #pragma HLS BIND_STORAGE variable=fifo_3 type=fifo impl=bram

    // Intermediate results
    hls::stream<data_t> s0_to_f0, s1_to_f1, s2_to_f2, s3_to_f3, s4_to_f4;
    hls::stream<data_t> f0_to_compute, f1_to_compute, f2_to_compute, f3_to_compute, f4_to_compute;
    hls::stream<data_t> to_discard; // s4 out (not needed as described before)

    #pragma HLS STREAM variable=s0_to_f0 depth=4
    #pragma HLS STREAM variable=s1_to_f1 depth=4
    #pragma HLS STREAM variable=s2_to_f2 depth=4
    #pragma HLS STREAM variable=s3_to_f3 depth=4
    #pragma HLS STREAM variable=s4_to_f4 depth=4
    #pragma HLS STREAM variable=to_discard depth=4
    #pragma HLS STREAM variable=f0_to_compute depth=4   
    #pragma HLS STREAM variable=f1_to_compute depth=4 
    #pragma HLS STREAM variable=f2_to_compute depth=4
    #pragma HLS STREAM variable=f3_to_compute depth=4 
    #pragma HLS STREAM variable=f4_to_compute depth=4 


    // All modules initialisation (Acc to Figure 5 (411))
    // s0 (Down)
    data_splitter<TOTAL_ELEMENTS>(A_in, fifo_0, s0_to_f0);
    data_filter<ROWS, COLUMNS, 0, ROWS-1, 0, COLUMNS-1>(s0_to_f0, f0_to_compute);

    // s1 (Right)
    data_splitter<TOTAL_ELEMENTS>(fifo_0, fifo_1, s1_to_f1);
    data_filter<ROWS, COLUMNS, 0, ROWS-1, 0, COLUMNS-1>(s1_to_f1, f1_to_compute);

    // s2 (Center)
    data_splitter<TOTAL_ELEMENTS>(fifo_1, fifo_2, s2_to_f2);
    data_filter<ROWS, COLUMNS, 0, ROWS-1, 0, COLUMNS-1>(s2_to_f2, f2_to_compute);

    // s3 (Left)
    data_splitter<TOTAL_ELEMENTS>(fifo_2, fifo_3, s3_to_f3);
    data_filter<ROWS, COLUMNS, 0, ROWS-1, 0, COLUMNS-1>(s3_to_f3, f3_to_compute);

    // s4 (Up)
    data_splitter<TOTAL_ELEMENTS>(fifo_3, to_discard, s4_to_f4);
    data_filter<ROWS, COLUMNS, 0, ROWS-1, 0, COLUMNS-1>(s4_to_f4, f4_to_compute);

    last_splitter_emptying<TOTAL_ELEMENTS>(to_discard);

    // Computation Kernel
    compute_kernel<KERNEL_ITERATIONS>(
        f0_to_compute, f1_to_compute, f2_to_compute, f3_to_compute, f4_to_compute, B_out);
}