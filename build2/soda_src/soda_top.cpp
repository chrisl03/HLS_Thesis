//  soda_top.cpp  —  COALESCED version
//  unpack_and_feed + filter_and_pack REMOVED. Data stays 512-bit end-to-end.
#include "soda_common.h"
#include "load_to_fifos.hpp"      // only load_input is used now (unpack_and_feed left unused)
#include "soda_kernel.hpp"        // coalesced soda_compute_coalesced
#include "store_from_fifos.hpp"   // only store_output is used now (filter_and_pack left unused)

// TOP LEVEL
void architecture_top_level(hls::burst_maxi<float16> A_in_mem, hls::burst_maxi<float_pack> B_out_mem) {
    #pragma HLS INTERFACE m_axi port=A_in_mem  bundle=gmem0 max_read_burst_length=64  num_read_outstanding=16  depth=SODA_BURSTS_IN
    #pragma HLS INTERFACE m_axi port=B_out_mem bundle=gmem1 max_write_burst_length=64 num_write_outstanding=16 depth=SODA_TOTAL_PACKETS_OUT
    #pragma HLS INTERFACE s_axilite port=return

    #pragma HLS DATAFLOW

    hls::stream<float16>    in_stream("in_stream");
    hls::stream<float_pack> out_stream("out_stream");
    #pragma HLS STREAM variable=in_stream  depth=8
    #pragma HLS STREAM variable=out_stream depth=8

    load_input(A_in_mem, in_stream);                 // 512-bit words in   (unchanged)
    soda_compute_coalesced(in_stream, out_stream);   // coalesced reuse + denoise
    store_output(out_stream, B_out_mem);             // 512-bit words out  (unchanged)
}