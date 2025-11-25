# 1. Δημιουργία Project
# Θα δημιουργήσει φάκελο "cong_stencil_project"
open_project -reset cong_stencil_project_2

# 2. Προσθήκη Αρχείων
# ΠΡΟΣΟΧΗ: Βεβαιώσου ότι τα αρχεία .cpp είναι στον ίδιο φάκελο με αυτό το script
add_files first_try_cong.cpp
add_files -tb cong_testbench.cpp

# 3. Ορισμός Top-Level Συνάρτησης
set_top architecture_top_level

# ########################################################
# 4. Δημιουργία Solution

# Ενεργοποίηση του Vitis Flow Target (παράγει .xo kernel για Vitis Platform)
# Αν θέλεις απλό IP για Vivado, αφαίρεσε το "-flow_target vitis"
open_solution -flow_target vitis -reset "solution1"

# 5. Ορισμός Πλακέτας & Ρολογιού
# Πλακέτα: ZCU104 (xczu7ev-ffvc1156-2-e)
set_part {xczu7ev-ffvc1156-2-e}

# Ρολόι: 300MHz (Περίοδος ~3.33ns)
create_clock -period 4 -name default

# --- ΕΝΤΟΛΕΣ ΔΙΑΜΟΡΦΩΣΗΣ (OPTIMIZATIONS) ---

# Απενεργοποίηση αυτόματου pipelining (το ελέγχουμε εμείς με pragmas)
config_compile -pipeline_loops 0

# Ενεργοποίηση "Unsafe Math Optimizations" 
# Επιτρέπει αναδιάταξη πράξεων float για καλύτερο latency/DSPs
config_compile -unsafe_math_optimizations

# Ορισμός Default FIFO σε LUTRAM (για τα μικρά buffers)
# ΣΗΜΑΝΤΙΚΟ: Τα μεγάλα buffers (1023) πρέπει να έχουν #pragma HLS BIND_STORAGE ... impl=bram στον κώδικα C++
config_storage fifo -impl lutram

# ----------------------------------------------------

# 6. Εκτέλεση Synthesis
csynth_design

# 7. Εκτέλεση Co-Simulation
# Ενεργοποιημένο tracing και profiling για πλήρη έλεγχο
cosim_design -trace_level none -enable_dataflow_profiling

vitis_hls -p cong_stencil_project_2

exit