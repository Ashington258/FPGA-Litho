# HLS C Synthesis Script for Vitis 2025.2
# ==========================================

# Create a new project
open_project -reset hls_top_simple_proj

# Add source files
add_files src/hls_fft_simple.cpp
add_files include/hls_types.h
add_files include/hls_fft_simple.h

# Add testbench
add_files -tb testbench/fft_tb_simple.cpp
add_files -tb data

# Set top function
set_top hls_top_simple

# Create solution
open_solution -flow_target vitis "solution1"

# Set part and clock
set_part {xcku3p-ffvb676-2-e}
create_clock -period 4 -name default

# Run C Synthesis
puts "=========================================="
puts "Running C Synthesis..."
puts "=========================================="

csynth_design

puts "=========================================="
puts "C Synthesis Completed!"
puts "=========================================="

exit