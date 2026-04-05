# ============================================================================
# 添加VIO IP到设计中，用于硬件调试
# ============================================================================
# 用途：在Vivado Block Design中添加VIO IP，实现JTAG调试功能
# 执行：vivado -mode tcl -source add_vio_debug.tcl
# ============================================================================

puts "=========================================="
puts "添加VIO IP到Block Design"
puts "=========================================="

# 打开Vivado项目
set project_path "/root/project/FPGA/vivado/test_bram_litho/test_bram_litho.xpr"
puts "打开项目: $project_path"
open_project $project_path

# 打开Block Design
puts "打开Block Design..."
open_bd_design design_1

# ================== 创建VIO IP ==================
puts "\n创建VIO IP..."

# 配置VIO参数
set vio_cell [create_bd_cell -type ip -vlnv xilinx.com:ip:vio:3.0 vio_debug]
puts "  ✓ 已创建VIO IP: $vio_cell"

# 配置VIO：
# - 2个输出信号（用于控制AP_START和OPERATION选择）
# - 1个输入信号（用于监控AP_DONE）
set_property -dict [list \
    CONFIG.C_NUM_PROBE_OUT {2} \
    CONFIG.C_NUM_PROBE_IN {1} \
    CONFIG.C_PROBE_OUT0_INIT_VAL {0} \
    CONFIG.C_PROBE_OUT1_INIT_VAL {0} \
] [get_bd_cells $vio_cell]

puts "  ✓ 已配置VIO参数"

# ================== 连接时钟和复位 ==================
puts "\n连接时钟和复位..."

# 连接时钟
connect_bd_net [get_bd_pins $vio_cell/clk] [get_bd_pins ps8_0_axi_periph/M00_ACLK]

# 连接复位
connect_bd_net [get_bd_pins $vio_cell/reset] [get_bd_pins proc_sys_reset_0/peripheral_aresetn]

puts "  ✓ 时钟和复位已连接"

# ================== 连接到HLS IP控制信号 ==================
puts "\n连接到HLS IP控制信号..."

# 获取HLS IP实例
set hls_ip [get_bd_cells -filter {VLNV =~ "*hls_litho_system_bram*"}]
puts "  找到HLS IP: $hls_ip"

# 创建连接：
# VIO输出0 -> HLS IP的ap_start（通过concat）
# VIO输出1 -> HLS IP的operation[3:0]
# VIO输入0 <- HLS IP的ap_done

# 方案A：通过Slice IP连接到控制寄存器
# 这里需要更复杂的连接逻辑，暂时提供手动操作指导

puts "\n=========================================="
puts "VIO IP已添加到设计！"
puts "=========================================="
puts "\n后续步骤："
puts "  1. 在Vivado GUI中手动连接VIO信号到HLS IP"
puts "  2. 或者使用以下Tcl命令完成连接："
puts ""
puts "     # 连接VIO输出到HLS IP"
puts "     connect_bd_net \[get_bd_pins $vio_cell/probe_out0\] \[get_bd_pins $hls_ip/ap_start\]"
puts "     connect_bd_net \[get_bd_pins $vio_cell/probe_out1\] \[get_bd_pins $hls_ip/operation\[3:0\]\]"
puts ""
puts "     # 连接HLS IP输出到VIO输入"
puts "     connect_bd_net \[get_bd_pins $hls_ip/ap_done\] \[get_bd_pins $vio_cell/probe_in0\]"
puts ""
puts "  3. 保存Block Design"
puts "  4. 重新生成bitstream"
puts "  5. 使用VIO Tcl命令控制硬件"
puts "=========================================="

# 保存设计
save_bd_design
puts "\n✓ Block Design已保存"

# 不自动运行实现，让用户手动完成连接
puts "\n提示：请在Vivado GUI中完成信号连接，然后重新实现设计"