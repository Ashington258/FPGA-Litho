# =============================================================================
# FPGA-Litho System Integration Synthesis Script
# =============================================================================
# 
# 运行方式: vitis-run --mode hls --tcl script/run_csynth_system.tcl --work_dir hls_system_proj
#
# =============================================================================

# 项目设置
set project_name "hls_litho_system_proj"
set solution_name "solution1"
set top_function "hls_litho_system"
set target_device "xcku3p-ffvb676-2-e"
set clock_period 5

# 打开/创建项目
open_project -reset $project_name

# 添加源文件
add_files {
    src/hls_litho_system.cpp
    src/hls_calc_image_integrated.cpp
    src/hls_socs.cpp
}

# 添加测试平台
add_files -tb {
    testbench/litho_system_tb.cpp
}

# 设置顶层函数
set_top $top_function

# 打开解决方案
open_solution -reset $solution_name

# 设置目标设备
set_part $target_device

# 创建时钟约束 (5ns = 200MHz)
create_clock -period $clock_period -name default

# 设置时钟不确定性
set_clock_uncertainty 1.35

# 配置优化
# - 目标II=1 对于核心循环
# - 使用BRAM存储中间结果
config_compile -unsafe_math_optimizations

# 运行C仿真
puts "=========================================="
puts "Running C Simulation..."
puts "=========================================="
csim_design

# 运行C综合
puts "=========================================="
puts "Running C Synthesis..."
puts "=========================================="
csynth_design

# 显示综合报告
puts "=========================================="
puts "Synthesis Complete!"
puts "=========================================="
puts "Report: $project_name/$solution_name/syn/report/${top_function}_csynth.rpt"

# 导出RTL (可选)
# export_design -flow syn -rtl verilog -format ip_catalog

# 退出
exit