#include <stdio.h>   
#include <iostream>
#include <vector>
#include <cmath>

typedef float data_t;

// Το παλιό struct για την έξοδο (64-bit)
struct float2 {
    data_t f0;
    data_t f1;
};

// Το ΝΕΟ θηριώδες struct για την είσοδο (512-bit)
struct float16 {
    data_t data[16];
};

const int ORIG_ROWS = 16;
const int ORIG_COLS = 1024;
const int ORIG_TOTAL = ORIG_ROWS * ORIG_COLS; // 16384
const int TOTAL_VECTORS = ORIG_TOTAL / 2;     // 8192

// Πόσα πακέτα των 512-bit θα στείλουμε συνολικά;
// 16384 πίξελ / 16 πίξελ ανά πακέτο = 1024 πακέτα!
const int BURSTS_512BIT = ORIG_TOTAL / 16; 

const int KERNEL_ROWS = ORIG_ROWS - 2; 
const int KERNEL_COLS = ORIG_COLS - 2; 
const int KERNEL_ITERATIONS = KERNEL_ROWS * KERNEL_COLS; // 14308

// Η νέα δήλωση του Hardware (Η είσοδος είναι πλέον float16)
void architecture_top_level(float16* A_in_mem, float2* B_out_mem);

// ==========================================
// GOLDEN MODEL (C++ Software Reference - ΑΜΕΤΑΒΛΗΤΟ!)
// ==========================================
void compute_golden(std::vector<data_t>& A_vec, std::vector<data_t>& B_golden_vec) {
    printf("  [Golden] Starting golden computation...\n");
    B_golden_vec.clear();

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
    printf("[TB] Starting 512-bit Gearbox SODA Testbench...\n");

    std::vector<data_t> A_flat_vector(ORIG_TOTAL);
    std::vector<data_t> B_golden_vector;
    
    for (int i = 0; i < ORIG_TOTAL; i++) {
        A_flat_vector[i] = (data_t)(i % 256) / 10.0f;
    }

    compute_golden(A_flat_vector, B_golden_vector);

    // 2. Προετοιμασία μνήμης για το Hardware
    // Προσοχή: Δεσμεύουμε 1024 πακέτα float16!
    float16* A_in_hw = new float16[BURSTS_512BIT];
    float2* B_out_hw = new float2[KERNEL_ITERATIONS / 2];

    // --- ΤΟ ΝΕΟ DATA PACKING ΣΤΟ TESTBENCH ---
    printf("[TB] Packing 16384 floats into 1024 512-bit bursts...\n");
    for (int i = 0; i < BURSTS_512BIT; i++) {
        for (int j = 0; j < 16; j++) {
            A_in_hw[i].data[j] = A_flat_vector[i * 16 + j];
        }
    }

    for (int i = 0; i < KERNEL_ITERATIONS / 2; i++) {
        B_out_hw[i].f0 = 0.0f; B_out_hw[i].f1 = 0.0f;
    }

    // 3. Εκτέλεση του Hardware
    printf("[TB] Running Hardware Top Level...\n");
    architecture_top_level(A_in_hw, B_out_hw);

    // 4. Επαλήθευση (ΑΜΕΤΑΒΛΗΤΗ)
    printf("[TB] Verifying results...\n");
    int errors = 0;

    for (int i = 0; i < KERNEL_ITERATIONS; i++) {
        data_t hls_result;
        if (i % 2 == 0) {
            hls_result = B_out_hw[i / 2].f0;
        } else {
            hls_result = B_out_hw[i / 2].f1;
        }
        
        data_t golden_result = B_golden_vector[i];

        if (std::abs(hls_result - golden_result) > 0.001f) {
            errors++;
            if (errors <= 5) {
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