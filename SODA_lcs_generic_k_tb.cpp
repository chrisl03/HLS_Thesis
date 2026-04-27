#include <stdio.h>   
#include <iostream>
#include <vector>
#include <cmath>

// === ΠΡΕΠΕΙ ΝΑ ΕΙΝΑΙ ΙΔΙΟ ΜΕ ΤΟ K ΤΟΥ HARDWARE ===
const int K = 8; 

typedef float data_t;

// Το πακέτο εισόδου από τη μνήμη (AXI Burst Gearbox)
struct float16 {
    data_t data[16];
};

// Το Generic πακέτο εξόδου
struct float_pack {
    data_t data[K];
};

const int ROWS = 16;
const int COLUMNS = 1024;
const int TOTAL_PIXELS = ROWS * COLUMNS;      // 16384

// Τα bursts των 512-bit (16 floats) που θα στείλουμε
const int BURSTS_512BIT = TOTAL_PIXELS / 16;  // 1024

const int KERNEL_ROWS = ROWS - 2; 
const int KERNEL_COLS = COLUMNS - 2; 
const int KERNEL_ITERATIONS = KERNEL_ROWS * KERNEL_COLS; // 14308

// Δήλωση του Top Level Hardware
void architecture_top_level(float16* A_in_mem, float_pack* B_out_mem);

// ==========================================
// GOLDEN MODEL (C++ Software Reference)
// ==========================================
void compute_golden(std::vector<data_t>& A_vec, std::vector<data_t>& B_golden_vec) {
    printf("  [Golden] Starting golden computation (Discarding Borders)...\n");
    B_golden_vec.clear();

    for (int i = 1; i < ROWS - 1; i++) {
        for (int j = 1; j < COLUMNS - 1; j++) {
            data_t a00  = A_vec[i * COLUMNS + j];       
            data_t a10  = A_vec[(i + 1) * COLUMNS + j]; 
            data_t a01  = A_vec[i * COLUMNS + (j + 1)]; 
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

// ==========================================
// MAIN TESTBENCH
// ==========================================
int main() {
    
    printf("[TB] Starting Generic SODA Testbench (K=%d)...\n", K);

    std::vector<data_t> A_flat_vector(TOTAL_PIXELS);
    std::vector<data_t> B_golden_vector;
    
    // 1. Αρχικοποίηση εισόδου με test τιμές
    for (int i = 0; i < TOTAL_PIXELS; i++) {
        A_flat_vector[i] = (data_t)(i % 256) / 10.0f;
    }

    compute_golden(A_flat_vector, B_golden_vector);

    // 2. Δέσμευση Μνήμης (Hardware Buffers)
    float16* A_in_hw = new float16[BURSTS_512BIT];
    
    // Υπολογισμός των απαιτούμενων πακέτων εξόδου
    // Χρησιμοποιούμε Ceil Division (στρογγυλοποίηση προς τα πάνω) για ασφάλεια
    // Προσθέτουμε +16 θέσεις ασφαλείας για τα dummy AXI bursts
    int out_packs = (KERNEL_ITERATIONS + K - 1) / K + 16;
    float_pack* B_out_hw = new float_pack[out_packs];

    // --- GENERIC PACKING ΕΙΣΟΔΟΥ ---
    printf("[TB] Packing 16384 floats into 1024 512-bit bursts...\n");
    for (int i = 0; i < BURSTS_512BIT; i++) {
        for (int j = 0; j < 16; j++) {
            A_in_hw[i].data[j] = A_flat_vector[i * 16 + j];
        }
    }

    // Καθαρισμός εξόδου
    for (int i = 0; i < out_packs; i++) {
        for(int j = 0; j < K; j++) {
            B_out_hw[i].data[j] = 0.0f;
        }
    }

    // 3. Εκτέλεση του Hardware
    printf("[TB] Running Hardware Top Level...\n");
    architecture_top_level(A_in_hw, B_out_hw);

    // 4. Επαλήθευση (Generic Unpacking)
    printf("[TB] Verifying results...\n");
    int errors = 0;

    for (int i = 0; i < KERNEL_ITERATIONS; i++) {
        // Μαθηματικά για να βρούμε σε ποιο struct και σε ποια θέση είναι το πίξελ μας
        int pack_idx = i / K;
        int elem_idx = i % K;
        
        data_t hls_result = B_out_hw[pack_idx].data[elem_idx];
        data_t golden_result = B_golden_vector[i];

        if (std::abs(hls_result - golden_result) > 0.001f) {
            errors++;
            if (errors <= 5) { // Τυπώνουμε μόνο τα 5 πρώτα λάθη για να μη γεμίσει η οθόνη
                int row = (i / KERNEL_COLS) + 1;
                int col = (i % KERNEL_COLS) + 1;
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