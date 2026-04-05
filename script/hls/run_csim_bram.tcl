# FPGA-Litho BRAM Version C Simulation Script
# Phase 7: C Simulation Verification
#
# Purpose: Verify SOCS algorithm fix (cyclic shift + correct indexing)
# Expected: Test 8 output non-zero (different from pre-fix value 122500)
#
# @author FPGA-Litho Team
# @date 2026-04-05

puts "=========================================="
puts "Phase 7: C Simulation Verification"
puts "Date: [clock format [clock seconds]]"
puts "=========================================="

# Open existing project
open_project hls_litho_system_bram_proj

# Open solution
open_solution solution1

# Set top function
set_top hls_litho_system_bram

# Run C Simulation
puts "=========================================="
puts "Running C Simulation for BRAM Version..."
puts "Verifying SOCS algorithm fix..."
puts "=========================================="

csim_design -clean

puts "=========================================="
puts "C Simulation Completed!"
puts "Please check Test 8 output result"
puts "=========================================="

# Exit
exit