# FPGA-Litho BRAM Version Co-Simulation Script
# Phase 6E: RTL Verification
#
# @author FPGA-Litho Team
# @date 2026-04-04

# Open existing project
open_project hls_litho_system_bram_proj

# Open solution
open_solution solution1

# Set target device
set_part xcku3p-ffvb676-2-e

# Create clock
create_clock -period 5ns -name default

# Co-Simulation settings
set_top hls_litho_system_bram

# Run Co-Simulation (Vitis Kernel flow)
# Use random data for verification
cosim_design -trace_level all -setup

# Exit
exit