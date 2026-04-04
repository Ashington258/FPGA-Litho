puts "=========================================="
puts "K-Litho 参数寄存器调试 (地址修正版)"
puts "=========================================="

set BASE 0x00000000

# 正确的寄存器地址 (根据HLS生成的_s_axi.v)
set AP_CTRL    [expr {$BASE + 0x00}]
set OPERATION  [expr {$BASE + 0x1C}]
set IDX        [expr {$BASE + 0x24}]
set VAL_R_REAL [expr {$BASE + 0x2C}]
set VAL_R_IMAG [expr {$BASE + 0x30}]
set LX         [expr {$BASE + 0x40}]  ;# 修正!
set LY         [expr {$BASE + 0x48}]  ;# 修正!
set NX         [expr {$BASE + 0x50}]  ;# 修正!
set NY         [expr {$BASE + 0x58}]  ;# 修正!
set SRCSIZE    [expr {$BASE + 0x60}]  ;# 修正!
set NKERNELS   [expr {$BASE + 0x68}]  ;# 修正!

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
puts "正确的寄存器地址映射 (HLS生成)"
puts "=========================================="
puts ""
puts "ADDR_AP_CTRL    = 0x00"
puts "ADDR_OPERATION  = 0x1C"
puts "ADDR_IDX        = 0x24"
puts "ADDR_VAL_R      = 0x2C-0x30"
puts "ADDR_LX         = 0x40 (之前错误: 0x70)"
puts "ADDR_LY         = 0x48 (之前错误: 0x74)"
puts "ADDR_NX         = 0x50 (之前错误: 0x40)"
puts "ADDR_NY         = 0x58 (之前错误: 0x48)"
puts "ADDR_SRCSIZE    = 0x60 (之前错误: 0x78)"
puts "ADDR_NKERNELS   = 0x68 (之前错误: 0x7C)"
puts ""

puts "\n=========================================="
puts "测试参数写入/读回 (修正后)"
puts "=========================================="

puts "\n1. 写入测试参数"
puts "----------------"

axi_write $LX 4
axi_write $LY 4
axi_write $NX 3
axi_write $NY 3
axi_write $NKERNELS 2
axi_write $SRCSIZE 16

puts "已写入:"
puts "  LX = 4  -> 0x40"
puts "  LY = 4  -> 0x48"
puts "  NX = 3  -> 0x50"
puts "  NY = 3  -> 0x58"
puts "  NKERNELS = 2  -> 0x68"
puts "  SRCSIZE = 16  -> 0x60"

puts "\n2. 读回参数寄存器"
puts "-----------------"

set lx_val [axi_read $LX]
set ly_val [axi_read $LY]
set nx_val [axi_read $NX]
set ny_val [axi_read $NY]
set nkernels_val [axi_read $NKERNELS]
set srcsize_val [axi_read $SRCSIZE]

puts "读回值:"
puts "  LX (0x40) = $lx_val (预期: 4)"
puts "  LY (0x48) = $ly_val (预期: 4)"
puts "  NX (0x50) = $nx_val (预期: 3)"
puts "  NY (0x58) = $ny_val (预期: 3)"
puts "  NKERNELS (0x68) = $nkernels_val (预期: 2)"
puts "  SRCSIZE (0x60) = $srcsize_val (预期: 16)"

puts "\n3. 验证结果"
puts "------------"

set pass_count 0
if {$lx_val == 4} { incr pass_count }
if {$ly_val == 4} { incr pass_count }
if {$nx_val == 3} { incr pass_count }
if {$ny_val == 3} { incr pass_count }
if {$nkernels_val == 2} { incr pass_count }
if {$srcsize_val == 16} { incr pass_count }

puts "验证: $pass_count / 6 参数正确"

if {$pass_count == 6} {
    puts ""
    puts "✅ 结论: 参数寄存器工作正常"
    puts "  地址映射已修正"
    puts "  可以进行计算测试"
} else {
    puts ""
    puts "❌ 结论: 参数寄存器仍然有问题"
}

puts "\n=========================================="