// unikernel.cpp — Vitis HLS port of the TAPA-generated SASA single-PE (DENOISE2D, 1024x1024)
//
// WHAT WAS CHANGED (and ONLY this — the compute is untouched):
//   * tapa::istream/ostream  -> hls::stream           (stream port types)
//   * s << x  /  s >> x       -> s.write(x) / x=s.read()   (standard hls::stream API)
//   * tapa::mmap/async_mmap  -> hls::burst_maxi        (HBM ports) + m_axi pragmas on the top
//   * tapa::task().invoke(..) -> a #pragma HLS DATAFLOW region of plain function calls
//
// FORCED, NON-OPTIONAL fixes (the generated code could not compile either way):
//   * the async_mmap `load` is split into read_source + write_product. A single read+write
//     loop DEADLOCKS in HLS: DENOISE2D consumes 129 words before its first output, so the
//     write side would block forever. TAPA hid this with async decoupling; HLS needs 2 procs.
//   * the undefined/duplicate generator symbols (inter_kernel, load_top/center/bot, the three
//     identical `load`s, inter_kernel_top/center/bot) are omitted — they are not used in the
//     COUNT=1 path and would not compile (undefined refs / redefinition).
//   * valid for ITERATION = 1 (the generated config). For iters>1 the inter-kernel ping-pong
//     would need to be added back.
#include <hls_stream.h>
#include "hls_burst_maxi.h"
#include "math.h"
#include "DENOISE2D.h"

static float DENOISE2D_stencil_kernel(float source_0_1, float source_0_0,
  float source_m1_0, float source_1_0, float source_0_m1)
{
  /*
    (source(0, 0) - source(0, -1)) * (source(0, 0) - source(0, -1)) + (source(0, 0) - source(0, 1)) * (source(0, 0) - source(0, 1)) + (source(0, 0) - source(-1, 0)) * (source(0, 0) - source(-1, 0)) + (source(0, 0) - source(1, 0)) * (source(0, 0) - source(1, 0))
  */
  return (source_0_0 - source_0_m1) * (source_0_0 - source_0_m1) + (source_0_0 - source_0_1) * (source_0_0 - source_0_1) + (source_0_0 - source_m1_0) * (source_0_0 - source_m1_0) + (source_0_0 - source_1_0) * (source_0_0 - source_1_0);
} // stencil kernel definition

// ======================= DENOISE2D : the single PE (compute) =======================
// Byte-for-byte the generated function; only the two stream ports and the << / >> operators
// were converted to the standard hls::stream API.
void DENOISE2D(hls::stream<INTERFACE_WIDTH>& source,
  hls::stream<INTERFACE_WIDTH>& product, int iters)
{
  INTERFACE_WIDTH source_block_m64;
  hls::stream<INTERFACE_WIDTH, 63> source_stream_m63_to_m2;
  INTERFACE_WIDTH source_block_m1;
  INTERFACE_WIDTH source_block_0;
  INTERFACE_WIDTH source_block_1;
  hls::stream<INTERFACE_WIDTH, 63> source_stream_2_to_63;
  INTERFACE_WIDTH source_block_64;

  for(int iter_idx = iters; iter_idx > 0; iter_idx--)
  {
    source_block_m64 = source.read();
    for (int i = 64 + -63; i < 64 + -1; i++) {
      source_stream_m63_to_m2.write(source.read());
    }
    source_block_m1 = source.read();
    source_block_0 = source.read();
    source_block_1 = source.read();
    for (int i = 64 + 2; i < 64 + 64; i++) {
      source_stream_2_to_63.write(source.read());
    }
    source_block_64 = source.read();

    float source_0_1[PARA_FACTOR], source_0_0[PARA_FACTOR], source_m1_0[PARA_FACTOR], source_1_0[PARA_FACTOR], source_0_m1[PARA_FACTOR];
    #pragma HLS array_partition variable=source_0_1 complete dim=0
    #pragma HLS array_partition variable=source_0_0 complete dim=0
    #pragma HLS array_partition variable=source_m1_0 complete dim=0
    #pragma HLS array_partition variable=source_1_0 complete dim=0
    #pragma HLS array_partition variable=source_0_m1 complete dim=0

    MAJOR_LOOP:
    for (int i = 0; i < GRID_COLS/WIDTH_FACTOR*PART_ROWS + 0; i++) {
      #pragma HLS pipeline II=1

      INTERFACE_WIDTH out_temp;
      COMPUTE_LOOP:
      for (int k = 0; k < PARA_FACTOR; k++) {
        #pragma HLS unroll
        float source_0_1[PARA_FACTOR], source_0_0[PARA_FACTOR], source_m1_0[PARA_FACTOR], source_1_0[PARA_FACTOR], source_0_m1[PARA_FACTOR];
        #pragma HLS array_partition variable=source_0_1 complete dim=0
        #pragma HLS array_partition variable=source_0_0 complete dim=0
        #pragma HLS array_partition variable=source_m1_0 complete dim=0
        #pragma HLS array_partition variable=source_1_0 complete dim=0
        #pragma HLS array_partition variable=source_0_m1 complete dim=0


        unsigned int idx_k = k << 5;

        uint32_t temp_source_0_1 = (k<15)?source_block_0.range(idx_k + 63, idx_k + 32) : source_block_1.range(idx_k + -449, idx_k + -480);
        source_0_1[k] = *((float*)(&temp_source_0_1));
        uint32_t temp_source_0_0 = source_block_0.range(idx_k+31, idx_k);
        source_0_0[k] = *((float*)(&temp_source_0_0));
        uint32_t temp_source_m1_0 = source_block_m64.range(idx_k+31, idx_k);
        source_m1_0[k] = *((float*)(&temp_source_m1_0));
        uint32_t temp_source_1_0 = source_block_64.range(idx_k+31, idx_k);
        source_1_0[k] = *((float*)(&temp_source_1_0));
        uint32_t temp_source_0_m1 = (k<1)?source_block_m1.range(idx_k + 511, idx_k + 480) : source_block_0.range(idx_k + -1, idx_k + -32);
        source_0_m1[k] = *((float*)(&temp_source_0_m1));

        float result = DENOISE2D_stencil_kernel(source_0_1[k], source_0_0[k], source_m1_0[k], source_1_0[k], source_0_m1[k]);
        out_temp.range(idx_k+31, idx_k) = *((uint32_t *)(&result));
      }
      product.write(out_temp);

      source_block_m64 = source_stream_m63_to_m2.read();
      source_stream_m63_to_m2.write(source_block_m1);
      source_block_m1 = source_block_0;
      source_block_0 = source_block_1;
      source_block_1 = source_stream_2_to_63.read();
      source_stream_2_to_63.write(source_block_64);

      source_block_64 = source.read();
    }

    INTERFACE_WIDTH popout_source_stream_m63_to_m2;
    while(!source_stream_m63_to_m2.empty()){
      popout_source_stream_m63_to_m2 = source_stream_m63_to_m2.read();
    }
    INTERFACE_WIDTH popout_source_stream_2_to_63;
    while(!source_stream_2_to_63.empty()){
      popout_source_stream_2_to_63 = source_stream_2_to_63.read();
    }
  }
  return;
} // stencil kernel definition

