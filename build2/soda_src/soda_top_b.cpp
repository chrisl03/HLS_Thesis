//  soda_top_b.cpp  (Kernel B, SLR1)  K=64, 8 read + 8 write, paired interleaved.
#include "soda_common.h"
#include "load_to_fifos.hpp"
#include "soda_kernel.hpp"
#include "store_from_fifos.hpp"

void architecture_top_b(
    hls::burst_maxi<float16> A_in_mem_0,
    hls::burst_maxi<float16> A_in_mem_1,
    hls::burst_maxi<float16> A_in_mem_2,
    hls::burst_maxi<float16> A_in_mem_3,
    hls::burst_maxi<float16> A_in_mem_4,
    hls::burst_maxi<float16> A_in_mem_5,
    hls::burst_maxi<float16> A_in_mem_6,
    hls::burst_maxi<float16> A_in_mem_7,
    hls::burst_maxi<float16> B_out_mem_0,
    hls::burst_maxi<float16> B_out_mem_1,
    hls::burst_maxi<float16> B_out_mem_2,
    hls::burst_maxi<float16> B_out_mem_3,
    hls::burst_maxi<float16> B_out_mem_4,
    hls::burst_maxi<float16> B_out_mem_5,
    hls::burst_maxi<float16> B_out_mem_6,
    hls::burst_maxi<float16> B_out_mem_7) {
    #pragma HLS INTERFACE m_axi port=A_in_mem_0 bundle=gmemr0 max_read_burst_length=64 num_read_outstanding=8 depth=BURSTS_PER_CH_EVEN
    #pragma HLS INTERFACE m_axi port=A_in_mem_1 bundle=gmemr1 max_read_burst_length=64 num_read_outstanding=8 depth=BURSTS_PER_CH_EVEN
    #pragma HLS INTERFACE m_axi port=A_in_mem_2 bundle=gmemr2 max_read_burst_length=64 num_read_outstanding=8 depth=BURSTS_PER_CH_EVEN
    #pragma HLS INTERFACE m_axi port=A_in_mem_3 bundle=gmemr3 max_read_burst_length=64 num_read_outstanding=8 depth=BURSTS_PER_CH_EVEN
    #pragma HLS INTERFACE m_axi port=A_in_mem_4 bundle=gmemr4 max_read_burst_length=64 num_read_outstanding=8 depth=BURSTS_PER_CH_EVEN
    #pragma HLS INTERFACE m_axi port=A_in_mem_5 bundle=gmemr5 max_read_burst_length=64 num_read_outstanding=8 depth=BURSTS_PER_CH_EVEN
    #pragma HLS INTERFACE m_axi port=A_in_mem_6 bundle=gmemr6 max_read_burst_length=64 num_read_outstanding=8 depth=BURSTS_PER_CH_EVEN
    #pragma HLS INTERFACE m_axi port=A_in_mem_7 bundle=gmemr7 max_read_burst_length=64 num_read_outstanding=8 depth=BURSTS_PER_CH_EVEN
    #pragma HLS INTERFACE m_axi port=B_out_mem_0 bundle=gmemw0 max_write_burst_length=64 num_write_outstanding=8 depth=BURSTS_PER_CH_EVEN
    #pragma HLS INTERFACE m_axi port=B_out_mem_1 bundle=gmemw1 max_write_burst_length=64 num_write_outstanding=8 depth=BURSTS_PER_CH_EVEN
    #pragma HLS INTERFACE m_axi port=B_out_mem_2 bundle=gmemw2 max_write_burst_length=64 num_write_outstanding=8 depth=BURSTS_PER_CH_EVEN
    #pragma HLS INTERFACE m_axi port=B_out_mem_3 bundle=gmemw3 max_write_burst_length=64 num_write_outstanding=8 depth=BURSTS_PER_CH_EVEN
    #pragma HLS INTERFACE m_axi port=B_out_mem_4 bundle=gmemw4 max_write_burst_length=64 num_write_outstanding=8 depth=BURSTS_PER_CH_EVEN
    #pragma HLS INTERFACE m_axi port=B_out_mem_5 bundle=gmemw5 max_write_burst_length=64 num_write_outstanding=8 depth=BURSTS_PER_CH_EVEN
    #pragma HLS INTERFACE m_axi port=B_out_mem_6 bundle=gmemw6 max_write_burst_length=64 num_write_outstanding=8 depth=BURSTS_PER_CH_EVEN
    #pragma HLS INTERFACE m_axi port=B_out_mem_7 bundle=gmemw7 max_write_burst_length=64 num_write_outstanding=8 depth=BURSTS_PER_CH_EVEN
    #pragma HLS INTERFACE s_axilite port=return

    #pragma HLS DATAFLOW

    hls::stream<data_t> in_streams[SODA_K];
    hls::stream<data_t> out_streams[SODA_K];
    #pragma HLS ARRAY_PARTITION variable=in_streams complete
    #pragma HLS ARRAY_PARTITION variable=out_streams complete
    #pragma HLS STREAM variable=in_streams depth=8
    #pragma HLS STREAM variable=out_streams depth=8

    hls::stream<float16> s0("s0"), s1("s1"), s2("s2"), s3("s3");
    hls::stream<float16> s4("s4"), s5("s5"), s6("s6"), s7("s7");
    #pragma HLS STREAM variable=s0 depth=8
    #pragma HLS STREAM variable=s1 depth=8
    #pragma HLS STREAM variable=s2 depth=8
    #pragma HLS STREAM variable=s3 depth=8
    #pragma HLS STREAM variable=s4 depth=8
    #pragma HLS STREAM variable=s5 depth=8
    #pragma HLS STREAM variable=s6 depth=8
    #pragma HLS STREAM variable=s7 depth=8

    hls::stream<float16> ws0("ws0"), ws1("ws1"), ws2("ws2"), ws3("ws3");
    hls::stream<float16> ws4("ws4"), ws5("ws5"), ws6("ws6"), ws7("ws7");
    #pragma HLS STREAM variable=ws0 depth=4
    #pragma HLS STREAM variable=ws1 depth=4
    #pragma HLS STREAM variable=ws2 depth=4
    #pragma HLS STREAM variable=ws3 depth=4
    #pragma HLS STREAM variable=ws4 depth=4
    #pragma HLS STREAM variable=ws5 depth=4
    #pragma HLS STREAM variable=ws6 depth=4
    #pragma HLS STREAM variable=ws7 depth=4

    read_channel_even(A_in_mem_0, s0);
    read_channel_odd (A_in_mem_1, s1);
    read_channel_even(A_in_mem_2, s2);
    read_channel_odd (A_in_mem_3, s3);
    read_channel_even(A_in_mem_4, s4);
    read_channel_odd (A_in_mem_5, s5);
    read_channel_even(A_in_mem_6, s6);
    read_channel_odd (A_in_mem_7, s7);

    unpack_and_feed(s0, s1, s2, s3, s4, s5, s6, s7, in_streams);

    soda_compute<SODA_K, TOTAL_ITERATIONS>(in_streams, out_streams);

    collect_and_split(out_streams, ws0, ws1, ws2, ws3, ws4, ws5, ws6, ws7);

    write_channel_even(ws0, B_out_mem_0);
    write_channel_odd (ws1, B_out_mem_1);
    write_channel_even(ws2, B_out_mem_2);
    write_channel_odd (ws3, B_out_mem_3);
    write_channel_even(ws4, B_out_mem_4);
    write_channel_odd (ws5, B_out_mem_5);
    write_channel_even(ws6, B_out_mem_6);
    write_channel_odd (ws7, B_out_mem_7);
}