//  tb_soda.cpp  — host testbench, updated for the COALESCED full-grid output layout.
//  Golden is full-grid; the host output pointer is read flat (pixel idx == float index).
#include "host_soda_fpga.h"
#include <iostream>
#include <vector>
#include <cstdlib>
#include <cmath>

typedef float data_t;

// ==========================================
// GOLDEN MODEL — FULL GRID (border = 0, inner in-place)
// ==========================================
void compute_golden_full(const std::vector<data_t>& A, std::vector<data_t>& B) {
    std::cout << "  [Golden] Full-grid reference (border = 0, inner in-place)...\n";
    B.assign(SODA_TOTAL_PIXELS, 0.0f);

    for (int i = 1; i < SODA_ROWS - 1; i++) {
        for (int j = 1; j < SODA_COLS - 1; j++) {
            data_t a00  = A[i * SODA_COLS + j];
            data_t a10  = A[(i + 1) * SODA_COLS + j];
            data_t a01  = A[i * SODA_COLS + (j + 1)];
            data_t a0m1 = A[i * SODA_COLS + (j - 1)];
            data_t am10 = A[(i - 1) * SODA_COLS + j];

            data_t r0 = a00 - a0m1;
            data_t r1 = a00 - a01;
            data_t r2 = a00 - am10;
            data_t r3 = a00 - a10;
            B[i * SODA_COLS + j] = (r0 * r0) + (r1 * r1) + (r2 * r2) + (r3 * r3);
        }
    }
    std::cout << "  [Golden] Done.\n";
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

    for (int i = 0; i < SODA_TOTAL_PIXELS; i++)
        A_flat_vector[i] = (data_t)(((unsigned)i * 48271u) % 251) / 10.0f;

    compute_golden_full(A_flat_vector, B_golden_vector);

    FPGA_SODA fpga;
    if (fpga.fpga_init(xclbin_path, device_index) != 0) {
        std::cerr << "FPGA init failed." << std::endl;
        return 1;
    }

    // serialize input into the XRT mapped buffer (full grid)
    float* A_in_hw = fpga.get_inA_ptr();
    std::cout << "[TB] Packing " << SODA_TOTAL_PIXELS << " floats...\n";
    for (int i = 0; i < SODA_TOTAL_PIXELS; i++)
        A_in_hw[i] = A_flat_vector[i];

    fpga.warmup(1);

    for (unsigned i = 0; i < iterations; ++i)
        fpga.run();

    std::cout << "[TB] Verifying results (full grid, in-place)...\n";
    const float* B_out_hw = fpga.get_outB_ptr();

    bool pass = true;
    int errors = 0;
    const float tol = 0.001f;


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