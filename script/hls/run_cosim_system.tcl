# RTL Co-Simulation for hls_litho_system
# Date: 2026-04-03
# Target: 200MHz (5ns clock)

# Open existing project
open_project hls_litho_system_proj

# Open solution
open_solution solution1

# Run C Synthesis first (required for Co-Sim)
csynth_design

# Run RTL Co-Simulation
# Options: -trace_level all, -rtl verilog
cosim_design -trace_level all -rtl verilog

# Close project
close_project