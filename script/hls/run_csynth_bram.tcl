# HLS C Synthesis Script for BRAM Version (Vitis 2025.2)
# =======================================================

puts "=========================================="
puts "Starting BRAM Version C Synthesis"
puts "=========================================="

# Create a new project
open_project -reset hls_litho_system_bram_proj

# Add source files
add_files src/hls_litho_system_bram.cpp
add_files include/hls_litho_system_bram.h
add_files include/hls_types.h
add_files include/hls_tcc.h
add_files include/hls_socs.h
add_files include/hls_calc_image_integrated.h

# Add testbench
add_files -tb testbench/litho_system_bram_tb.cpp

# Set top function
set_top hls_litho_system_bram

# Create solution
open_solution -flow_target vitis "solution1"

# Set part and clock
# Target: xcku3p FPGA, 200MHz (5ns period)
set_part {xcku3p-ffvb676-2-e}
create_clock -period 5 -name default

# Run C Synthesis
puts "=========================================="
puts "Running C Synthesis for BRAM Version..."
puts "Target: xcku3p, 200MHz, BRAM storage"
puts "=========================================="

csynth_design

puts "=========================================="
puts "C Synthesis Completed!"
puts "=========================================="

# Print resource utilization summary
puts "\n*** Resource Utilization Summary ***"
puts "BRAM Blocks: Check report for BRAM utilization"
puts "Estimated Fmax: Check report for timing analysis"

exit