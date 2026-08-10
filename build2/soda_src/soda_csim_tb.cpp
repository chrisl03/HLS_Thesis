//  soda_csim_tb.cpp  (parametric K=16 / K=32, 2R+2W) - για Vitis HLS C-sim/cosim
#include <stdio.h>
#include <vector>
#include <cmath>
#include "soda_common.h"

void compute_golden(std::vector<data_t>& A, std::vector<data_t>& B) {
    printf("  [Golden] computing (discarding borders)...\n");
    B.clear();
    for (int i = 1; i < SODA_ROWS - 1; i++)
        for (int j = 1; j < SODA_COLS - 1; j++) {
            data_t a00=A[i*SODA_COLS+j], a10=A[(i+1)*SODA_COLS+j], a01=A[i*SODA_COLS+j+1];
            data_t a0m1=A[i*SODA_COLS+j-1], am10=A[(i-1)*SODA_COLS+j];
            data_t r0=a00-a0m1, r1=a00-a01, r2=a00-am10, r3=a00-a10;
            B.push_back(r0*r0+r1*r1+r2*r2+r3*r3);
        }
    printf("  [Golden] produced %zu outputs.\n", B.size());
}

int main() {
    printf("[TB] SODA csim (K=%d, BURSTS_PER_VEC=%d)...\n", SODA_K, BURSTS_PER_VEC);

    std::vector<data_t> A_flat(SODA_TOTAL_PIXELS);
    std::vector<data_t> B_golden;
    for (int i = 0; i < SODA_TOTAL_PIXELS; i++) A_flat[i] = (data_t)(i % 256) / 10.0f;
    compute_golden(A_flat, B_golden);

    // buffer sizes (σε bursts/packets ανα channel)
    int bursts_0 = SODA_BURSTS_IN_0, bursts_1 = SODA_BURSTS_IN_1;
    int packets_0 = SODA_PACKETS_OUT_0, packets_1 = SODA_PACKETS_OUT_1;

    float16* A_in_0 = new float16[bursts_0 > 0 ? bursts_0 : 1];
    float16* A_in_1 = new float16[bursts_1 > 0 ? bursts_1 : 1];
    float_pack* B_out_0 = new float_pack[packets_0 > 0 ? packets_0 : 1];
    float_pack* B_out_1 = new float_pack[packets_1 > 0 ? packets_1 : 1];

    // --- PACKING (K<=16 εναλλαξ ανα BURST / K=32 split) ---
    if (SODA_K <= 16) {
        // ανα BURST (16 floats), εναλλαξ channel - δουλευει K=8 ΚΑΙ K=16
        const int total_bursts = SODA_TOTAL_PIXELS / 16;
        for (int b = 0; b < total_bursts; b++) {
            float16 tmp;
            for (int j = 0; j < 16; j++) tmp[j] = A_flat[b * 16 + j];
            if ((b & 1) == 0) A_in_0[b / 2] = tmp;
            else              A_in_1[b / 2] = tmp;
        }
    } else {
        for (int v = 0; v < TOTAL_VECTORS; v++) {
            float16 t0, t1;
            for (int j = 0; j < 16; j++) {
                t0[j] = A_flat[v * 32 + j];
                t1[j] = A_flat[v * 32 + 16 + j];
            }
            A_in_0[v] = t0;
            A_in_1[v] = t1;
        }
    }

    for (int i = 0; i < packets_0; i++) for (int j = 0; j < SODA_K; j++) B_out_0[i][j] = 0.0f;
    for (int i = 0; i < packets_1; i++) for (int j = 0; j < SODA_K; j++) B_out_1[i][j] = 0.0f;

    hls::burst_maxi<float16>    A0(A_in_0), A1(A_in_1);
    hls::burst_maxi<float_pack> B0(B_out_0), B1(B_out_1);

    printf("[TB] Running...\n");
    architecture_top_level(A0, A1, B0, B1);

    printf("[TB] Verifying...\n");
    int errors = 0;
    for (int i = 0; i < SODA_KERNEL_ITER; i++) {
        int pack_idx = i / SODA_K, elem_idx = i % SODA_K;
        float_pack pk = ((pack_idx & 1) == 0) ? B_out_0[pack_idx/2] : B_out_1[pack_idx/2];
        data_t hls_r = pk[elem_idx];
        if (std::abs(hls_r - B_golden[i]) > 0.001f) {
            errors++;
            if (errors <= 5) {
                int row = (i / SODA_KERNEL_COLS) + 1, col = (i % SODA_KERNEL_COLS) + 1;
                printf("  [ERROR] idx %d (R%d,C%d) HLS=%f SW=%f\n", i, row, col, hls_r, B_golden[i]);
            }
        }
    }

    delete[] A_in_0; delete[] A_in_1; delete[] B_out_0; delete[] B_out_1;

    if (errors == 0) { printf("\n=== TEST PASSED! 0 errors ===\n"); return 0; }
    printf("\n=== TEST FAILED! %d mismatches ===\n", errors); return 1;
}