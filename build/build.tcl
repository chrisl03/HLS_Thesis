open_project -reset hls_project

add_files src/SODA_lcs_generic_k.cpp -cflags "-std=c++14 -DALLOW_EMPTY_HLS_STREAM_READS"
add_files -tb src/SODA_lcs_generic_k_tb.cpp -cflags "-std=c++14 -DALLOW_EMPTY_HLS_STREAM_READS"

set_top architecture_top_level

open_solution -flow_target vitis -reset "architecture_top_level"

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

#vitis_hls -f run_hls.tcl
#vitis_hls -p cong_stencil_project_2

#call C:\Xilinx2025.2\2025.2\Vitis\settings64.bat
#cd /d C:\GitHub\HLS_Thesis
#vitis-run --mode hls --tcl run_hls.tcl

#%APPDATA%/Xilinx/HLS_init.tcl