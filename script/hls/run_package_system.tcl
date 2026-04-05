# =============================================================================
# FPGA-Litho System Export Script (XO/IP Package)
# =============================================================================
# 
# 运行方式: vitis-run --mode hls --tcl script/run_package_system.tcl --work_dir hls_litho_system_proj
#
# 此脚本导出HLS IP为Vitis内核或Vivado IP
# =============================================================================

# 项目设置
set project_name "hls_litho_system_proj"
set solution_name "solution1"
set top_function "hls_litho_system"

# 打开现有项目
open_project $project_name

# 打开解决方案
open_solution $solution_name

puts "=========================================="
puts "Starting IP Export..."
puts "=========================================="

# Vitis HLS export_design 命令
# -format xo: 生成Vitis内核对象(.xo)
# -format ip_catalog: 生成Vivado IP
# -evaluation: 评估版本

# 生成Vivado IP (用于Vivado IP集成器)
export_design -format ip_catalog \
              -description "FPGA-Litho Lithography Simulation System" \
              -vendor "fpga-litho.org" \
              -version "1.0" \
              -display_name "FPGA-Litho System"

# 复制生成的IP到输出目录
file mkdir "ip_export"
file copy -force "${project_name}/${solution_name}/impl/ip" "ip_export/hls_litho_system_ip"

puts "=========================================="
puts "IP Export Complete!"
puts "=========================================="
puts "Output: ip_export/hls_litho_system_ip/"
puts "Format: Vivado IP Catalog"

# 关闭项目
close_project

exit