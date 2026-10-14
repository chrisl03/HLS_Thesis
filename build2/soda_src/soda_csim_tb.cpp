//  soda_csim_tb.cpp  (2-SLR, K=64 8R+8W paired) - HLS csim/cosim of ONE kernel
//  Verifies architecture_top_a on a single 513x1024 block. Kernel B is identical
//  (only the name differs), so one HLS run covers both. The split/seam is a
//  system-level property tested by the XRT host (tb_soda.cpp), not here.
#include <stdio.h>
#include <vector>
#include <cmath>
#include "soda_common.h"

// top-level of kernel A (defined in soda_top_a.cpp)
void architecture_top_a(
    hls::burst_maxi<float16> A_in_mem_0, hls::burst_maxi<float16> A_in_mem_1,
    hls::burst_maxi<float16> A_in_mem_2, hls::burst_maxi<float16> A_in_mem_3,
    hls::burst_maxi<float16> A_in_mem_4, hls::burst_maxi<float16> A_in_mem_5,
    hls::burst_maxi<float16> A_in_mem_6, hls::burst_maxi<float16> A_in_mem_7,
    hls::burst_maxi<float16> B_out_mem_0, hls::burst_maxi<float16> B_out_mem_1,
    hls::burst_maxi<float16> B_out_mem_2, hls::burst_maxi<float16> B_out_mem_3,
    hls::burst_maxi<float16> B_out_mem_4, hls::burst_maxi<float16> B_out_mem_5,
    hls::burst_maxi<float16> B_out_mem_6, hls::burst_maxi<float16> B_out_mem_7);

inline data_t stencil_at(const std::vector<data_t>& A, int i, int j) {
    data_t a00=A[i*SODA_COLS+j], a10=A[(i+1)*SODA_COLS+j], a01=A[i*SODA_COLS+j+1];
    data_t a0m1=A[i*SODA_COLS+j-1], am10=A[(i-1)*SODA_COLS+j];
    data_t r0=a00-a0m1, r1=a00-a01, r2=a00-am10, r3=a00-a10;
    return r0*r0+r1*r1+r2*r2+r3*r3;
}

int main() {
    printf("[TB] SODA csim (K=64, 8+8 paired, 1 block %dx%d)...\n", SODA_ROWS, SODA_COLS);
    const int N = (BURSTS_PER_CH_EVEN > 0 ? BURSTS_PER_CH_EVEN : 1);  // per-channel size

    std::vector<data_t> A_flat(SODA_TOTAL_PIXELS);
    for (int i=0;i<SODA_TOTAL_PIXELS;i++) A_flat[i]=(data_t)(i%256)/10.0f;

    float16* A_in[NUM_READ_CH];
    float16* B_out[NUM_WRITE_CH];
    for (int c=0;c<NUM_READ_CH;c++)  A_in[c]  = new float16[N];
    for (int c=0;c<NUM_WRITE_CH;c++){ B_out[c] = new float16[N];
        for (int i=0;i<N;i++) for(int j=0;j<16;j++) B_out[c][i][j]=0.0f; }

    // --- paired packing: even vector -> channel 2g, odd -> 2g+1, burst = v/2 ---
    for (int v=0; v<TOTAL_VECTORS; v++) {
        int burst = v/2;
        for (int g=0; g<NUM_PAIRS; g++) {
            float16 tmp;
            for (int f=0; f<16; f++) tmp[f] = A_flat[v*SODA_K + g*16 + f];
            int chan = ((v & 1)==0) ? (2*g) : (2*g + 1);
            A_in[chan][burst] = tmp;
        }
    }

    hls::burst_maxi<float16> A0(A_in[0]),A1(A_in[1]),A2(A_in[2]),A3(A_in[3]);
    hls::burst_maxi<float16> A4(A_in[4]),A5(A_in[5]),A6(A_in[6]),A7(A_in[7]);
    hls::burst_maxi<float16> B0(B_out[0]),B1(B_out[1]),B2(B_out[2]),B3(B_out[3]);
    hls::burst_maxi<float16> B4(B_out[4]),B5(B_out[5]),B6(B_out[6]),B7(B_out[7]);

    printf("[TB] Running architecture_top_a...\n");
    architecture_top_a(A0,A1,A2,A3,A4,A5,A6,A7, B0,B1,B2,B3,B4,B5,B6,B7);

    // --- verify the block interior (local rows 1..ROWS-2, cols 1..COLS-2) ---
    printf("[TB] Verifying block interior (ignoring block borders)...\n");
    int errors=0, checked=0;
    for (int i=1;i<SODA_ROWS-1;i++){
        for (int j=1;j<SODA_COLS-1;j++){
            int idx=i*SODA_COLS+j;
            int v=idx/SODA_K, k=idx%SODA_K, g=k/16, f=k%16;
            int chan = ((v & 1)==0) ? (2*g) : (2*g + 1);
            data_t hls_r = B_out[chan][v/2][f];
            data_t golden= stencil_at(A_flat,i,j);
            checked++;
            if (std::abs(hls_r-golden)>0.001f){ errors++;
                if (errors<=5) printf("  [ERROR] (R%d,C%d) HLS=%f SW=%f\n", i, j, hls_r, golden); }
        }
    }
    printf("Checked %d interior pixels, mismatches: %d\n", checked, errors);

    for (int c=0;c<NUM_READ_CH;c++)  delete[] A_in[c];
    for (int c=0;c<NUM_WRITE_CH;c++) delete[] B_out[c];

    if (errors==0){ printf("\n=== TEST PASSED! 0 errors ===\n"); return 0; }
    printf("\n=== TEST FAILED! %d ===\n", errors); return 1;
}