#include <stdio.h>   
#include "ap_int.h"    
#include "ap_fixed.h"   
#include "hls_stream.h" 
#include "hls_math.h" 
#include <vector> 
#include <cmath>
#include <fstream>  // Added for file writing
#include <iomanip>  // Added for formatting

typedef float data_t;

// --- CONSTANTS ---
const int ROWS = 16;
const int COLUMNS = 1024;
const int TOTAL_ELEMENTS = ROWS * COLUMNS;
const int KERNEL_ITERATIONS = TOTAL_ELEMENTS; 

void architecture_top_level(hls::stream<data_t>& A_in,
                          hls::stream<data_t>& B_out);

// --- GOLDEN MODEL (With Padding Logic) ---
void compute_golden(std::vector<data_t>& A_vec, std::vector<data_t>& B_golden_vec) {
    B_golden_vec.clear();
    for (int i = 0; i < ROWS; i++) {
        for (int j = 0; j < COLUMNS; j++) {
            
            // Replicate Padding Indices
            int r_down  = (i == ROWS - 1)    ? i : i + 1;
            int c_right = (j == COLUMNS - 1) ? j : j + 1;
            int c_left  = (j == 0)           ? j : j - 1;
            int r_up    = (i == 0)           ? i : i - 1;

            data_t a00  = A_vec[i * COLUMNS + j];       
            data_t a10  = A_vec[r_down * COLUMNS + j];  
            data_t a01  = A_vec[i * COLUMNS + c_right]; 
            data_t a0m1 = A_vec[i * COLUMNS + c_left];  
            data_t am10 = A_vec[r_up * COLUMNS + j];    

            data_t res_0 = a00 - a0m1;
            data_t res_1 = a00 - a01;
            data_t res_2 = a00 - am10;
            data_t res_3 = a00 - a10;

            data_t b_val = (res_0 * res_0) + (res_1 * res_1) +
                           (res_2 * res_2) + (res_3 * res_3);

            B_golden_vec.push_back(b_val);
        }
    }
}

// --- HELPER: Save Matrix to File ---
void save_matrix_to_file(const std::vector<data_t>& data, const char* filename) {
    std::ofstream outfile(filename);
    if (!outfile.is_open()) {
        printf("[TB] Error opening file %s\n", filename);
        return;
    }

    outfile << "      ";
    for(int j=0; j<COLUMNS; j++) outfile << std::setw(8) << j; 
    outfile << "\n";

    for (int i = 0; i < ROWS; i++) {
        outfile << "R" << std::setw(3) << i << ": ";
        for (int j = 0; j < COLUMNS; j++) {
            outfile << std::setw(8) << std::fixed << std::setprecision(1) << data[i*COLUMNS + j];
        }
        outfile << "\n";
    }
    outfile.close();
    printf("[TB] Saved full results to %s\n", filename);
}

// --- HELPER: Print Small Corner to Console ---
void print_corner_comparison(const std::vector<data_t>& golden, const std::vector<data_t>& hls) {
    printf("\n--- VISUAL CHECK (Top-Left 8x8) ---\n");
    printf("Format: [Golden | HLS]\n");
    
    int limit_r = (ROWS < 8) ? ROWS : 8;
    int limit_c = (COLUMNS < 8) ? COLUMNS : 8;

    for (int i = 0; i < limit_r; i++) {
        printf("Row %2d: ", i);
        for (int j = 0; j < limit_c; j++) {
            int idx = i * COLUMNS + j;
            // Print integer part for compactness if they are whole numbers
            printf("[%3.0f|%3.0f] ", golden[idx], hls[idx]);
        }
        printf("\n");
    }
    printf("-----------------------------------\n");
}

int main() {
    printf("[TB] Starting Testbench...\n");

    // 1. Setup Data
    std::vector<data_t> A_input(TOTAL_ELEMENTS);
    std::vector<data_t> B_golden;
    std::vector<data_t> B_hls(TOTAL_ELEMENTS); // Pre-allocate

    // Initialize with a pattern that makes tracking easy
    // Value = Row index (so all pixels in row 0 are 0, row 1 are 1...)
    // This makes gradients easy to predict (Vertical diff=1, Horizontal diff=0)
    for (int i = 0; i < ROWS; i++) {
        for(int j=0; j < COLUMNS; j++) {
            A_input[i*COLUMNS + j] = (data_t)i; 
        }
    }

    // 2. Compute Golden
    compute_golden(A_input, B_golden);

    // 3. Run HLS
    hls::stream<data_t> A_in_stream("A_in");
    hls::stream<data_t> B_out_stream("B_out");

    for (int i = 0; i < TOTAL_ELEMENTS; i++) A_in_stream.write(A_input[i]);

    printf("[TB] Running HLS Kernel...\n");
    architecture_top_level(A_in_stream, B_out_stream);

    // 4. Capture HLS Output
    for (int i = 0; i < KERNEL_ITERATIONS; i++) {
        if (B_out_stream.empty()) {
            printf("[FATAL] Stream empty at index %d!\n", i);
            break; 
        }
        B_hls[i] = B_out_stream.read();
    }

    // 5. Compare and Report
    int errors = 0;
    for (int i = 0; i < TOTAL_ELEMENTS; i++) {
        if (std::abs(B_hls[i] - B_golden[i]) > 0.001f) errors++;
    }

    // 6. Visualization
    print_corner_comparison(B_golden, B_hls);
    save_matrix_to_file(B_golden, "golden_output.txt");
    save_matrix_to_file(B_hls, "hls_output.txt");

    if (errors == 0) {
        printf("\n--- TEST PASSED ---\n");
    } else {
        printf("\n--- TEST FAILED: %d mismatches ---\n", errors);
        printf("Check 'golden_output.txt' and 'hls_output.txt' for details.\n");
    }

    return (errors == 0) ? 0 : 1;
}