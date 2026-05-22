//  soda_top.cpp
#include "soda_common.h"
#include "load_to_fifos.hpp"
#include "soda_kernel.hpp"
#include "store_from_fifos.hpp"

// TOP LEVEL
void architecture_top_level(hls::burst_maxi<float16> A_in_mem, hls::burst_maxi<float_pack> B_out_mem) {
    #pragma HLS INTERFACE m_axi port=A_in_mem bundle=gmem0 depth=SODA_BURSTS_IN
    #pragma HLS INTERFACE m_axi port=B_out_mem bundle=gmem1 depth=SODA_TOTAL_PACKETS_OUT
    #pragma HLS INTERFACE s_axilite port=return

    #pragma HLS DATAFLOW

    hls::stream<data_t> in_streams[SODA_K];
    hls::stream<data_t> out_streams[SODA_K];
    #pragma HLS ARRAY_PARTITION variable=in_streams complete
    #pragma HLS ARRAY_PARTITION variable=out_streams complete

    hls::stream<float16>   pack_in_stream("pack_in_stream");
    hls::stream<float_pack> pack_stream("pack_stream");
    
    #pragma HLS STREAM variable=in_streams depth=8 
    #pragma HLS STREAM variable=out_streams depth=8
    #pragma HLS STREAM variable=pack_in_stream depth=16 
    #pragma HLS STREAM variable=pack_stream depth=16 
    
    load_input(A_in_mem, pack_in_stream);
    unpack_and_feed(pack_in_stream, in_streams);

    soda_compute<SODA_K, TOTAL_ITERATIONS>(in_streams, out_streams);

    filter_and_pack(out_streams, pack_stream);
    store_output(pack_stream, B_out_mem);
}