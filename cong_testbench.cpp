#include <stdio.h>
#include <vector>
#include "ap_int.h"
#include "ap_fixed.h"
#include "hls_stream.h"
#include "hls_math.h"

typedef float data_t;
const int ROWS = 16; 
const int COLUMNS = 1024;
const int TOTAL_ELEMENTS = ROWS * COLUMNS;
const int KERNEL_ROWS = ROWS - 2;
const int KERNEL_COLUMNS = COLUMNS - 2;
const int KERNEL_ITERATIONS = KERNEL_ROWS * KERNEL_COLUMNS;

// Prototype of the top-level function
void architecture_top_level(data_t* A_in_mem, data_t* B_out_mem);

void compute_golden(std::vector<data_t>& A_vec, std::vector<data_t>& B_golden_vec) {

    printf("  [Golden] Starting golden computation...\n");
    // Clear the output vector to be sure
    B_golden_vec.clear();

    // Loops from Listing 1 [cite: 55-59]
    for (int i = 1; i < ROWS - 1; i++) {
        for (int j = 1; j < COLUMNS - 1; j++) {
            
            // Convert 2D index to 1D index
            data_t a10  = A_vec[(i + 1) * COLUMNS + j]; // A[i+1][j]
            data_t a01  = A_vec[i * COLUMNS + (j + 1)]; // A[i][j+1]
            data_t a00  = A_vec[i * COLUMNS + j];       // A[i][j]
            data_t a0m1 = A_vec[i * COLUMNS + (j - 1)]; // A[i][j-1]
            data_t am10 = A_vec[(i - 1) * COLUMNS + j]; // A[i-1][j]

            // Same operations as the kernel
            data_t res_0 = a00 - a0m1;
            data_t res_1 = a00 - a01;
            data_t res_2 = a00 - am10;
            data_t res_3 = a00 - a10;

            data_t b_val = (res_0 * res_0) + (res_1 * res_1) +
                           (res_2 * res_2) + (res_3 * res_3);

            // Add result to the list
            B_golden_vec.push_back(b_val);
        }
    }
    printf("  [Golden] Golden computation finished. Produced %zu outputs.\n", B_golden_vec.size());
}

int main() {
    printf("[TB] Starting Testbench...\n");

    // 1. Create Data
    std::vector<data_t> RAM_in(TOTAL_ELEMENTS);
    std::vector<data_t> RAM_out(KERNEL_ITERATIONS); // Output is smaller
    std::vector<data_t> Golden_out;
    
    // Fill input vector with simple data (e.g., 0, 1, 2, ...)
    printf("[TB] Initializing input memory...\n");
    for (int i = 0; i < TOTAL_ELEMENTS; i++) {
        RAM_in[i] = (data_t)(i % 256);
    }

    // 2. Compute "Golden" Result
    compute_golden(RAM_in, Golden_out);

    // 4. Run HLS Kernel

    printf("[TB] Calling 'architecture_top_level' with memory pointers...\n");
    
    architecture_top_level(RAM_in.data(), RAM_out.data());
    
    printf("[TB] Execution finished.\n");

    // 5. Verify Results
    printf("[TB] Verifying results...\n");
    int errors = 0;
    for (int i = 0; i < KERNEL_ITERATIONS; i++) {
        data_t hls_val = RAM_out[i]; // Read from output memory
        data_t ref_val = Golden_out[i];

        // Float comparison
        if (std::abs(hls_val - ref_val) > 0.001) {
            errors++;
            if (errors < 10) {
                printf("  [ERROR] i=%d HLS=%f Ref=%f\n", i, hls_val, ref_val);
            }
        }
    }

    if (errors == 0) {
        printf("\n--- TEST PASSED ---\n");
        printf("Output size matches: %d pixels\n", KERNEL_ITERATIONS);
    } else {
        printf("\n--- TEST FAILED: %d errors ---\n", errors);
    }

    return (errors == 0) ? 0 : 1;
}