// ============ memory engine : split of the TAPA async_mmap `load` (read / write) ============
void read_source(hls::burst_maxi<INTERFACE_WIDTH> source, hls::stream<INTERFACE_WIDTH>& k_wr)
{
  const unsigned int N_in = GRID_COLS/WIDTH_FACTOR*PART_ROWS + (TOP_APPEND+BOTTOM_APPEND) * STAGE_COUNT;
  source.read_request(0, N_in);
  for (unsigned int i = 0; i < N_in; i++) {
    #pragma HLS pipeline II=1
    k_wr.write(source.read());
  }
}
void write_product(hls::burst_maxi<INTERFACE_WIDTH> product, hls::stream<INTERFACE_WIDTH>& k_rd)
{
  const unsigned int N_out = GRID_COLS/WIDTH_FACTOR*PART_ROWS;
  product.write_request(0, N_out);
  for (unsigned int i = 0; i < N_out; i++) {
    #pragma HLS pipeline II=1
    product.write(k_rd.read());
  }
  product.write_response();
}

// ============================ top : COUNT=1 single PE ============================
// TAPA:  tapa::task().invoke(inter_kernel, ...).invoke(DENOISE2D, ...)
//  ->    one HLS DATAFLOW region: read_source -> DENOISE2D -> write_product
void unikernel(hls::burst_maxi<INTERFACE_WIDTH> source_0,
               hls::burst_maxi<INTERFACE_WIDTH> product_0, uint32_t iters)
{
  #pragma HLS INTERFACE m_axi port=source_0  bundle=gmem0 max_read_burst_length=64  num_read_outstanding=8  depth=MIDDLE_REGION
  #pragma HLS INTERFACE m_axi port=product_0 bundle=gmem1 max_write_burst_length=64 num_write_outstanding=8 depth=MIDDLE_REGION
  #pragma HLS INTERFACE s_axilite port=iters
  #pragma HLS INTERFACE s_axilite port=return

  #pragma HLS DATAFLOW
  hls::stream<INTERFACE_WIDTH> k_wr_0;
  hls::stream<INTERFACE_WIDTH> k_rd_0;
  #pragma HLS STREAM variable=k_wr_0 depth=4
  #pragma HLS STREAM variable=k_rd_0 depth=4

  read_source(source_0, k_wr_0);
  DENOISE2D(k_wr_0, k_rd_0, iters);
  write_product(product_0, k_rd_0);
}