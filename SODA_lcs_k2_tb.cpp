#include <stdio.h>   
#include <iostream>
#include <vector>
#include <cmath>

typedef float data_t;

// Το ίδιο struct που ορίσαμε στο Hardware
struct float2 {
    data_t f0;
    data_t f1;
};

const int ORIG_ROWS = 16;
const int ORIG_COLS = 1024;
const int ORIG_TOTAL = ORIG_ROWS * ORIG_COLS; // 16384
const int TOTAL_VECTORS = ORIG_TOTAL / 2;     // 8192

const int KERNEL_ROWS = ORIG_ROWS - 2; 
const int KERNEL_COLS = ORIG_COLS - 2; 
const int KERNEL_ITERATIONS = KERNEL_ROWS * KERNEL_COLS; // 14308

// Δήλωση της συνάρτησης του Hardware
void architecture_top_level(float2* A_in_mem, data_t* B_out_mem);

// ==========================================
// GOLDEN MODEL (C++ Software Reference)
// ==========================================
void compute_golden(std::vector<data_t>& A_vec, std::vector<data_t>& B_golden_vec) {
    printf("  [Golden] Starting golden computation...\n");
    B_golden_vec.clear();

    // Υπολογίζουμε ΜΟΝΟ τον καθαρό εσωτερικό πυρήνα (χωρίς τα borders)
    for (int i = 1; i < ORIG_ROWS - 1; i++) {
        for (int j = 1; j < ORIG_COLS - 1; j++) {
            
            data_t a00  = A_vec[i * ORIG_COLS + j];       
            data_t a10  = A_vec[(i + 1) * ORIG_COLS + j]; 
            data_t a01  = A_vec[i * ORIG_COLS + (j + 1)]; 
            data_t a0m1 = A_vec[i * ORIG_COLS + (j - 1)]; 
            data_t am10 = A_vec[(i - 1) * ORIG_COLS + j]; 

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
    printf("[TB] Starting LCS SODA Testbench for k=2...\n");

    // 1. Δημιουργία των 1D πινάκων Software
    std::vector<data_t> A_flat_vector(ORIG_TOTAL);
    std::vector<data_t> B_golden_vector;
    
    // Γέμισμα με τυχαίες τιμές
    for (int i = 0; i < ORIG_TOTAL; i++) {
        A_flat_vector[i] = (data_t)(i % 256) / 10.0f;
    }

    // Τρέχουμε το Software Golden Model
    compute_golden(A_flat_vector, B_golden_vector);

    // 2. Προετοιμασία μνήμης για το Hardware (AXI Simulation)
    // Δεσμεύουμε μνήμη τύπου float2 για την είσοδο και data_t για την έξοδο
    float2* A_in_hw = new float2[TOTAL_VECTORS];
    data_t* B_out_hw = new data_t[KERNEL_ITERATIONS];

    // Συσκευασία (Packing) 2 floats σε 1 struct float2
    printf("[TB] Packing 16384 floats into 8192 float2 structs...\n");
    for (int i = 0; i < TOTAL_VECTORS; i++) {
        A_in_hw[i].f0 = A_flat_vector[i * 2];       // Άρτιο πίξελ
        A_in_hw[i].f1 = A_flat_vector[i * 2 + 1];   // Περιττό πίξελ
    }

    // Αρχικοποίηση της μνήμης εξόδου με μηδενικά για σιγουριά
    for (int i = 0; i < KERNEL_ITERATIONS; i++) {
        B_out_hw[i] = 0.0f;
    }

    // 3. Εκτέλεση του Hardware
    printf("[TB] Running Hardware Top Level...\n");
    architecture_top_level(A_in_hw, B_out_hw);

    // 4. Επαλήθευση (Verification)
    printf("[TB] Verifying results...\n");
    
    int errors = 0;

    // Πρόσεξε πόσο απλή είναι η `for` τώρα! Μόνο 0 έως 14307.
    for (int i = 0; i < KERNEL_ITERATIONS; i++) {
        data_t hls_result = B_out_hw[i]; 
        data_t golden_result = B_golden_vector[i];

        if (std::abs(hls_result - golden_result) > 0.001f) {
            errors++;
            if (errors <= 5) {
                // Υπολογισμός γραμμής/στήλης μόνο για το print (βοηθάει στο debugging)
                int row = (i / KERNEL_COLS) + 1;
                int col = (i % KERNEL_COLS) + 1;
                printf("  [ERROR] Mismatch at index %d (Row: %d, Col: %d) -> HLS: %f, SW: %f\n",
                       i, row, col, hls_result, golden_result);
            }
        }
    }

    // Απελευθέρωση μνήμης
    delete[] A_in_hw;
    delete[] B_out_hw;

    // 5. Αποτελέσματα
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