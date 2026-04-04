# K-Litho BRAM Version IP Package Script
# Phase 6E: Export Vitis Kernel IP
#
# @author K-Litho Team
# @date 2026-04-04

# Open existing project
open_project hls_litho_system_bram_proj

# Open solution
open_solution solution1

# Set target device
set_part xcku3p-ffvb676-2-e

# Create clock
create_clock -period 5ns -name default

# Export IP for Vitis Kernel flow
# This creates the kernel.xml and required files for xclbin generation
export_design -format xo -output hls_litho_system_bram.xo

# Exit
exit