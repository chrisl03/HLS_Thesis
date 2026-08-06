//  tb_soda.cpp  (parametric K=16 / K=32, 2R+2W)
//  Packing χειριζεται και τις 2 περιπτωσεις μεσω BURSTS_PER_VEC.

#include "host_soda_fpga.h"
#include "host_visible.h"
#include <iostream>
#include <vector>
#include <cstdlib>
#include <cmath>

typedef float data_t;

// ==========================================
// GOLDEN MODEL
// ==========================================
void compute_golden(const std::vector<data_t>& A_vec, std::vector<data_t>& B_golden_vec) {
    std::cout << "  [Golden] Starting golden computation (Discarding Borders)...\n";
    B_golden_vec.clear();
    for (int i = 1; i < SODA_ROWS - 1; i++) {
        for (int j = 1; j < SODA_COLS - 1; j++) {
            data_t a00  = A_vec[i * SODA_COLS + j];
            data_t a10  = A_vec[(i + 1) * SODA_COLS + j];
            data_t a01  = A_vec[i * SODA_COLS + (j + 1)];
            data_t a0m1 = A_vec[i * SODA_COLS + (j - 1)];
            data_t am10 = A_vec[(i - 1) * SODA_COLS + j];
            data_t r0 = a00 - a0m1, r1 = a00 - a01, r2 = a00 - am10, r3 = a00 - a10;
            B_golden_vec.push_back(r0*r0 + r1*r1 + r2*r2 + r3*r3);
        }
    }
    std::cout << "  [Golden] Finished. Produced " << B_golden_vec.size() << " valid outputs.\n";
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <xclbin> [device_idx] [iters]\n";
        return 1;
    }
    const std::string xclbin_path = argv[1];
    const unsigned device_index = (argc >= 3) ? (unsigned)std::stoul(argv[2]) : 0;
    const unsigned iterations   = (argc >= 4) ? (unsigned)std::stoul(argv[3]) : 1;

    std::vector<data_t> A_flat(SODA_TOTAL_PIXELS);
    std::vector<data_t> B_golden;
    for (int i = 0; i < SODA_TOTAL_PIXELS; i++)
        A_flat[i] = (data_t)(i % 256) / 10.0f;

    compute_golden(A_flat, B_golden);

    FPGA_SODA fpga;
    if (fpga.fpga_init(xclbin_path, device_index) != 0) {
        std::cerr << "FPGA init failed.\n";
        return 1;
    }

    float* A0 = fpga.get_inA0_ptr();
    float* A1 = fpga.get_inA1_ptr();

    // ------------------------------------------------------------------
    // PACKING - χειριζεται K=16 και K=32 μεσω BURSTS_PER_VEC.
    // Το load_input διαβαζει: για καθε vector i, BURSTS_PER_VEC bursts.
    //
    //  K=16 (BURSTS_PER_VEC=1): vector i = 1 burst, εναλλαξ channel (i%2).
    //       -> burst i πηγαινει channel (i%2), θεση i/2.
    //
    //  K=32 (BURSTS_PER_VEC=2): vector i = 2 bursts (b0,b1) παραλληλα.
    //       -> channel0[i] = πρωτο μισο (floats 0..15) του vector i
    //          channel1[i] = δευτερο μισο (floats 16..31) του vector i
    // ------------------------------------------------------------------
    std::cout << "[TB] Packing (K=" << SODA_K << ", BURSTS_PER_VEC="
              << BURSTS_PER_VEC << ")...\n";

    for (int v = 0; v < TOTAL_VECTORS; v++) {
        if (BURSTS_PER_VEC == 1) {
            // K=16: ενα burst (16 floats) = ενα vector, εναλλαξ channel
            float* dst = ((v & 1) == 0) ? A0 : A1;
            int pos = v / 2;
            for (int j = 0; j < 16; j++)
                dst[pos * 16 + j] = A_flat[v * 16 + j];
        } else {
            // K=32: vector v = floats [v*32 .. v*32+31]
            //   channel0[v] = floats 0..15,  channel1[v] = floats 16..31
            for (int j = 0; j < 16; j++) {
                A0[v * 16 + j] = A_flat[v * 32 + j];
                A1[v * 16 + j] = A_flat[v * 32 + 16 + j];
            }
        }
    }

    fpga.warmup(1);
    for (unsigned it = 0; it < iterations; ++it) fpga.run();

    std::cout << "[TB] Verifying...\n";
    const float* B0 = fpga.get_outB0_ptr();
    const float* B1 = fpga.get_outB1_ptr();

    int errors = 0;
    const float tol = 0.001f;
    for (int i = 0; i < SODA_KERNEL_ITER; i++) {
        int pack_idx = i / SODA_K;
        int elem_idx = i % SODA_K;
        // output packs interleaved: pack pack_idx -> channel (pack_idx%2)
        const float* src = ((pack_idx & 1) == 0) ? B0 : B1;
        int pack_pos = pack_idx / 2;
        float hls_result = src[pack_pos * SODA_K + elem_idx];
        float golden = B_golden[i];
        if (std::abs(hls_result - golden) > tol) {
            errors++;
            if (errors <= 5) {
                int row = (i / SODA_KERNEL_COLS) + 1;
                int col = (i % SODA_KERNEL_COLS) + 1;
                std::cout << "  [ERROR] idx " << i << " (R" << row << ",C" << col
                          << ") HLS=" << hls_result << " SW=" << golden << "\n";
            }
        }
    }

    std::cout << "Total mismatches: " << errors << "\n";
    fpga.print_performance_timings();
    fpga.save_results_to_csv("benchmark_soda.csv");

    if (errors) {
        std::cerr << "=== TEST FAILED! " << errors << " mismatches ===\n";
        return 1;
    }
    std::cout << "=== TEST PASSED! 0 errors ===\n";
    return 0;
}