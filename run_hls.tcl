# 1. Create Project
# This will create a folder named "cong_stencil_project_2"
open_project -reset cong_stencil_project_2

# 2. Add Design Files
# NOTE: Ensure that .cpp files are in the same directory as this script
add_files first_try_cong.cpp
add_files -tb cong_testbench.cpp

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

# Clock: 300MHz (Period ~3.33ns)
create_clock -period 3.33 -name default

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
cosim_design -trace_level none -enable_dataflow_profiling

exit