//  tb_soda.cpp  (K=64, 8R+8W paired interleaved, host ignores borders)
#include "host_soda_fpga.h"
#include "host_visible.h"
#include <iostream>
#include <vector>
#include <cmath>

typedef float data_t;

inline data_t stencil_at(const std::vector<data_t>& A, int i, int j) {
    data_t a00=A[i*SODA_COLS+j], a10=A[(i+1)*SODA_COLS+j], a01=A[i*SODA_COLS+j+1];
    data_t a0m1=A[i*SODA_COLS+j-1], am10=A[(i-1)*SODA_COLS+j];
    data_t r0=a00-a0m1, r1=a00-a01, r2=a00-am10, r3=a00-a10;
    return r0*r0+r1*r1+r2*r2+r3*r3;
}

int main(int argc, char** argv) {
    if (argc < 2) { std::cerr << "Usage: " << argv[0] << " <xclbin> [dev] [iters]\n"; return 1; }
    const std::string xclbin = argv[1];
    const unsigned dev = (argc>=3)?std::stoul(argv[2]):0;
    const unsigned iters = (argc>=4)?std::stoul(argv[3]):1;

    std::vector<data_t> A_flat(SODA_TOTAL_PIXELS);
    for (int i=0;i<SODA_TOTAL_PIXELS;i++) A_flat[i]=(data_t)(i%256)/10.0f;

    FPGA_SODA fpga;
    if (fpga.fpga_init(xclbin, dev) != 0) { std::cerr << "init failed\n"; return 1; }

    float* A0 = fpga.get_inA0_ptr();
    float* A1 = fpga.get_inA1_ptr();
    float* A2 = fpga.get_inA2_ptr();
    float* A3 = fpga.get_inA3_ptr();
    float* A4 = fpga.get_inA4_ptr();
    float* A5 = fpga.get_inA5_ptr();
    float* A6 = fpga.get_inA6_ptr();
    float* A7 = fpga.get_inA7_ptr();
    float* A[8] = {A0, A1, A2, A3, A4, A5, A6, A7};
    const float* B0 = fpga.get_outB0_ptr();
    const float* B1 = fpga.get_outB1_ptr();
    const float* B2 = fpga.get_outB2_ptr();
    const float* B3 = fpga.get_outB3_ptr();
    const float* B4 = fpga.get_outB4_ptr();
    const float* B5 = fpga.get_outB5_ptr();
    const float* B6 = fpga.get_outB6_ptr();
    const float* B7 = fpga.get_outB7_ptr();
    const float* B[8] = {B0, B1, B2, B3, B4, B5, B6, B7};

    // PACKING (paired interleaved): vector v, pair p, κομματι = A_flat[v*64+p*16..+15]
    //   αν v αρτιο -> channel 2p, αλλιως 2p+1. Θεση = v/2 στο channel.
    std::cout << "[TB] Packing (K=64, 8 channels paired)...\n";
    for (int v = 0; v < TOTAL_VECTORS; v++) {
        int row = v / 2;
        for (int p = 0; p < NUM_PAIRS; p++) {
            int c = 2*p + (v & 1);
            for (int j = 0; j < 16; j++)
                A[c][row*16 + j] = A_flat[v*64 + p*16 + j];
        }
    }

    fpga.warmup(1);
    for (unsigned it=0; it<iters; ++it) fpga.run();

    std::cout << "[TB] Verifying (ignoring borders)...\n";
    int errors=0, checked=0;
    const float tol=0.001f;
    for (int i=1;i<SODA_ROWS-1;i++){
        for (int j=1;j<SODA_COLS-1;j++){
            int idx=i*SODA_COLS+j;
            int v=idx/SODA_K, elem=idx%SODA_K, p=elem/16, pos=elem%16;
            int c=2*p+(v&1), row=v/2;
            float hls_r=B[c][row*16+pos];
            float golden=stencil_at(A_flat,i,j);
            checked++;
            if (std::abs(hls_r-golden)>tol){
                errors++;
                if (errors<=5) std::cout << "  [ERROR] (R"<<i<<",C"<<j<<") HLS="<<hls_r<<" SW="<<golden<<"\n";
            }
        }
    }
    std::cout << "Checked " << checked << " interior pixels, mismatches: " << errors << "\n";
    fpga.print_performance_timings();
    fpga.save_results_to_csv("benchmark_soda.csv");
    if (errors){ std::cerr << "=== TEST FAILED! "<<errors<<" ===\n"; return 1; }
    std::cout << "=== TEST PASSED! 0 errors ===\n";
    return 0;
}