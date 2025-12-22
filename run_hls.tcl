# 1. Create Project
# This will create a folder named "cong_stencil_project_2"
open_project -reset cong_stencil_project_2_no_lcs_padded

# 2. Add Design Files
# NOTE: Ensure that .cpp files are in the same directory as this script
add_files cong_no_lcs.cpp
add_files -tb cong_no_lcs_tb.cpp

# 3. Set Top-Level Function
set_top architecture_top_level

# ########################################################
# 4. Create Solution

# Enable Vitis Flow Target (generates .xo kernel for Vitis Platform)
# If you want simple IP for Vivado, remove "-flow_target vitis"
open_solution -flow_target vitis -reset "solution1"

# 5. Set Board & Clock
# Board: ZCU104 (xczu7ev-ffvc1156-2-e)
set_part {xczu7ev-ffvc1156-2-e}

# Clock: 300MHz (Period ~4ns)
create_clock -period 7 -name default

# --- CONFIGURATION / OPTIMIZATION COMMANDS ---

# Disable automatic loop pipelining (we control this manually with pragmas)
config_compile -pipeline_loops 0

# Enable "Unsafe Math Optimizations" 
# Allows reordering of float operations for better latency/DSPs
config_compile -unsafe_math_optimizations

# Set default FIFO implementation to LUTRAM (for small buffers)
config_storage fifo -impl lutram

# ----------------------------------------------------

# 6. Run Synthesis
csynth_design

# 7. Run Co-Simulation
# Profiling enabled to check for deadlocks/stalls
cosim_design -trace_level all -enable_dataflow_profiling

exit

#vitis_hls -f run_hls.tcl
#vitis_hls -p cong_stencil_project_2

#call C:\Xilinx2025.2\2025.2\Vitis\settings64.bat
#cd /d C:\GitHub\HLS_Thesis
#vitis-run --mode hls --tcl run_hls.tcl

#%APPDATA%/Xilinx/HLS_init.tcl