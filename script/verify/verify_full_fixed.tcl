puts "=========================================="
puts "K-Litho 完整功能验证 (地址修正版)"
puts "=========================================="

set BASE 0x00000000

# =====================
# 寄存器地址定义 (根据HLS生成的_s_axi.v修正)
# =====================
set AP_CTRL    [expr {$BASE + 0x00}]
set GIE        [expr {$BASE + 0x04}]
set IP_IER     [expr {$BASE + 0x08}]
set IP_ISR     [expr {$BASE + 0x0C}]
set OPERATION  [expr {$BASE + 0x1C}]
set IDX        [expr {$BASE + 0x24}]
set VAL_R_REAL [expr {$BASE + 0x2C}]
set VAL_R_IMAG [expr {$BASE + 0x30}]
set LX         [expr {$BASE + 0x40}]  ;# 修正! (原 0x70)
set LY         [expr {$BASE + 0x48}]  ;# 修正! (原 0x74)
set NX         [expr {$BASE + 0x50}]  ;# 修正! (原 0x40)
set NY         [expr {$BASE + 0x58}]  ;# 修正! (原 0x48)
set SRCSIZE    [expr {$BASE + 0x60}]  ;# 修正! (原 0x78)
set NKERNELS   [expr {$BASE + 0x68}]  ;# 修正! (原 0x7C)

# 操作码
set OP_RESET        9
set OP_LOAD_SOURCE  0
set OP_COMPUTE_SOCS 6
set OP_READ_IMG_OUT 8

set AXI [lindex [get_hw_axis *] 0]

# =====================
# AXI 操作函数
# =====================
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

proc wait_done {} {
    global AP_CTRL
    set timeout 0
    while {$timeout < 100000} {
        set ctrl [axi_read $AP_CTRL]
        if {($ctrl & 0x02) == 0} {
            return 1
        }
        incr timeout
    }
    return 0
}

proc start_operation {} {
    global AP_CTRL
    axi_write $AP_CTRL 0x01
}

proc get_status {} {
    global AP_CTRL
    return [axi_read $AP_CTRL]
}

# =====================
# 测试数据 (简单4x4模式)
# =====================
set TEST_SIZE 16
set N_KERNELS 1
set NX_VAL 4
set NY_VAL 4

# 测试数据: 全部填充 1.0
set test_data [list]
for {set i 0} {$i < $TEST_SIZE} {incr i} {
    lappend test_data 1.0 0.0  ;# real, imag
}

# =====================
# 开始测试
# =====================
puts "\n=========================================="
puts "第一阶段: 参数设置 (修正后地址)"
puts "=========================================="

puts "\n写入参数..."
puts "  NX = $NX_VAL, NY = $NY_VAL"
puts "  N_KERNELS = $N_KERNELS"

axi_write $NX $NX_VAL
axi_write $NY $NY_VAL
axi_write $NKERNELS $N_KERNELS
axi_write $LX $NX_VAL
axi_write $LY $NY_VAL
axi_write $SRCSIZE $TEST_SIZE

puts "\n读回验证..."
set nx_r [axi_read $NX]
set ny_r [axi_read $NY]
set nk_r [axi_read $NKERNELS]
puts "  NX = $nx_r (预期: $NX_VAL)"
puts "  NY = $ny_r (预期: $NY_VAL)"
puts "  NKERNELS = $nk_r (预期: $N_KERNELS)"

if {$nx_r != $NX_VAL || $ny_r != $NY_VAL || $nk_r != $N_KERNELS} {
    puts "\n❌ 参数设置失败!"
    exit 1
}
puts "✅ 参数设置成功"

puts "\n=========================================="
puts "第二阶段: 复位系统"
puts "=========================================="

axi_write $OPERATION $OP_RESET
start_operation
wait_done
set status [get_status]
puts "  复位状态: 0x[format %08X $status]"
puts "✅ 复位完成"

puts "\n=========================================="
puts "第三阶段: 加载源数据"
puts "=========================================="

set val_count 0
foreach {real imag} $test_data {
    set idx [expr {$val_count * 2}]
    
    # 写入索引
    axi_write $IDX $val_count
    
    # 写入实部和虚部
    set real_fixed [expr {int($real * 32768)}]
    set imag_fixed [expr {int($imag * 32768)}]
    axi_write $VAL_R_REAL $real_fixed
    axi_write $VAL_R_IMAG $imag_fixed
    
    # 执行写入操作
    axi_write $OPERATION $OP_LOAD_SOURCE
    start_operation
    wait_done
    
    incr val_count
    if {$val_count % 4 == 0} {
        puts "  已加载 $val_count / $TEST_SIZE 点"
    }
}

puts "✅ 加载 $val_count 个数据点完成"

puts "\n=========================================="
puts "第四阶段: SOCS 计算"
puts "=========================================="

puts "  开始 SOCS 计算..."
set start_time [clock milliseconds]

axi_write $OPERATION $OP_COMPUTE_SOCS
start_operation

if {[wait_done]} {
    set end_time [clock milliseconds]
    set elapsed [expr {$end_time - $start_time}]
    puts "  计算耗时: ${elapsed}ms"
    puts "✅ SOCS 计算完成"
} else {
    puts "❌ 计算超时!"
    exit 1
}

puts "\n=========================================="
puts "第五阶段: 读取输出"
puts "=========================================="

puts "  读取前10个输出点..."

set output_values [list]
for {set i 0} {$i < 10} {incr i} {
    axi_write $IDX $i
    axi_write $OPERATION $OP_READ_IMG_OUT
    start_operation
    wait_done
    
    set real_val [axi_read $VAL_R_REAL]
    set imag_val [axi_read $VAL_R_IMAG]
    
    # 转换为浮点
    set real_float [expr {$real_val / 32768.0}]
    set imag_float [expr {$imag_val / 32768.0}]
    
    lappend output_values [list $real_float $imag_float]
    puts "  点 $i: real = $real_float, imag = $imag_float (raw: $real_val, $imag_val)"
}

# 检查输出是否全为零
set all_zero 1
foreach val $output_values {
    set real [lindex $val 0]
    set imag [lindex $val 1]
    if {$real != 0 || $imag != 0} {
        set all_zero 0
        break
    }
}

puts "\n=========================================="
puts "测试结果总结"
puts "=========================================="

if {$all_zero} {
    puts "❌ 输出仍然全为零!"
    puts "  这表明除了地址问题外，可能还有其他问题"
} else {
    puts "✅ 输出非零!"
    puts "  地址修正有效，系统开始正常工作"
    
    # 计算输出统计
    set max_real 0
    set min_real 0
    foreach val $output_values {
        set r [lindex $val 0]
        if {$r > $max_real} { set max_real $r }
        if {$r < $min_real} { set min_real $r }
    }
    puts "  实部范围: [$min_real, $max_real]"
}

puts "\n完成时间: [clock format [clock seconds]]"