puts "=========================================="
puts "K-Litho 参数寄存器调试"
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
set N_OFFSET   [expr {$BASE + 0x40}]
set M_OFFSET   [expr {$BASE + 0x48}]
set SRC_SIZE   [expr {$BASE + 0x78}]
set NKERNELS   [expr {$BASE + 0x7C}]

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
puts "测试参数写入/读回"
puts "=========================================="

puts "\n1. 写入测试参数"
puts "----------------"

# 写入测试值
axi_write $LX_OFFSET 4
axi_write $LY_OFFSET 4
axi_write $N_OFFSET 3
axi_write $M_OFFSET 3
axi_write $NKERNELS 2
axi_write $SRC_SIZE 16

puts "已写入:"
puts "  LX = 4"
puts "  LY = 4"
puts "  N = 3"
puts "  M = 3"
puts "  NKERNELS = 2"
puts "  SRC_SIZE = 16"

puts "\n2. 读回参数寄存器"
puts "-----------------"

set lx_val [axi_read $LX_OFFSET]
set ly_val [axi_read $LY_OFFSET]
set n_val [axi_read $N_OFFSET]
set m_val [axi_read $M_OFFSET]
set nkernels_val [axi_read $NKERNELS]
set srcsize_val [axi_read $SRC_SIZE]

puts "读回值:"
puts "  LX_OFFSET   (0x70) = $lx_val (预期: 4)"
puts "  LY_OFFSET   (0x74) = $ly_val (预期: 4)"
puts "  N_OFFSET    (0x40) = $n_val (预期: 3)"
puts "  M_OFFSET    (0x48) = $m_val (预期: 3)"
puts "  NKERNELS    (0x7C) = $nkernels_val (预期: 2)"
puts "  SRC_SIZE    (0x78) = $srcsize_val (预期: 16)"

puts "\n3. 验证结果"
puts "------------"

set pass_count 0
set total 6

if {$lx_val == 4} { incr pass_count }
if {$ly_val == 4} { incr pass_count }
if {$n_val == 3} { incr pass_count }
if {$m_val == 3} { incr pass_count }
if {$nkernels_val == 2} { incr pass_count }
if {$srcsize_val == 16} { incr pass_count }

puts "验证: $pass_count / $total 参数正确"

if {$pass_count == $total} {
    puts ""
    puts "结论: 参数寄存器工作正常"
    puts "  问题可能在计算逻辑内部"
} else {
    puts ""
    puts "结论: 参数寄存器写入失败"
    puts "  可能原因:"
    puts "    1. 寄存器地址偏移错误"
    puts "    2. AXI Lite接口问题"
    puts "    3. HLS生成的寄存器映射不匹配"
}

puts "\n4. 检查BRAM数据 (读取前4个点)"
puts "------------------------------"

# 执行READ_IMG_OUT操作，不经过计算
set OP_READ_IMG_OUT 8
set AP_CTRL_ADDR [expr {$BASE + 0x00}]

puts "读取img_out_bram前4个点:"
for {set i 0} {$i < 4} {incr i} {
    axi_write $OPERATION $OP_READ_IMG_OUT
    axi_write $IDX_LOW $i
    axi_write $AP_CTRL 1
    after 10
    
    set val_real [axi_read $VAL_OUT_REAL]
    puts "  idx=$i: 0x[format %08X $val_real]"
}

puts "\n=========================================="
puts "调试完成"
puts "=========================================="