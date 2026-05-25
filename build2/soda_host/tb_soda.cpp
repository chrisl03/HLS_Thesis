//  tb_soda.cpp

#include "host_soda_fpga.h"
#include <iostream>
#include <vector>
#include <cstdlib>
#include <cmath>

typedef float data_t;

// ==========================================
// GOLDEN MODEL (C++ Software Reference)\
// ==========================================
void compute_golden(const std::vector<data_t>& A_vec, std::vector<data_t>& B_golden_vec) {
    std::cout << "  [Golden] Starting golden computation (Discarding Borders)..." << std::endl;
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
    std::cout << "  [Golden] Finished. Produced " << B_golden_vec.size() << " valid outputs.\n";
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage:\n  " << argv[0] << " <xclbin_path> [device_index] [iterations]" << std::endl;
        return 1;
    }

    const std::string xclbin_path = argv[1];
    const unsigned device_index = (argc >= 3) ? (unsigned)std::stoul(argv[2]) : 0;
    const unsigned iterations   = (argc >= 4) ? (unsigned)std::stoul(argv[3]) : 1;

    std::vector<data_t> A_flat_vector(SODA_TOTAL_PIXELS);
    std::vector<data_t> B_golden_vector;

    // input innit
    for (int i = 0; i < SODA_TOTAL_PIXELS; i++) {
        A_flat_vector[i] = (data_t)(i % 256) / 10.0f;
    }

    compute_golden(A_flat_vector, B_golden_vector);

    //  FPGA innit
    FPGA_SODA fpga;
    if (fpga.fpga_init(xclbin_path, device_index) != 0) {
        std::cerr << "FPGA init failed." << std::endl;
        return 1;
    }

    // 3. Serialize inputs directly into XRT mapped BO memory
    float* A_in_hw = fpga.get_inA_ptr();
    std::cout << "[TB] Packing " << SODA_TOTAL_PIXELS << " floats into 512-bit bursts...\n";
    for (int i = 0; i < SODA_BURSTS_IN; i++) {
        for (int j = 0; j < 16; j++) {
            A_in_hw[i * 16 + j] = A_flat_vector[i * 16 + j];
        }
    }

    // Optional warmup
    fpga.warmup(1);

    // 4. Run Kernel
    for (unsigned i = 0; i < iterations; ++i) {
        fpga.run();
    }

    std::cout << "[TB] Verifying results...\n";
    const float* B_out_hw = fpga.get_outB_ptr();
    
    bool pass = true;
    int errors = 0;
    const float tol = 0.001f;

    for (int i = 0; i < SODA_KERNEL_ITER; i++) {
        int pack_idx = i / SODA_K;
        int elem_idx = i % SODA_K;
        
        float hls_result = B_out_hw[pack_idx * SODA_K + elem_idx];
        float golden_result = B_golden_vector[i];

        if (std::abs(hls_result - golden_result) > tol) {
            pass = false;
            errors++;
            if (errors <= 5) {
                int row = (i / SODA_KERNEL_COLS) + 1;
                int col = (i % SODA_KERNEL_COLS) + 1;
                std::cout << "  [ERROR] Mismatch at index " << i 
                          << " (Row: " << row << ", Col: " << col 
                          << ") -> HLS: " << hls_result 
                          << ", SW: " << golden_result << std::endl;
            }
        }
    }

    std::cout << "Total mismatches: " << errors << std::endl;

    fpga.print_performance_timings();
    fpga.save_results_to_csv("benchmark_soda.csv");

    if (!pass) {
        std::cerr << "=======================================\n";
        std::cerr << "  TEST FAILED! " << errors << " mismatches found.\n";
        std::cerr << "=======================================\n";
        return 1;
    }

    std::cout << "=======================================\n";
    std::cout << "  TEST PASSED! 0 errors detected.\n";
    std::cout << "=======================================\n";
    return 0;
}