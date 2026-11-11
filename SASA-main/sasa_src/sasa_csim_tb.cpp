// sasa_csim_tb.cpp — C-sim / co-sim testbench for the SASA DENOISE2D single-PE (unikernel).
//
// Purpose: prove the dataflow runs end-to-end WITHOUT deadlock (csim) and give the cycle count
// for GFLOP/s (cosim). It fills `source` with a deterministic ramp, runs the kernel once
// (ITERATION), and sanity-checks that every produced lane is finite and the output is not all-zero.
//
// NOTE: this does NOT compare against a CPU golden stencil — replicating the exact SASA padding
// (TOP_APPEND/BOTTOM_APPEND halo) + 512-bit packing would be a second implementation and risks
// false mismatches. For the SODA-vs-SASA comparison the metric is throughput (cosim latency at the
// same 250 MHz). Ask if you want a full numeric golden added.
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <vector>
#include "DENOISE2D.h"
#include "hls_burst_maxi.h"

void unikernel(hls::burst_maxi<INTERFACE_WIDTH> source_0,
               hls::burst_maxi<INTERFACE_WIDTH> product_0, uint32_t iters);

static inline float get_lane(const INTERFACE_WIDTH& w, int lane) {
  uint32_t b = (uint32_t)w.range(lane * 32 + 31, lane * 32);
  float f; std::memcpy(&f, &b, sizeof(f)); return f;
}
static inline void set_lane(INTERFACE_WIDTH& w, int lane, float f) {
  uint32_t b; std::memcpy(&b, &f, sizeof(b));
  w.range(lane * 32 + 31, lane * 32) = b;
}

int main() {
  const size_t N = (size_t)MIDDLE_REGION;
  printf("SASA DENOISE2D csim | grid %dx%d | MIDDLE_REGION=%zu words | ITERATION=%d | PARA_FACTOR=%d\n",
         GRID_ROWS, GRID_COLS, N, ITERATION, PARA_FACTOR);

  std::vector<INTERFACE_WIDTH> source(N), product(N);
  for (size_t i = 0; i < N; i++) {
    INTERFACE_WIDTH w = 0;
    for (int k = 0; k < PARA_FACTOR; k++)
      set_lane(w, k, (float)(((i * PARA_FACTOR + k) % 997) * 0.01f));
    source[i]  = w;
    product[i] = 0;
  }

  // burst_maxi is constructed from the host array pointer in csim (same as your SODA tb)
  unikernel(source.data(), product.data(), (uint32_t)ITERATION);

  const size_t N_out = (size_t)(GRID_COLS / WIDTH_FACTOR) * PART_ROWS;  // words DENOISE2D emits
  size_t nonzero = 0; bool finite_ok = true;
  for (size_t i = 0; i < N_out; i++)
    for (int k = 0; k < PARA_FACTOR; k++) {
      float v = get_lane(product[i], k);
      if (!std::isfinite(v)) finite_ok = false;
      if (v != 0.0f) nonzero++;
    }

  printf("checked %zu output words | nonzero lanes = %zu | all finite = %s\n",
         N_out, nonzero, finite_ok ? "yes" : "NO");
  for (int i = 0; i < 3; i++)
    printf("  product[%d]: lane0=%g lane1=%g\n", i, get_lane(product[i], 0), get_lane(product[i], 1));

  if (!finite_ok || nonzero == 0) { printf("CSIM FAILED\n"); return 1; }
  printf("CSIM PASSED (ran to completion, no deadlock)\n");
  return 0;
}
