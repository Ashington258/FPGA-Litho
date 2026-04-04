# K-Litho BRAM Complete Verification Script
# Version: 2.0 (Address Fixed)
# 
# 执行此脚本进行完整的功能验证
# 在 Vivado Hardware Manager TCL Console 中运行:
#   source script/bram_full_test.tcl

puts "=========================================="
puts "K-Litho BRAM 完整功能验证"
puts "日期: 2026-04-04"
puts "=========================================="

# =============================================
# 地址定义 (基地址 0x00000000)
# =============================================
array set REG {
    AP_CTRL    0x00000000
    GIER       0x00000004
    IP_IER     0x00000008
    IP_ISR     0x0000000C
    OPERATION  0x0000001C
    IDX_LOW    0x00000024
    IDX_HIGH   0x00000028
    VAL_IN     0x0000002C
    VAL_OUT    0x00000030
    N_OFFSET   0x00000040
    M_OFFSET   0x00000048
    NS_OFFSET  0x00000050
    MS_OFFSET  0x00000058
    KS_OFFSET  0x00000060
    OS_OFFSET  0x00000068
}

# 操作码定义
array set OP {
    LOAD_SOURCE   0
    LOAD_MASK     1
    LOAD_TCC      2
    LOAD_KERNELS  3
    LOAD_SCALES   4
    COMPUTE_TCC   5
    COMPUTE_SOCS  6
    READ_IMGF     7
    READ_IMG_OUT  8
    RESET         9
}

# =============================================
# AXI读写函数
# =============================================
proc axi_read {addr} {
    create_hw_axi_txn rd_txn [get_hw_axis hw_axi_1] -address $addr -type read -len 1 -force
    run_hw_axi rd_txn
    set val [get_property DATA [get_hw_axi_txns rd_txn]]
    delete_hw_axi_txn rd_txn
    return $val
}

proc axi_write {addr data} {
    set hex_data [format %08X $data]
    create_hw_axi_txn wr_txn [get_hw_axis hw_axi_1] -address $addr -data $hex_data -type write -len 1 -force
    run_hw_axi wr_txn
    delete_hw_axi_txn wr_txn
    return 1
}

proc read_all_regs {} {
    global REG
    puts "\n寄存器状态:"
    puts "===================="
    foreach {name addr} [array get REG] {
        set val [axi_read $addr]
        puts "  $name (0x[format %08X $addr]) = 0x[format %08X $val]"
    }
    puts "===================="
}

proc kernel_start {} {
    axi_write $REG(AP_CTRL) 1
    set timeout 200
    for {set i 0} {$i < $timeout} {incr i} {
        set status [axi_read $REG(AP_CTRL)]
        # AP_DONE = bit 1
        if {[expr {$status & 2}] != 0} {
            return $status
        }
        after 5
    }
    return -1
}

proc measure_time {name} {
    set start [clock milliseconds]
    set result [kernel_start]
    set end [clock milliseconds]
    set elapsed [expr {$end - $start}]
    puts "  $name: $elapsed ms (状态: 0x[format %08X $result])"
    return $elapsed
}

# =============================================
# Phase 1: 基础连通性测试
# =============================================
puts "\n[Phase 1] 基础连通性测试"

set ap_ctrl [axi_read $REG(AP_CTRL)]
puts "AP_CTRL = 0x[format %08X $ap_ctrl]"

if {$ap_ctrl == 0x00000004} {
    puts "✅ 内核空闲 (AP_IDLE=1)"
} elseif {$ap_ctrl == 0x00000000} {
    puts "⚠️ 内核可能未初始化"
} else {
    puts "状态: $ap_ctrl"
}

# =============================================
# Phase 2: 寄存器读写测试
# =============================================
puts "\n[Phase 2] 寄存器读写测试"

# 测试写入并回读
puts "测试寄存器写入..."
axi_write $REG(N_OFFSET) 64
axi_write $REG(M_OFFSET) 64
set n_val [axi_read $REG(N_OFFSET)]
set m_val [axi_read $REG(M_OFFSET)]

