//  tb_soda.cpp  (2-SLR split, K=64 8R+8W paired per kernel)
//
//  Full image 1024x1024 split in rows into two overlapping blocks (halo = 1 row):
//      Kernel A  <- image rows [0   .. 512]   (513 rows) -> valid out rows 1..511
//      Kernel B  <- image rows [511 .. 1023]  (513 rows) -> valid out rows 512..1022
#include "host_soda_fpga.h"
#include "host_visible.h"
#include <iostream>
#include <vector>
#include <cmath>

typedef float data_t;

// 5-point denoise2d golden on the FULL image
inline data_t stencil_at(const std::vector<data_t>& A, int i, int j) {
    data_t a00=A[i*SODA_COLS+j], a10=A[(i+1)*SODA_COLS+j], a01=A[i*SODA_COLS+j+1];
    data_t a0m1=A[i*SODA_COLS+j-1], am10=A[(i-1)*SODA_COLS+j];
    data_t r0=a00-a0m1, r1=a00-a01, r2=a00-am10, r3=a00-a10;
    return r0*r0+r1*r1+r2*r2+r3*r3;
}

// --- paired interleave address of pixel p (0..SODA_TOTAL_PIXELS-1 in a block) ---
//   vector v = p/64 ; lane k = p%64 ; pair group g = k/16 ; float f = k%16
//   even vector -> channel 2g , odd vector -> channel 2g+1 , position (v/2)*16+f
static inline void paired_addr(long p, int& chan, long& pos) {
    long v = p / SODA_K;
    int  k = (int)(p % SODA_K);
    int  g = k / 16;
    int  f = k % 16;
    chan = ((v & 1) == 0) ? (2*g) : (2*g + 1);
    pos  = (v / 2) * 16 + f;
}

// pack a 513x1024 block (flat, row-major) into the 8 read-channel buffers
static void pack_block(const data_t* block, float* chan[NUM_READ_CH]) {
    for (long p = 0; p < SODA_TOTAL_PIXELS; p++) {
        int c; long pos; paired_addr(p, c, pos);
        chan[c][pos] = block[p];
    }
}

// unpack the 8 write-channel buffers back into a 513x1024 block (flat)
static void unpack_block(const float* const chan[NUM_WRITE_CH], data_t* block) {
    for (long p = 0; p < SODA_TOTAL_PIXELS; p++) {
        int c; long pos; paired_addr(p, c, pos);
        block[p] = chan[c][pos];
    }
}

int main(int argc, char** argv) {
    if (argc < 2) { std::cerr << "Usage: " << argv[0] << " <xclbin> [dev] [iters]\n"; return 1; }
    const std::string xclbin = argv[1];
    const unsigned dev   = (argc>=3)?std::stoul(argv[2]):0;
    const unsigned iters = (argc>=4)?std::stoul(argv[3]):1;

    const int FULL = SODA_ROWS_FULL;           // 1024
    const int COLS = SODA_COLS;                // 1024
    const long BLOCK = (long)SODA_ROWS * COLS; // 513*1024

    // --- full input image ---
    std::vector<data_t> A_full((long)FULL*COLS);
    for (long i=0;i<(long)FULL*COLS;i++) A_full[i]=(data_t)(i%256)/10.0f;

    // --- build the two overlapping blocks (halo) ---
    std::vector<data_t> blkA(BLOCK), blkB(BLOCK);
    for (int lr=0; lr<SODA_ROWS; lr++) {       // local row 0..512
        int grA = A_ROW_START + lr;            // 0..512
        int grB = B_ROW_START + lr;            // 511..1023
        for (int c=0;c<COLS;c++) {
            blkA[(long)lr*COLS + c] = A_full[(long)grA*COLS + c];
            blkB[(long)lr*COLS + c] = A_full[(long)grB*COLS + c];
        }
    }

    FPGA_SODA fpga;
    if (fpga.fpga_init(xclbin, dev) != 0) { std::cerr << "init failed\n"; return 1; }

    // channel pointer arrays
    float* Ain[NUM_READ_CH];  float* Bin[NUM_READ_CH];
    const float* Aout[NUM_WRITE_CH]; const float* Bout[NUM_WRITE_CH];
    for (int c=0;c<NUM_READ_CH;c++){ Ain[c]=fpga.inA_ptr(0,c);  Bin[c]=fpga.inA_ptr(1,c); }
    for (int c=0;c<NUM_WRITE_CH;c++){ Aout[c]=fpga.outB_ptr(0,c); Bout[c]=fpga.outB_ptr(1,c); }

    std::cout << "[TB] Packing 2 blocks (K=64, paired, halo=1)...\n";
    pack_block(blkA.data(), Ain);
    pack_block(blkB.data(), Bin);

    fpga.warmup(1);
    for (unsigned it=0; it<iters; ++it) fpga.run();

    // --- unpack both block outputs ---
    std::vector<data_t> outA(BLOCK), outB(BLOCK);
    unpack_block(Aout, outA.data());
    unpack_block(Bout, outB.data());

    // --- reassemble full image from the VALID local rows of each block ---
    std::vector<data_t> R((long)FULL*COLS, 0.0f);
    for (int lr=VALID_LOCAL_FIRST; lr<=VALID_LOCAL_LAST; lr++) {   // 1..511
        int grA = A_ROW_START + lr;     // 1..511
        int grB = B_ROW_START + lr;     // 512..1022
        for (int c=0;c<COLS;c++) {
            R[(long)grA*COLS + c] = outA[(long)lr*COLS + c];
            R[(long)grB*COLS + c] = outB[(long)lr*COLS + c];
        }
    }

    // --- verify interior of the FULL image (borders ignored) ---
    std::cout << "[TB] Verifying full interior 1..1022 (ignoring borders)...\n";
    int errors=0, checked=0; const float tol=0.001f;
    for (int i=1;i<FULL-1;i++) {
        for (int j=1;j<COLS-1;j++) {
            float hls_r = R[(long)i*COLS + j];
            float golden= stencil_at(A_full,i,j);
            checked++;
            if (std::abs(hls_r-golden)>tol){ errors++;
                if (errors<=5) std::cout << "  [ERROR] (R"<<i<<",C"<<j<<") HLS="<<hls_r<<" SW="<<golden<<"\n"; }
        }
    }
    std::cout << "Checked " << checked << " interior pixels, mismatches: " << errors << "\n";
    fpga.print_performance_timings();
    fpga.save_results_to_csv("benchmark_soda_2slr.csv");
    if (errors){ std::cerr << "=== TEST FAILED! "<<errors<<" ===\n"; return 1; }
    std::cout << "=== TEST PASSED! 0 errors ===\n";
    return 0;
}