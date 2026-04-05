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

# Export IP for Vivado IP Catalog flow
# This creates the IP for integration in Vivado Block Design
export_design -format ip_catalog

# Exit
exit