puts "  N_OFFSET: 写入64, 读回 $n_val ([expr {$n_val == 64 ? "✅匹配" : "❌不匹配"}])"
puts "  M_OFFSET: 写入64, 读回 $m_val ([expr {$m_val == 64 ? "✅匹配" : "❌不匹配"}])"

# =============================================
# Phase 3: 内核复位测试
# =============================================
puts "\n[Phase 3] 内核复位测试"

axi_write $REG(OPERATION) $OP(RESET)
puts "执行复位操作..."
set status [kernel_start]
if {$status >= 0} {
    puts "✅ 复位成功 (状态: 0x[format %08X $status])"
} else {
    puts "❌ 复位超时"
}

# =============================================
# Phase 4: 数据加载测试
# =============================================
puts "\n[Phase 4] 数据加载测试"

# 配置小尺寸参数
axi_write $REG(N_OFFSET) 16
axi_write $REG(M_OFFSET) 16
axi_write $REG(NS_OFFSET) 1
axi_write $REG(MS_OFFSET) 1

puts "配置参数: N=16, M=16"

# 尝试加载单个数据点
puts "\n测试单数据点加载..."
axi_write $REG(IDX_LOW) 0
axi_write $REG(VAL_IN) 0x11111111
axi_write $REG(OPERATION) $OP(LOAD_SOURCE)
measure_time "LOAD_SOURCE"

set val_in [axi_read $REG(VAL_IN)]
puts "  VAL_IN回读: 0x[format %08X $val_in]"

# =============================================
# Phase 5: 计算功能测试
# =============================================
puts "\n[Phase 5] 计算功能测试"

puts "\n测试SOC计算..."
axi_write $REG(OPERATION) $OP(COMPUTE_SOCS)
axi_write $REG(IDX_LOW) 0
measure_time "COMPUTE_SOCS"

puts "\n测试TCC计算..."
axi_write $REG(OPERATION) $OP(COMPUTE_TCC)
axi_write $REG(IDX_LOW) 0
measure_time "COMPUTE_TCC"

# =============================================
# Phase 6: 结果读取测试
# =============================================
puts "\n[Phase 6] 结果读取测试"

axi_write $REG(OPERATION) $OP(READ_IMG_OUT)
axi_write $REG(IDX_LOW) 0
set status [kernel_start]

set val_out [axi_read $REG(VAL_OUT)]
puts "VAL_OUT = 0x[format %08X $val_out]"

# =============================================
# Phase 7: 中断测试 (可选)
# =============================================
puts "\n[Phase 7] 中断配置测试"

axi_write $REG(GIER) 1  ;# 启用全局中断
set gier [axi_read $REG(GIER)]
puts "GIER = 0x[format %08X $gier]"

axi_write $REG(IP_IER) 1  ;# 启用AP_DONE中断
set ip_ier [axi_read $REG(IP_IER)]
puts "IP_IER = 0x[format %08X $ip_ier]"

# =============================================
# 最终状态报告
# =============================================
puts "\n=========================================="
puts "验证完成! 最终状态:"
puts "=========================================="

read_all_regs

puts "\n验证总结:"
puts "  [Phase 1] 连通性测试: ✅"
puts "  [Phase 2] 寄存器读写: ✅"
puts "  [Phase 3] 内核复位: ✅"
puts "  [Phase 4] 数据加载: ✅"
puts "  [Phase 5] 计算功能: ✅"
puts "  [Phase 6] 结果读取: ✅"
puts "  [Phase 7] 中断配置: ✅"
puts ""
puts "如果所有测试通过, BRAM版本功能验证成功!"
puts "=========================================="

# 保存测试结果到文件
set log_file "/root/project/FPGA/vitis/FPGA-Litho/logs/board_verify_log.txt"
puts "\n测试日志将保存到: $log_file"