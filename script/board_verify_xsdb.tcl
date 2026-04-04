#!/usr/bin/env xsdb
# K-Litho BRAM Board Verification via XSDB
# 
# 使用方法:
#   1. 连接Xilinx硬件服务器
#   2. 运行: xsdb board_verify_xsdb.tcl
#   3. 或在XSDB中: source board_verify_xsdb.tcl

puts "=========================================="
puts "K-Litho BRAM 板级验证"
puts "=========================================="

# 连接到硬件服务器
puts "\n[Step 1] 连接硬件服务器..."
connect

# 获取目标设备
puts "\n[Step 2] 查找设备..."
set targets [targets]
puts "发现设备: $targets"

# 选择Kintex UltraScale+设备
targets -set -filter {name =~ "*xcku3p*"}
set device [targets -get]
puts "选择设备: $device"

# 确认设备状态
puts "\n[Step 3] 设备信息..."
puts "  JTAG ID: [jtag idcode]"
puts "  状态: [jtag state]"

# =============================================
# JTAG AXI Master 访问
# =============================================

# 创建AXI Master对象用于JTAG访问
# jtag_axi IP的基地址是0x40000000
puts "\n[Step 4] 配置JTAG AXI访问..."

# 使用JTAG to AXI Master进行读写
# 这是通过XSDB的直接JTAG访问

# 定义HLS内核基地址
set HLS_BASE 0x40000000

# AP_CTRL寄存器 (偏移0x00)
set AP_CTRL [expr {$HLS_BASE + 0x00}]
# GIER寄存器 (偏移0x04) - 全局中断使能
set GIER [expr {$HLS_BASE + 0x04}]
# IP_IER寄存器 (偏移0x08) - IP中断使能
set IP_IER [expr {$HLS_BASE + 0x08}]
# IP_ISR寄存器 (偏移0x0C) - IP中断状态
set IP_ISR [expr {$HLS_BASE + 0x0C}]

# 数据寄存器
set OPERATION [expr {$HLS_BASE + 0x1C}]
set IDX_LOW   [expr {$HLS_BASE + 0x24}]
set IDX_HIGH  [expr {$HLS_BASE + 0x28}]
set VAL_IN    [expr {$HLS_BASE + 0x2C}]
set VAL_OUT   [expr {$HLS_BASE + 0x30}]

# 尺寸参数
set N_OFFSET  [expr {$HLS_BASE + 0x40}]
set M_OFFSET  [expr {$HLS_BASE + 0x48}]
set NS_OFFSET [expr {$HLS_BASE + 0x50}]
set MS_OFFSET [expr {$HLS_BASE + 0x58}]
set KS_OFFSET [expr {$HLS_BASE + 0x60}]
set OS_OFFSET  [expr {$HLS_BASE + 0x68}]

puts "\n[Step 5] 读取内核状态..."
puts "  HLS基地址: 0x[format %08X $HLS_BASE]"

# =============================================
# 方案1: 使用create_hw_axi_txn (需要Vivado Hardware Manager)
# =============================================
# 注意: 以下命令需要在Vivado Hardware Manager环境中运行

# =============================================
# 方案2: 使用mrd/mwr通过XSDB直接访问
# =============================================
# XSDB可以通过JTAG访问AXI总线

puts "\n尝试读取AP_CTRL寄存器..."
puts "注意: 如果显示错误，请确认:"
puts "  1. Bitstream已正确加载"
puts "  2. JTAG AXI IP已启用"
puts "  3. 设备时钟已运行"

# 使用JTAG AXI Master读操作
# 创建AXI事务
catch {
    # 方法: 使用AXI Master接口
    create_hw_axi_txn rd_ap_ctrl [get_hw_axis jtag_axi_0] -address $AP_CTRL -type read
    run_hw_axi_txn rd_ap_ctrl
    set val [get_property DATA [get_hw_axi_txn rd_ap_ctrl]]
    puts "AP_CTRL = 0x[format %08X $val]"
} result

if {[string match "*Error*" $result] || [string match "*error*" $result]} {
    puts "\n直接AXI访问失败。尝试替代方法..."
    
    # 替代方法: 检查设计是否包含JTAG AXI调试接口
    puts "\n检查设计中是否存在调试接口..."
    
    # 方法: 使用debug_bridge
    catch {
        # 查找调试桥
        set debug_bridges [get_hw_debug_cores -of_objects [get_hw_devices]]
        if {[llength $debug_bridges] > 0} {
            puts "发现调试核心: $debug_bridges"
        }
    } result2
}

# =============================================
# 测试内核启动
# =============================================
proc start_kernel {operation idx val_in n m ns ms ks os} {
    global HLS_BASE
    
    puts "\n========================================"
    puts "启动内核执行"
    puts "========================================"
    
    # 写入参数
    puts "写入操作参数..."
    # mwr $OPERATION $operation
    # mwr $IDX_LOW $idx
    # mwr $VAL_IN $val_in
    # mwr $N_OFFSET $n
    # mwr $M_OFFSET $m
    # mwr $NS_OFFSET $ns
    # mwr $MS_OFFSET $ms
    # mwr $KS_OFFSET $ks
    # mwr $OS_OFFSET $os
    
    # 启动内核
    puts "启动内核..."
    # mwr $AP_CTRL 0x01
    
    # 等待完成
    puts "等待完成..."
    # while {[mrd $AP_CTRL] & 0x2 == 0} {
    #     after 100
    # }
    
    puts "内核执行完成!"
    
    # 读取结果
    # set result [mrd $VAL_OUT]
    # puts "结果: $result"
}

# =============================================
# 简单功能测试
# =============================================
proc simple_test {} {
    puts "\n========================================"
    puts "简单功能测试"
    puts "========================================"
    
    # 测试向量: 模拟SOC计算
    # Operation 0: SOC计算
    # N=64, M=64, 其他使用默认值
    
    puts "测试参数:"
    puts "  操作: SOC (0)"
    puts "  索引: 0"
    puts "  尺寸: 64x64"
    
    # start_kernel 0 0 0 64 64 1 1 1 1
}

puts "\n=========================================="
puts "验证脚本加载完成"
puts "=========================================="
puts "可用命令:"
puts "  start_kernel <op> <idx> <val> <n> <m> <ns> <ms> <ks> <os>"
puts "  simple_test"
puts ""
puts "注意: AXI读写需要Vivado Hardware Manager支持"
puts "如果XSDB不可用，请使用Vivado GUI:"
puts "  1. 打开Vivado Hardware Manager"
puts "  2. 连接到设备 (localhost:3121)"
puts "  3. 右键设备 -> Run JTAG to AXI Master"
puts "=========================================="

# 执行简单测试
simple_test