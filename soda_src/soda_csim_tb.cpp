//  soda_csim_tb.cpp
#include <stdio.h>   
#include <iostream>
#include <vector>
#include <cmath>

// Κάνουμε include ΟΛΗ την υποδομή του Hardware μας!
#include "soda_common.h"

// ==========================================
// GOLDEN MODEL (C++ Software Reference)
// ==========================================
void compute_golden(std::vector<data_t>& A_vec, std::vector<data_t>& B_golden_vec) {
    printf("  [Golden] Starting golden computation (Discarding Borders)...\n");
    B_golden_vec.clear();

    for (int i = 1; i < SODA_ROWS - 1; i++) {
        for (int j = 1; j < SODA_COLS - 1; j++) {
            data_t a00  = A_vec[i * SODA_COLS + j];       
            data_t a10  = A_vec[(i + 1) * SODA_COLS + j]; 
            data_t a01  = A_vec[i * SODA_COLS + (j + 1)]; 
            data_t a0m1 = A_vec[i * SODA_COLS + (j - 1)]; 
            data_t am10 = A_vec[(i - 1) * SODA_COLS + j]; 

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

// ==========================================
// MAIN TESTBENCH
// ==========================================   
int main() {
    
    printf("[TB] Starting Generic SODA Testbench (K=%d)...\n", SODA_K);

    std::vector<data_t> A_flat_vector(SODA_TOTAL_PIXELS);
    std::vector<data_t> B_golden_vector;
    
    // 1. Αρχικοποίηση εισόδου με test τιμές
    for (int i = 0; i < SODA_TOTAL_PIXELS; i++) {
        A_flat_vector[i] = (data_t)(i % 256) / 10.0f;
    }

    compute_golden(A_flat_vector, B_golden_vector);

    // 2. Δέσμευση Μνήμης (Hardware Buffers)
    // Χρησιμοποιούμε τα έτοιμα macros από το host_visible.h!
    float16* A_in_hw = new float16[SODA_BURSTS_IN];
    float_pack* B_out_hw = new float_pack[SODA_TOTAL_PACKETS_OUT];

    // --- GENERIC PACKING ΕΙΣΟΔΟΥ ---
    printf("[TB] Packing %d floats into %d 512-bit bursts...\n", SODA_TOTAL_PIXELS, SODA_BURSTS_IN);
    for (int i = 0; i < SODA_BURSTS_IN; i++) {
        for (int j = 0; j < 16; j++) {
            A_in_hw[i][j] = A_flat_vector[i * 16 + j];
        }
    }

    // Καθαρισμός εξόδου
    for (int i = 0; i < SODA_TOTAL_PACKETS_OUT; i++) {
        for(int j = 0; j < SODA_K; j++) {
            B_out_hw[i][j] = 0.0f;
        }
    }

    hls::burst_maxi<float16> A_in_maxi(A_in_hw); 
    hls::burst_maxi<float_pack> B_out_maxi(B_out_hw);

    // 3. Εκτέλεση του Hardware
    printf("[TB] Running Hardware Top Level...\n");
    architecture_top_level(A_in_maxi, B_out_maxi);

    // 4. Επαλήθευση (Generic Unpacking)
    printf("[TB] Verifying results...\n");
    int errors = 0;

    for (int i = 0; i < SODA_KERNEL_ITER; i++) {
        // Μαθηματικά για να βρούμε σε ποιο struct και σε ποια θέση είναι το πίξελ μας
        int pack_idx = i / SODA_K;
        int elem_idx = i % SODA_K;
        
        data_t hls_result = B_out_hw[pack_idx][elem_idx];
        data_t golden_result = B_golden_vector[i];

        if (std::abs(hls_result - golden_result) > 0.001f) {
            errors++;
            if (errors <= 5) { // Τυπώνουμε μόνο τα 5 πρώτα λάθη για να μη γεμίσει η οθόνη
                int row = (i / SODA_KERNEL_COLS) + 1;
                int col = (i % SODA_KERNEL_COLS) + 1;
                printf("  [ERROR] Mismatch at index %d (Row: %d, Col: %d) -> HLS: %f, SW: %f\n",
                       i, row, col, hls_result, golden_result);
            }
        }
    }

    delete[] A_in_hw;
    delete[] B_out_hw;

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