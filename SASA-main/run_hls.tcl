#
# SASA DENOISE2D single-PE (COUNT=1) — standalone Vitis HLS flow (csim / csynth / cosim).
# Same conditions as your K=16 SODA run_hls.tcl; only the top / sources / solution name change.
#
#   top function : unikernel
#   kernel src   : sasa_src/unikernel.cpp   (needs sasa_src/DENOISE2D.h next to it)
#   testbench    : sasa_src/sasa_csim_tb.cpp
#

open_project -reset hls_project

add_files    sasa_src/unikernel.cpp    -cflags "-std=c++14 -Isasa_src -DALLOW_EMPTY_HLS_STREAM_READS"
add_files -tb sasa_src/sasa_csim_tb.cpp -cflags "-std=c++14 -Isasa_src -DALLOW_EMPTY_HLS_STREAM_READS"

set_top unikernel

open_solution -flow_target vitis -reset "unikernel"

set_part {xcu55c-fsvh2892-2L-e}

create_clock -period 4.000 -name default

config_compile -pipeline_loops 0
config_interface -m_axi_conservative_mode=0
config_compile -enable_auto_rewind
config_rtl -add_register_in_block_condition=false
config_compile -unsafe_math_optimizations
config_storage fifo -impl auto -auto_srl_max_bits 0 -auto_srl_max_depth 0
config_array_partition -throughput_driven off
config_array_partition -complete_threshold 1

csim_design
csynth_design
cosim_design -trace_level all -enable_dataflow_profiling

exit

# vitis_hls -f run_hls.tcl
# (Windows) vitis-run --mode hls --tcl run_hls.tcl
