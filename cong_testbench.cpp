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
void architecture_top_level(hls::stream<data_t>& A_in,
                          hls::stream<data_t>& B_out);

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
    // Using C++ Standard Library (STL) vector
    std::vector<data_t> A_input_vector(TOTAL_ELEMENTS);
    std::vector<data_t> B_golden_vector;
    
    // Fill input vector with simple data (e.g., 0, 1, 2, ...)
    for (int i = 0; i < TOTAL_ELEMENTS; i++) {
        A_input_vector[i] = (data_t)(i % 256); // A simple "ramp"
    }

    // 2. Compute "Golden" Result
    compute_golden(A_input_vector, B_golden_vector);

    // 3. Create hls::stream and fill
    hls::stream<data_t> A_in_stream("A_in_stream");
    hls::stream<data_t> B_out_stream("B_out_stream");

    // Send all input data to the HLS kernel
    printf("[TB] Writing %d elements to HLS input stream...\n", TOTAL_ELEMENTS);
    for (int i = 0; i < TOTAL_ELEMENTS; i++) {
        A_in_stream.write(A_input_vector[i]);
    }

    // 4. Execute HLS Kernel (DUT)
    // Call your top-level function
    printf("[TB] Calling 'architecture_top_level' (HLS Kernel)...\n");
    architecture_top_level(A_in_stream, B_out_stream);
    printf("[TB] HLS Kernel execution finished.\n");

    // 5. Verify Results
    printf("[TB] Verifying results...\n");
    int errors = 0;
    for (int i = 0; i < KERNEL_ITERATIONS; i++) {
        // Read result from HLS
        data_t hls_result = B_out_stream.read();
        
        // Get the "golden" result
        data_t golden_result = B_golden_vector[i];

        // Compare (with tolerance because it is float)
        if (std::abs(hls_result - golden_result) > 0.001f) {
            errors++;
            printf("  [ERROR] Mismatch at index %d: HLS Result = %f, Golden Result = %f\n",
                   i, hls_result, golden_result);
        }
    }

    // 6. Final Report
    if (errors == 0) {
        printf("\n--- TEST PASSED ---\n");
        printf("All %d output values matched the golden reference.\n", KERNEL_ITERATIONS);
    } else {
        printf("\n--- TEST FAILED ---\n");
        printf("%d mismatches found.\n", errors);
    }

    // Return 0 means success in C-Simulation
    return (errors == 0) ? 0 : 1;
}