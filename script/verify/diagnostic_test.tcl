puts "=========================================="
puts "K-Litho 数据加载诊断"
puts "=========================================="
puts "分析为什么输出全零"
puts ""

set BASE 0x00000000

# 寄存器地址
set AP_CTRL    [expr {$BASE + 0x00}]
set OPERATION  [expr {$BASE + 0x1C}]
set IDX_LOW    [expr {$BASE + 0x24}]
set VAL_IN_REAL [expr {$BASE + 0x2C}]
set VAL_IN_IMAG [expr {$BASE + 0x30}]
set VAL_OUT_REAL [expr {$BASE + 0x34}]
set VAL_OUT_IMAG [expr {$BASE + 0x38}]
set LX_OFFSET  [expr {$BASE + 0x70}]
set LY_OFFSET  [expr {$BASE + 0x74}]
set N_OFFSET   [expr {$BASE + 0x40}]
set M_OFFSET   [expr {$BASE + 0x48}]
set SRC_SIZE   [expr {$BASE + 0x78}]
set NKERNELS   [expr {$BASE + 0x7C}]

set OP_LOAD_SOURCE   0
set OP_LOAD_MASK     1
set OP_LOAD_KERNELS  3
set OP_READ_IMG_OUT  8
set OP_RESET         9

set AXI [lindex [get_hw_axis *] 0]

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

puts "\n=========================================="
puts "问题分析"
puts "=========================================="

puts "\n【问题根源】: 数据加载不完整"
puts ""
puts "HLS代码分析:"
puts "  KERNEL_MASK_SOCS_LOOP 循环:"
puts "    for int i = 0 to Lx*Ly"
puts "        kernel_idx = i % 225"
puts "        kernel_val = kernels_bram at (kernel_start + kernel_idx)"
puts ""
puts "当前参数配置:"
puts "  Lx = 16, Ly = 16"
puts "  循环范围 = 0 到 255 (共256次)"
puts ""
puts "已加载的数据:"
puts "  光源: 索引136-139 (仅4个点)"
puts "  掩模: 索引0-3 (仅4个点)"
puts "  核数据: 每核索引0-3 (仅4个点/核)"
puts ""
puts "未加载的数据:"
puts "  索引4-255: 全部为零（BRAM初始状态）"
puts ""
puts "计算结果:"
puts "  i=0-3: 使用已加载的数据 (索引0-3)"
puts "  i=4-255: 使用未加载的零数据"
puts "  -> 大部分计算结果为零"
puts "  -> 平方幅度后仍为零"
puts ""

puts "\n=========================================="
puts "验证测试: 加载完整掩模数据"
puts "=========================================="

puts "\n--- 重置系统 ---"
axi_write $OPERATION $OP_RESET
axi_write $AP_CTRL 1
after 50
puts "系统已重置"

puts "\n--- 配置参数 ---"
set Lx 4
set Ly 4
axi_write $LX_OFFSET $Lx
axi_write $LY_OFFSET $Ly
puts "配置: Lx=$Lx, Ly=$Ly (缩小测试范围)"

puts "\n--- 加载完整掩模数据 (4x4=16个点) ---"
puts "加载掩模索引0-15..."

for {set i 0} {$i < $Lx * $Ly} {incr i} {
    axi_write $IDX_LOW $i
    axi_write $VAL_IN_REAL 0x3F800000  ;# 1.0
    axi_write $VAL_IN_IMAG 0x00000000
    axi_write $OPERATION $OP_LOAD_MASK
    axi_write $AP_CTRL 1
    after 5
    if {$i % 4 == 0} {
        puts "  已加载: 索引 $i"
    }
}
puts "✅ 掩模完整加载 ($Lx x $Ly = [expr {$Lx*$Ly}]个点)"

puts "\n--- 加载完整核数据 (每核16个点) ---"
set nkernels 1
set kernel_size [expr {$Lx * $Ly}]
axi_write $NKERNELS $nkernels

for {set k 0} {$k < $nkernels} {incr k} {
    puts "加载核 $k (索引0-[expr {$kernel_size-1}])..."
    for {set i 0} {$i < $kernel_size} {incr i} {
        set idx [expr {$k * 225 + $i}]
        axi_write $IDX_LOW $idx
        axi_write $VAL_IN_REAL 0x3F800000  ;# 1.0
        axi_write $VAL_IN_IMAG 0x00000000
        axi_write $OPERATION $OP_LOAD_KERNELS
        axi_write $AP_CTRL 1
        after 5
    }
    puts "  ✅ 核 $k 加载完成"
}

puts "\n--- 加载权重 ---"
axi_write $IDX_LOW 0
axi_write $VAL_IN_REAL 0x3F800000  ;# 1.0
axi_write $OPERATION [set OP_LOAD_SCALES 4]
axi_write $AP_CTRL 1
after 5
puts "✅ 权重加载完成"

puts "\n--- 执行SOCS计算 ---"
axi_write $OPERATION 6  ;# OP_COMPUTE_SOCS
set start_time [clock milliseconds]
axi_write $AP_CTRL 1
after 20
set elapsed [expr {[clock milliseconds] - $start_time}]
puts "✅ SOCS计算完成 (耗时: $elapsed ms)"

puts "\n--- 读取结果 ---"
axi_write $OPERATION $OP_READ_IMG_OUT

puts "\n输出结果 (完整范围):"
puts "| idx | real (hex)  | 解析值    |"

for {set i 0} {$i < $Lx * $Ly} {incr i} {
    axi_write $IDX_LOW $i
    axi_write $AP_CTRL 1
    after 5
    
    set val_real [axi_read $VAL_OUT_REAL]
    
    # 解析IEEE 754 float
    if {$val_real == 0x3F800000} {
        set parsed "1.0"
    } elseif {$val_real == 0x00000000} {
        set parsed "0.0"
    } elseif {$val_real == 0x40000000} {
        set parsed "2.0"
    } else {
        set parsed "?"
    }
    
    puts "| $i   | 0x[format %08X $val_real] | $parsed    |"
}

puts "\n=========================================="
puts "诊断结果"
puts "=========================================="
puts ""
puts "验证方法:"
puts "  1. 缩小测试范围 (Lx=4, Ly=4)"
puts "  2. 加载完整范围内的所有数据"
puts "  3. 验证计算结果"
puts ""
puts "预期结果:"
puts "  如果加载完整数据，输出应该为非零值"
puts "  输入=1.0, 核=1.0, 权重=1.0"
puts "  → product_acc = 1.0"
puts "  → mag_sq = 1.0^2 + 0^2 = 1.0"
puts ""
puts "=========================================="

puts "\n下一步建议:"
puts "  1. 如果输出仍为零 → 检查HLS计算逻辑"
puts "  2. 如果输出为非零 → 问题确认: 数据不完整"
puts "  3. 使用完整数据集重新测试"