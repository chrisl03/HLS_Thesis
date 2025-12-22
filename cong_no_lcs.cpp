#include <stdio.h>  
#include "ap_int.h"    
#include "ap_fixed.h" 
#include "hls_stream.h"
#include "hls_math.h"  

typedef float data_t;

const int ORIG_ROWS = 16;
const int ORIG_COLS = 1024;
const int ORIG_TOTAL = ORIG_ROWS * ORIG_COLS;

const int PAD_ROWS = 18;
const int PAD_COLS = 1026;
const int PAD_TOTAL = PAD_ROWS * PAD_COLS;

const int OUT_ITERATIONS = ORIG_ROWS * ORIG_COLS;

const int FIFO_0_DEPTH = 1025; //instead of 1023, due to the padding
const int FIFO_1_DEPTH = 4;
const int FIFO_2_DEPTH = 4;
const int FIFO_3_DEPTH = 1025;

void padding_generator_safe(hls::stream<data_t> &in, hls::stream<data_t> &out) {
    
    data_t line_buf[ORIG_COLS]; // Used for Top/Bottom rows

    for (int r = 0; r < ORIG_ROWS; r++) {
        
        //if First row load to buffer to write in the next row as well
        if (r == 0) {
    
    
    static data_t last_pix_val;
    
    //This loop saves first row (in order to be used in the next row as well) and writes it into the new first row (padded)
    for (int c = 0; c < PAD_COLS; c++) {
        #pragma HLS PIPELINE II=1
        data_t val;
        
        if (c == 0) {
            data_t p = in.read();    // read
            line_buf[0] = p;         // save for second collumn (originally first)
            val = p;                 // write in first collumn
        } 
        else if (c == 1) {
            val = line_buf[0];       // second collumn (originally first)
        }
        else if (c > 1 && c < PAD_COLS - 1) {
            data_t p = in.read();    // read
            line_buf[c-1] = p;       // save   (currently saving everything in buffer in order to have it ready to be copied in the padded row, otherwise everything is lost)
            val = p;                 // write
            if (c == PAD_COLS - 2) last_pix_val = p; //saving for last column pad
        }
        else { 
            val = last_pix_val; //last column pad
        }
        out.write(val);
    }

    // writing original Row 0 (now row 1), using line_buf
    for (int c = 0; c < PAD_COLS; c++) {     //c is padded index (0-1025) while idx is a temporary index in order to write correctly 
        #pragma HLS PIPELINE II=1
        int idx = c - 1;
        if (idx < 0) idx = 0;
        if (idx >= ORIG_COLS) idx = ORIG_COLS - 1;
        out.write(line_buf[idx]);
    }
}
        
        // Row 15
        else if (r == ORIG_ROWS - 1) {   //if is in line 31 (if r==0)
            
            static data_t last_pix_val;
            
            // this loop writes the original last row, whhile at the same time saving it for the bottom row pad
            for (int c = 0; c < PAD_COLS; c++) {
                #pragma HLS PIPELINE II=1
                data_t val;
                
                if (c == 0) {
                    data_t p = in.read();
                    line_buf[0] = p;
                    val = p;         // Left Pad
                } 
                else if (c == 1) {
                    val = line_buf[0]; 
                }
                else if (c > 1 && c < PAD_COLS - 1) {
                    data_t p = in.read();
                    line_buf[c-1] = p; 
                    val = p;
                    if (c == PAD_COLS - 2) last_pix_val = p;
                }
                else { // Right Pad
                    val = last_pix_val;
                }
                out.write(val);
            }

            // This loop copies original last row into the new bottom pad
            for (int c = 0; c < PAD_COLS; c++) {
                #pragma HLS PIPELINE II=1
                int idx = c - 1;
                if (idx < 0) idx = 0;
                if (idx >= ORIG_COLS) idx = ORIG_COLS - 1;
                out.write(line_buf[idx]);
            }
        }

        // Intermidiate rows, only first and last column padding, no need to fill the buffer
        else {
            static data_t first_pixel, last_pixel;
            
            for (int c = 0; c < PAD_COLS; c++) {
                #pragma HLS PIPELINE II=1
                data_t val;
                
                if (c == 0) {
                    first_pixel = in.read(); 
                    val = first_pixel; 
                } 
                else if (c == 1) {
                    val = first_pixel;
                }
                else if (c > 1 && c < PAD_COLS - 1) {
                    data_t p = in.read();
                    val = p;
                    if (c == PAD_COLS - 2) last_pixel = p;
                }
                else { 
                    val = last_pixel;
                }
                out.write(val);
            }
        }
    }
}


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

        // Calculations in Listing 1 (408), Listing 2 (409)
        data_t res_0 = a00 - a0m1;
        data_t res_1 = a00 - a01;
        data_t res_2 = a00 - am10;
        data_t res_3 = a00 - a10;

        data_t b_val = (res_0 * res_0) + (res_1 * res_1) +
                       (res_2 * res_2) + (res_3 * res_3);

        out_B.write(b_val);
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

    hls::stream<data_t> A_padded;
    #pragma HLS STREAM variable=A_padded depth=4
    
    //  (16x1024 -> 18x1026)
    padding_generator_safe(A_in, A_padded);

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
    // s0 (Down): Pass Rows 2..17
    data_splitter<PAD_TOTAL>(A_padded, fifo_0, s0_to_f0);
    data_filter<PAD_ROWS, PAD_COLS, 2, 17, 1, 1024>(s0_to_f0, f0_to_compute);

    // s1 (Right): Pass Cols 2..1025
    data_splitter<PAD_TOTAL>(fifo_0, fifo_1, s1_to_f1);
    data_filter<PAD_ROWS, PAD_COLS, 1, 16, 2, 1025>(s1_to_f1, f1_to_compute);

    // s2 (Center): Pass Center (1..16, 1..1024)
    data_splitter<PAD_TOTAL>(fifo_1, fifo_2, s2_to_f2);
    data_filter<PAD_ROWS, PAD_COLS, 1, 16, 1, 1024>(s2_to_f2, f2_to_compute);

    // s3 (Left): Pass Cols 0..1023
    data_splitter<PAD_TOTAL>(fifo_2, fifo_3, s3_to_f3);
    data_filter<PAD_ROWS, PAD_COLS, 1, 16, 0, 1023>(s3_to_f3, f3_to_compute);

    // s4 (Up): Pass Rows 0..15
    data_splitter<PAD_TOTAL>(fifo_3, to_discard, s4_to_f4);
    data_filter<PAD_ROWS, PAD_COLS, 0, 15, 1, 1024>(s4_to_f4, f4_to_compute);

    last_splitter_emptying<PAD_TOTAL>(to_discard);

    // Kernel produces exactly 16x1024 pixels
    compute_kernel<OUT_ITERATIONS>(
        f0_to_compute, f1_to_compute, f2_to_compute, f3_to_compute, f4_to_compute, B_out);
}