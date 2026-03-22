#include <stdio.h>   
#include <iostream>
#include <vector>
#include <cmath>
#include "hls_stream.h" 

typedef float data_t;

const int ROWS = 16;
const int COLUMNS = 1024;
const int TOTAL_ELEMENTS = ROWS * COLUMNS;

// ΔΗΛΩΣΗ ΤΟΥ HARDWARE (Απλά μια γραμμή, χωρίς τον κώδικά του!)
void architecture_top_level(hls::stream<data_t> &A_in, hls::stream<data_t> &B_out);

// ========================================================
// 2. SOFTWARE GOLDEN MODEL
// ========================================================

void compute_golden(std::vector<data_t>& A_vec, std::vector<data_t>& B_golden_vec) {
    printf("  [Golden] Starting golden computation...\n");
    B_golden_vec.clear();

    for (int i = 1; i < ROWS - 1; i++) {
        for (int j = 1; j < COLUMNS - 1; j++) {
            
            data_t a10  = A_vec[(i + 1) * COLUMNS + j]; 
            data_t a01  = A_vec[i * COLUMNS + (j + 1)]; 
            data_t a00  = A_vec[i * COLUMNS + j];       
            data_t a0m1 = A_vec[i * COLUMNS + (j - 1)]; 
            data_t am10 = A_vec[(i - 1) * COLUMNS + j]; 

            data_t res_0 = a00 - a0m1;
            data_t res_1 = a00 - a01;
            data_t res_2 = a00 - am10;
            data_t res_3 = a00 - a10;

            data_t b_val = (res_0 * res_0) + (res_1 * res_1) +
                           (res_2 * res_2) + (res_3 * res_3);

            B_golden_vec.push_back(b_val);
        }
    }
    printf("  [Golden] Finished. Produced %zu valid outputs.\n", B_golden_vec.size());
}

// ========================================================
// 3. TESTBENCH (MAIN)
// ========================================================

int main() {
    printf("[TB] Starting Testbench...\n");

    std::vector<data_t> A_input_vector(TOTAL_ELEMENTS);
    std::vector<data_t> B_golden_vector;
    
    // Γέμισμα με δεκαδικά δεδομένα
    for (int i = 0; i < TOTAL_ELEMENTS; i++) {
        A_input_vector[i] = (data_t)(i % 256) / 10.0f;
    }

    compute_golden(A_input_vector, B_golden_vector);

    hls::stream<data_t> A_in_stream("A_in_stream");
    hls::stream<data_t> B_out_stream("B_out_stream");

    printf("[TB] Writing %d elements to HLS...\n", TOTAL_ELEMENTS);
    for (int i = 0; i < TOTAL_ELEMENTS; i++) {
        A_in_stream.write(A_input_vector[i]);
    }

    printf("[TB] Running HLS Kernel...\n");
    architecture_top_level(A_in_stream, B_out_stream);

    printf("[TB] Verifying results...\n");
    
    // 1. Διαβάζουμε όλο το αποτέλεσμα του Hardware σε έναν πίνακα
    std::vector<data_t> B_hw_vector(TOTAL_ELEMENTS);
    for (int i = 0; i < TOTAL_ELEMENTS; i++) {
        B_hw_vector[i] = B_out_stream.read();
    }

    int errors = 0;
    int golden_index = 0;

    // 2. Έλεγχος στα εσωτερικά πίξελ
    for (int i = 1; i < ROWS - 1; i++) {
        for (int j = 1; j < COLUMNS - 1; j++) {
            
            // Το Hardware βγάζει το αποτέλεσμα 1024 κύκλους (1 ολόκληρη γραμμή) καθυστερημένα!
            int center_index = i * COLUMNS + j;
            data_t hls_result = B_hw_vector[center_index + 1024]; 
            
            data_t golden_result = B_golden_vector[golden_index];
            golden_index++;

            if (std::abs(hls_result - golden_result) > 0.001f) {
                errors++;
                if (errors <= 5) {
                    printf("  [ERROR] Mismatch at (Row: %d, Col: %d) -> HLS: %f, SW: %f\n",
                           i, j, hls_result, golden_result);
                }
            }
        }
    }

    if (errors == 0) {
        printf("\n=======================================\n");
        printf("  TEST PASSED! 0 errors detected.      \n");
        printf("=======================================\n");
        return 0;
    } else {
        printf("\n=======================================\n");
        printf("  TEST FAILED! %d mismatches found.    \n", errors);
        printf("=======================================\n");
        return 1;
    }
}