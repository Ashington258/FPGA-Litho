puts "=========================================="
puts "K-Litho 范围内参数测试"
puts "=========================================="
puts ""
puts "当前硬件参数约束 (HLS综合时确定):"
puts "  NX: 最大 3 (BRAM_MAX_NX_TCC = 3)"
puts "  NY: 最大 15"
puts "  LX, LY: 最大 64"
puts "  NKERNELS: 最大 8"
puts ""
puts "测试策略: 使用约束范围内的参数值"
puts "=========================================="

set BASE 0x00000000

# 寄存器地址
set AP_CTRL    [expr {$BASE + 0x00}]
set OPERATION  [expr {$BASE + 0x1C}]
set IDX        [expr {$BASE + 0x24}]
set VAL_R_REAL [expr {$BASE + 0x2C}]
set VAL_R_IMAG [expr {$BASE + 0x30}]
set LX         [expr {$BASE + 0x40}]
set LY         [expr {$BASE + 0x48}]
set NX         [expr {$BASE + 0x50}]
set NY         [expr {$BASE + 0x58}]
set SRCSIZE    [expr {$BASE + 0x60}]
set NKERNELS   [expr {$BASE + 0x68}]

# 操作码
set OP_RESET        9
set OP_LOAD_SOURCE  0
set OP_LOAD_MASK    1
set OP_COMPUTE_SOCS 6
set OP_READ_IMG_OUT 8

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

proc wait_done {} {
    global AP_CTRL
    set timeout 0
    while {$timeout < 10000} {
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

puts "\n=========================================="
puts "第一阶段: 使用范围内参数"
puts "=========================================="

# 使用硬件支持的范围内参数
set NX_VAL 2       ;# 范围内 (最大3)
set NY_VAL 2       ;# 范围内 (最大15)
set LX_VAL 4       ;# 范围内 (最大64)
set LY_VAL 4       ;# 范围内 (最大64)
set NKERNELS_VAL 1 ;# 范围内 (最大8)
set SRC_SIZE_VAL 4 ;# 小范围测试

puts "写入参数 (范围内):"
puts "  NX = $NX_VAL"
puts "  NY = $NY_VAL"
puts "  LX = $LX_VAL"
puts "  LY = $LY_VAL"
puts "  NKERNELS = $NKERNELS_VAL"
puts "  SRCSIZE = $SRC_SIZE_VAL"

axi_write $NX $NX_VAL
axi_write $NY $NY_VAL
axi_write $LX $LX_VAL
axi_write $LY $LY_VAL
axi_write $NKERNELS $NKERNELS_VAL
axi_write $SRCSIZE $SRC_SIZE_VAL

puts "\n读回验证:"
puts "  NX = [axi_read $NX] (预期: $NX_VAL)"
puts "  NY = [axi_read $NY] (预期: $NY_VAL)"
puts "  LX = [axi_read $LX] (预期: $LX_VAL)"
puts "  LY = [axi_read $LY] (预期: $LY_VAL)"
puts "  NKERNELS = [axi_read $NKERNELS] (预期: $NKERNELS_VAL)"
puts "  SRCSIZE = [axi_read $SRCSIZE] (预期: $SRC_SIZE_VAL)"

puts "\n=========================================="
puts "第二阶段: 复位系统"
puts "=========================================="

axi_write $OPERATION $OP_RESET
start_operation
wait_done
puts "✅ 复位完成"

puts "\n=========================================="
puts "第三阶段: 加载测试数据"
puts "=========================================="

# 加载4个简单数据点 (LX*LY = 4*4 = 16 点)
set total_points [expr {$LX_VAL * $LY_VAL}]
puts "加载 $total_points 个数据点..."

for {set i 0} {$i < $total_points} {incr i} {
    axi_write $IDX $i
    
    # 使用简单的测试值 (1.0, 0.0) 定点表示
    set real_fixed [expr {int(1.0 * 32768)}]  ;# 0x8000
    set imag_fixed 0
    
    axi_write $VAL_R_REAL $real_fixed
    axi_write $VAL_R_IMAG $imag_fixed
    
    axi_write $OPERATION $OP_LOAD_MASK
    start_operation
    wait_done
    
    if {$i % 4 == 0} {
        puts "  已加载 $i / $total_points 点"
    }
}
puts "✅ 数据加载完成"

puts "\n=========================================="
puts "第四阶段: SOCS 计算"
puts "=========================================="

puts "开始 SOCS 计算..."
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
}

puts "\n=========================================="
puts "第五阶段: 读取输出"
puts "=========================================="

puts "读取前8个输出点..."
set non_zero_count 0

for {set i 0} {$i < 8} {incr i} {
    axi_write $IDX $i
    axi_write $OPERATION $OP_READ_IMG_OUT
    start_operation
    wait_done
    
    set real_val [axi_read $VAL_R_REAL]
    set imag_val [axi_read $VAL_R_IMAG]
    
    set real_float [expr {$real_val / 32768.0}]
    set imag_float [expr {$imag_val / 32768.0}]
    
    puts "  点 $i: real = $real_float (raw: $real_val)"
    
    if {$real_val != 0} {
        incr non_zero_count
    }
}

puts "\n=========================================="
puts "结果总结"
puts "=========================================="

if {$non_zero_count > 0} {
    puts "✅ 成功! $non_zero_count / 8 输出点非零"
    puts "  系统使用范围内参数工作正常"
    puts ""
    puts "下一步: 重新综合HLS IP核增大参数范围"
    puts "        修改 hls_litho_system_bram.h 中的 BRAM_MAX_NX_TCC 等常量"
} else {
    puts "❌ 输出仍然全为零"
    puts "  需要进一步调查问题"
}

puts "\n完成时间: [clock format [clock seconds]]"