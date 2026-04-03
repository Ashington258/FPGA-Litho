# ============================================================
# SOCS模块HLS C-Synthesis TCL脚本
# ============================================================

# Project setup
open_project hls_socs_proj
set_top hls_calc_socs

# Add source files (headers as design files for include path)
add_files src/hls_socs.cpp
add_files include/hls_socs.h
add_files include/hls_types.h

# Add testbench
add_files -tb testbench/socs_tb.cpp

# Solution setup
open_solution solution1

# Target device: Kintex UltraScale+
set_part {xcku3p-ffvb676-2-e}

# Clock constraint: 5ns = 200MHz
create_clock -period 5 -name default

# Run C simulation first
csim_design

# Run synthesis
csynth_design

puts "=== SOCS Module Synthesis Complete ==="

exit

# 打开解决方案
open_solution -reset $solution_name

# 设置器件
set_part $part

# 设置时钟
create_clock -period $clock_period -name default

# 运行C仿真
csim_design

# 运行C综合
csynth_design

# 输出报告
report_csynth

# 关闭项目
close_project

puts "============================================"
puts "SOCS HLS C-Synthesis Completed"
puts "============================================"