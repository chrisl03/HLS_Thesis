//  soda_csim_tb.cpp  (K=64, 8R+8W paired, host ignores borders) - HLS csim/cosim
#include <stdio.h>
#include <vector>
#include <cmath>
#include "soda_common.h"

inline data_t stencil_at(const std::vector<data_t>& A, int i, int j) {
    data_t a00=A[i*SODA_COLS+j], a10=A[(i+1)*SODA_COLS+j], a01=A[i*SODA_COLS+j+1];
    data_t a0m1=A[i*SODA_COLS+j-1], am10=A[(i-1)*SODA_COLS+j];
    data_t r0=a00-a0m1, r1=a00-a01, r2=a00-am10, r3=a00-a10;
    return r0*r0+r1*r1+r2*r2+r3*r3;
}

int main() {
    printf("[TB] SODA csim (K=64, 8 channels paired)...\n");
    std::vector<data_t> A_flat(SODA_TOTAL_PIXELS);
    for (int i=0;i<SODA_TOTAL_PIXELS;i++) A_flat[i]=(data_t)(i%256)/10.0f;

    float16* A_in_0 = new float16[BURSTS_PER_CH_EVEN > 0 ? BURSTS_PER_CH_EVEN : 1];
    float16* A_in_1 = new float16[BURSTS_PER_CH_EVEN > 0 ? BURSTS_PER_CH_EVEN : 1];
    float16* A_in_2 = new float16[BURSTS_PER_CH_EVEN > 0 ? BURSTS_PER_CH_EVEN : 1];
    float16* A_in_3 = new float16[BURSTS_PER_CH_EVEN > 0 ? BURSTS_PER_CH_EVEN : 1];
    float16* A_in_4 = new float16[BURSTS_PER_CH_EVEN > 0 ? BURSTS_PER_CH_EVEN : 1];
    float16* A_in_5 = new float16[BURSTS_PER_CH_EVEN > 0 ? BURSTS_PER_CH_EVEN : 1];
    float16* A_in_6 = new float16[BURSTS_PER_CH_EVEN > 0 ? BURSTS_PER_CH_EVEN : 1];
    float16* A_in_7 = new float16[BURSTS_PER_CH_EVEN > 0 ? BURSTS_PER_CH_EVEN : 1];
    float16* B_out_0 = new float16[BURSTS_PER_CH_EVEN > 0 ? BURSTS_PER_CH_EVEN : 1];
    float16* B_out_1 = new float16[BURSTS_PER_CH_EVEN > 0 ? BURSTS_PER_CH_EVEN : 1];
    float16* B_out_2 = new float16[BURSTS_PER_CH_EVEN > 0 ? BURSTS_PER_CH_EVEN : 1];
    float16* B_out_3 = new float16[BURSTS_PER_CH_EVEN > 0 ? BURSTS_PER_CH_EVEN : 1];
    float16* B_out_4 = new float16[BURSTS_PER_CH_EVEN > 0 ? BURSTS_PER_CH_EVEN : 1];
    float16* B_out_5 = new float16[BURSTS_PER_CH_EVEN > 0 ? BURSTS_PER_CH_EVEN : 1];
    float16* B_out_6 = new float16[BURSTS_PER_CH_EVEN > 0 ? BURSTS_PER_CH_EVEN : 1];
    float16* B_out_7 = new float16[BURSTS_PER_CH_EVEN > 0 ? BURSTS_PER_CH_EVEN : 1];
    float16* A_in[8] = {A_in_0, A_in_1, A_in_2, A_in_3, A_in_4, A_in_5, A_in_6, A_in_7};
    float16* B_out[8] = {B_out_0, B_out_1, B_out_2, B_out_3, B_out_4, B_out_5, B_out_6, B_out_7};
    for (int i=0;i<BURSTS_PER_CH_EVEN;i++) for(int j=0;j<16;j++) B_out_0[i][j]=0.0f;
    for (int i=0;i<BURSTS_PER_CH_EVEN;i++) for(int j=0;j<16;j++) B_out_1[i][j]=0.0f;
    for (int i=0;i<BURSTS_PER_CH_EVEN;i++) for(int j=0;j<16;j++) B_out_2[i][j]=0.0f;
    for (int i=0;i<BURSTS_PER_CH_EVEN;i++) for(int j=0;j<16;j++) B_out_3[i][j]=0.0f;
    for (int i=0;i<BURSTS_PER_CH_EVEN;i++) for(int j=0;j<16;j++) B_out_4[i][j]=0.0f;
    for (int i=0;i<BURSTS_PER_CH_EVEN;i++) for(int j=0;j<16;j++) B_out_5[i][j]=0.0f;
    for (int i=0;i<BURSTS_PER_CH_EVEN;i++) for(int j=0;j<16;j++) B_out_6[i][j]=0.0f;
    for (int i=0;i<BURSTS_PER_CH_EVEN;i++) for(int j=0;j<16;j++) B_out_7[i][j]=0.0f;

    // PACKING (paired interleaved)
    for (int v=0; v<TOTAL_VECTORS; v++){
        int row=v/2;
        for (int p=0;p<NUM_PAIRS;p++){
            int c=2*p+(v&1);
            float16 tmp;
            for (int j=0;j<16;j++) tmp[j]=A_flat[v*64 + p*16 + j];
            A_in[c][row]=tmp;
        }
    }

    hls::burst_maxi<float16> A0(A_in_0);
    hls::burst_maxi<float16> A1(A_in_1);
    hls::burst_maxi<float16> A2(A_in_2);
    hls::burst_maxi<float16> A3(A_in_3);
    hls::burst_maxi<float16> A4(A_in_4);
    hls::burst_maxi<float16> A5(A_in_5);
    hls::burst_maxi<float16> A6(A_in_6);
    hls::burst_maxi<float16> A7(A_in_7);
    hls::burst_maxi<float16> B0(B_out_0);
    hls::burst_maxi<float16> B1(B_out_1);
    hls::burst_maxi<float16> B2(B_out_2);
    hls::burst_maxi<float16> B3(B_out_3);
    hls::burst_maxi<float16> B4(B_out_4);
    hls::burst_maxi<float16> B5(B_out_5);
    hls::burst_maxi<float16> B6(B_out_6);
    hls::burst_maxi<float16> B7(B_out_7);

    printf("[TB] Running...\n");
    architecture_top_level(A0, A1, A2, A3, A4, A5, A6, A7, B0, B1, B2, B3, B4, B5, B6, B7);

    printf("[TB] Verifying (ignoring borders)...\n");
    int errors=0, checked=0;
    for (int i=1;i<SODA_ROWS-1;i++){
        for (int j=1;j<SODA_COLS-1;j++){
            int idx=i*SODA_COLS+j;
            int v=idx/SODA_K, elem=idx%SODA_K, p=elem/16, pos=elem%16;
            int c=2*p+(v&1), row=v/2;
            data_t hls_r=B_out[c][row][pos];
            data_t golden=stencil_at(A_flat,i,j);
            checked++;
            if (std::abs(hls_r-golden)>0.001f){
                errors++;
                if (errors<=5) printf("  [ERROR] (R%d,C%d) HLS=%f SW=%f\n", i, j, hls_r, golden);
            }
        }
    }
    printf("Checked %d interior pixels, mismatches: %d\n", checked, errors);

    delete[] A_in_0; delete[] B_out_0;
    delete[] A_in_1; delete[] B_out_1;
    delete[] A_in_2; delete[] B_out_2;
    delete[] A_in_3; delete[] B_out_3;
    delete[] A_in_4; delete[] B_out_4;
    delete[] A_in_5; delete[] B_out_5;
    delete[] A_in_6; delete[] B_out_6;
    delete[] A_in_7; delete[] B_out_7;

    if (errors==0){ printf("\n=== TEST PASSED! 0 errors ===\n"); return 0; }
    printf("\n=== TEST FAILED! %d ===\n", errors); return 1;
}