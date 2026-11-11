//  soda_csim_tb.cpp  — COALESCED full-grid, for the m_axi (pointer) top signature.
//  Only change vs before: the top is now architecture_top_level(float16*, float_pack*),
//  so we pass the array pointers directly (no hls::burst_maxi wrappers).
#include <stdio.h>
#include <iostream>
#include <vector>
#include <cmath>

#include "soda_common.h"

// ==========================================
// GOLDEN MODEL — FULL GRID (same layout as the kernel)
// ==========================================
void compute_golden_full(const std::vector<data_t>& A, std::vector<data_t>& B) {
    printf("  [Golden] Full-grid reference (border = 0, inner in-place)...\n");
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
    printf("  [Golden] Done.\n");
}

int main() {
    printf("[TB] Coalesced SODA csim (K=%d), FULL-GRID output, m_axi top...\n", SODA_K);

    std::vector<data_t> A_flat(SODA_TOTAL_PIXELS);
    std::vector<data_t> B_golden;

    for (int i = 0; i < SODA_TOTAL_PIXELS; i++)
        A_flat[i] = (data_t)(i % 256) / 10.0f;

    compute_golden_full(A_flat, B_golden);

    float16*    A_in_hw  = new float16[SODA_BURSTS_IN];
    float_pack* B_out_hw = new float_pack[SODA_TOTAL_PACKETS_OUT];

    for (int i = 0; i < SODA_BURSTS_IN; i++)
        for (int j = 0; j < 16; j++)
            A_in_hw[i][j] = A_flat[i * 16 + j];

    for (int i = 0; i < SODA_TOTAL_PACKETS_OUT; i++)
        for (int j = 0; j < SODA_K; j++)
            B_out_hw[i][j] = 0.0f;

    printf("[TB] Running Hardware Top Level...\n");
    architecture_top_level(A_in_hw, B_out_hw);   // <-- plain pointers (m_axi), no burst_maxi

    printf("[TB] Verifying (full grid, in-place)...\n");
    int errors = 0;
    for (int idx = 0; idx < SODA_TOTAL_PIXELS; idx++) {
        int word = idx / 16;
        int lane = idx % 16;
        data_t hls_result    = B_out_hw[word][lane];
        data_t golden_result = B_golden[idx];

        if (std::abs(hls_result - golden_result) > 0.001f) {
            errors++;
            if (errors <= 5) {
                int row = idx / SODA_COLS;
                int col = idx % SODA_COLS;
                printf("  [ERROR] (row %d, col %d) -> HLS: %f, SW: %f\n",
                       row, col, hls_result, golden_result);
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
    }
    printf("\n=======================================\n");
    printf("  TEST FAILED! %d mismatches found.    \n", errors);
    printf("=======================================\n");
    return 1;
}