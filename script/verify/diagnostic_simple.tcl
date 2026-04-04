puts "=========================================="
puts "K-Litho 数据完整性诊断测试"
puts "=========================================="

set BASE 0x00000000

# 寄存器地址
set AP_CTRL    [expr {$BASE + 0x00}]
set OPERATION  [expr {$BASE + 0x1C}]
set IDX_LOW    [expr {$BASE + 0x24}]
set VAL_IN_REAL [expr {$BASE + 0x2C}]
set VAL_IN_IMAG [expr {$BASE + 0x30}]
set VAL_OUT_REAL [expr {$BASE + 0x34}]
set LX_OFFSET  [expr {$BASE + 0x70}]
set LY_OFFSET  [expr {$BASE + 0x74}]
set NKERNELS   [expr {$BASE + 0x7C}]

# 操作码
set OP_LOAD_MASK     1
set OP_LOAD_KERNELS  3
set OP_LOAD_SCALES   4
set OP_COMPUTE_SOCS  6
set OP_READ_IMG_OUT  8
set OP_RESET         9

set AXI [lindex [get_hw_axis *] 0]

# 辅助函数
proc axi_read {addr} {
    global AXI
    create_hw_axi_txn rd_txn $AXI -address $addr -type read -len 1 -force
    run_hw_axi rd_txn
    set val [get_property DATA [get_hw_axi_txns rd_txn]]
    delete_hw_axi_txn rd_txn
    scan $val "%x" intval
    return $intval
}

proc axi_write {addr data} {
    global AXI
    set hex_data [format %08X $data]
    create_hw_axi_txn wr_txn $AXI -address $addr -data $hex_data -type write -len 1 -force
    run_hw_axi wr_txn
    delete_hw_axi_txn wr_txn
}

proc start_kernel {} {
    global AP_CTRL
    axi_write $AP_CTRL 1
    set timeout 50
    for {set i 0} {$i < $timeout} {incr i} {
        set status [axi_read $AP_CTRL]
        if {[expr {$status & 0x02}] != 0} {
            return $status
        }
        after 5
    }
    return -1
}

puts "\n问题分析:"
puts "=========================================="
puts "上次测试输出全零的原因:"
puts "  1. 测试范围: Lx=16, Ly=16 (256个点)"
puts "  2. 仅加载4个数据点 (索引0-3)"
puts "  3. 252个未加载点全部为零"
puts "  4. 计算时大部分使用零数据"
puts ""
puts "解决方案:"
puts "  1. 缩小测试范围: Lx=4, Ly=4 (16个点)"
puts "  2. 加载完整范围所有数据"
puts "  3. 验证计算结果是否非零"
puts ""

puts "\n=========================================="
puts "开始诊断测试"
puts "=========================================="

puts "\n--- Step 1: 系统复位 ---"
axi_write $OPERATION $OP_RESET
start_kernel
puts "系统已重置"

puts "\n--- Step 2: 配置参数 (缩小范围) ---"
set Lx 4
set Ly 4
set nkernels 1

axi_write $LX_OFFSET $Lx
axi_write $LY_OFFSET $Ly
axi_write $NKERNELS $nkernels
puts "配置: Lx=$Lx, Ly=$Ly, nkernels=$nkernels"
puts "计算范围: [expr {$Lx*$Ly}]个点"

puts "\n--- Step 3: 加载完整掩模数据 ---"
puts "加载掩模索引0-15..."

for {set i 0} {$i < [expr {$Lx*$Ly}]} {incr i} {
    axi_write $IDX_LOW $i
    axi_write $VAL_IN_REAL 0x3F800000
    axi_write $VAL_IN_IMAG 0x00000000
    axi_write $OPERATION $OP_LOAD_MASK
    start_kernel
}
puts "掩模完整加载完成"

puts "\n--- Step 4: 加载完整核数据 ---"
set kernel_size [expr {$Lx*$Ly}]
puts "加载核0 (索引0-[expr {$kernel_size-1}])..."

for {set i 0} {$i < $kernel_size} {incr i} {
    set idx $i
    axi_write $IDX_LOW $idx
    axi_write $VAL_IN_REAL 0x3F800000
    axi_write $VAL_IN_IMAG 0x00000000
    axi_write $OPERATION $OP_LOAD_KERNELS
    start_kernel
}
puts "核数据完整加载完成"

puts "\n--- Step 5: 加载权重 ---"
axi_write $IDX_LOW 0
axi_write $VAL_IN_REAL 0x3F800000
axi_write $OPERATION $OP_LOAD_SCALES
start_kernel
puts "权重加载完成"

puts "\n--- Step 6: 执行SOCS计算 ---"
axi_write $OPERATION $OP_COMPUTE_SOCS
puts "启动计算..."
set start_time [clock milliseconds]
start_kernel
set elapsed [expr {[clock milliseconds] - $start_time}]
puts "计算完成 (耗时: $elapsed ms)"

puts "\n--- Step 7: 读取输出结果 ---"
axi_write $OPERATION $OP_READ_IMG_OUT

puts ""
puts "输出结果 (完整16个点):"
puts "| idx | real (hex)  | 解析值 |"

set nonzero_count 0

for {set i 0} {$i < [expr {$Lx*$Ly}]} {incr i} {
    axi_write $IDX_LOW $i
    start_kernel
    
    set val_real [axi_read $VAL_OUT_REAL]
    
    # 解析IEEE 754 float
    if {$val_real == 0x3F800000} {
        set parsed "1.0"
        incr nonzero_count
    } elseif {$val_real == 0x40000000} {
        set parsed "2.0"
        incr nonzero_count
    } elseif {$val_real == 0x00000000} {
        set parsed "0.0"
    } else {
        set parsed "?"
        incr nonzero_count
    }
    
    puts "| $i   | 0x[format %08X $val_real] | $parsed  |"
}

puts ""
puts "=========================================="
puts "诊断结果"
puts "=========================================="
puts ""
puts "非零输出数量: $nonzero_count / [expr {$Lx*$Ly}]"
puts ""

if {$nonzero_count > 0} {
    puts "结论: 计算逻辑正常工作"
    puts "  - 加载完整数据后产生非零输出"
    puts "  - 上次全零是因为数据加载不完整"
    puts ""
    puts "下一步: 使用完整16x16数据集测试"
} else {
    puts "结论: 需要进一步检查"
    puts "  - 即使加载完整数据仍输出全零"
    puts "  - 可能是计算逻辑问题"
    puts "  - 建议检查HLS C仿真"
}

puts ""
puts "==========================================