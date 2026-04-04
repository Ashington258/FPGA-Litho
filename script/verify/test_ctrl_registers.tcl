puts "=========================================="
puts "K-Litho CTRL寄存器测试"
puts "=========================================="

set BASE 0x00000000

# 完整的寄存器地址 (包括CTRL)
set AP_CTRL    [expr {$BASE + 0x00}]
set GIE        [expr {$BASE + 0x04}]
set IER        [expr {$BASE + 0x08}]
set ISR        [expr {$BASE + 0x0C}]

set OPERATION_DATA [expr {$BASE + 0x1C}]
set OPERATION_CTRL [expr {$BASE + 0x20}]

set IDX_DATA [expr {$BASE + 0x24}]
set IDX_CTRL [expr {$BASE + 0x28}]

set VAL_R_DATA0 [expr {$BASE + 0x2C}]
set VAL_R_DATA1 [expr {$BASE + 0x30}]
set VAL_R_CTRL  [expr {$BASE + 0x34}]

set MODE_DATA [expr {$BASE + 0x38}]
set MODE_CTRL [expr {$BASE + 0x3C}]

set LX_DATA  [expr {$BASE + 0x40}]
set LX_CTRL  [expr {$BASE + 0x44}]
set LY_DATA  [expr {$BASE + 0x48}]
set LY_CTRL  [expr {$BASE + 0x4C}]
set NX_DATA  [expr {$BASE + 0x50}]
set NX_CTRL  [expr {$BASE + 0x54}]
set NY_DATA  [expr {$BASE + 0x58}]
set NY_CTRL  [expr {$BASE + 0x5C}]
set SRCSIZE_DATA [expr {$BASE + 0x60}]
set SRCSIZE_CTRL [expr {$BASE + 0x64}]
set NKERNELS_DATA [expr {$BASE + 0x68}]
set NKERNELS_CTRL [expr {$BASE + 0x6C}]

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
puts "测试1: 只写DATA寄存器"
puts "=========================================="

axi_write $LX_DATA 4
axi_write $LY_DATA 4
axi_write $NX_DATA 3
axi_write $NY_DATA 3
axi_write $NKERNELS_DATA 2
axi_write $SRCSIZE_DATA 16

puts "已写入DATA寄存器:"
puts "  LX_DATA = 4"
puts "  LY_DATA = 4"
puts "  NX_DATA = 3"
puts "  NY_DATA = 3"
puts "  NKERNELS_DATA = 2"
puts "  SRCSIZE_DATA = 16"

puts "\n读回结果:"
puts "  LX = [axi_read $LX_DATA]"
puts "  LY = [axi_read $LY_DATA]"
puts "  NX = [axi_read $NX_DATA]"
puts "  NY = [axi_read $NY_DATA]"
puts "  NKERNELS = [axi_read $NKERNELS_DATA]"
puts "  SRCSIZE = [axi_read $SRCSIZE_DATA]"

puts "\n=========================================="
puts "测试2: 写DATA + CTRL寄存器"
puts "=========================================="

# 写入DATA后，也写入CTRL寄存器（值=1表示自动启动）
axi_write $LX_DATA 5
axi_write $LX_CTRL 1

axi_write $LY_DATA 6
axi_write $LY_CTRL 1

axi_write $NX_DATA 7
axi_write $NX_CTRL 1

axi_write $NY_DATA 8
axi_write $NY_CTRL 1

axi_write $NKERNELS_DATA 3
axi_write $NKERNELS_CTRL 1

axi_write $SRCSIZE_DATA 20
axi_write $SRCSIZE_CTRL 1

puts "已写入DATA + CTRL寄存器:"
puts "  LX_DATA = 5, LX_CTRL = 1"
puts "  LY_DATA = 6, LY_CTRL = 1"
puts "  NX_DATA = 7, NX_CTRL = 1"
puts "  NY_DATA = 8, NY_CTRL = 1"
puts "  NKERNELS_DATA = 3, NKERNELS_CTRL = 1"
puts "  SRCSIZE_DATA = 20, SRCSIZE_CTRL = 1"

puts "\n读回结果:"
puts "  LX = [axi_read $LX_DATA]"
puts "  LY = [axi_read $LY_DATA]"
puts "  NX = [axi_read $NX_DATA]"
puts "  NY = [axi_read $NY_DATA]"
puts "  NKERNELS = [axi_read $NKERNELS_DATA]"
puts "  SRCSIZE = [axi_read $SRCSIZE_DATA]"

puts "\n=========================================="
puts "测试3: 检查CTRL寄存器值"
puts "=========================================="

puts "CTRL寄存器读回:"
puts "  LX_CTRL = [axi_read $LX_CTRL]"
puts "  LY_CTRL = [axi_read $LY_CTRL]"
puts "  NX_CTRL = [axi_read $NX_CTRL]"
puts "  NY_CTRL = [axi_read $NY_CTRL]"
puts "  NKERNELS_CTRL = [axi_read $NKERNELS_CTRL]"
puts "  SRCSIZE_CTRL = [axi_read $SRCSIZE_CTRL]"

puts "\n=========================================="
puts "测试4: GIE/IER寄存器设置"
puts "=========================================="

# 设置全局中断和中断使能
axi_write $GIE 1
axi_write $IER 1

puts "已写入GIE=1, IER=1"

puts "\n读回GIE/IER:"
puts "  GIE = [axi_read $GIE]"
puts "  IER = [axi_read $IER]"

puts "\n=========================================="
puts "测试5: 最终参数验证"
puts "=========================================="

# 再次设置参数
axi_write $LX_DATA 4
axi_write $LY_DATA 4
axi_write $NX_DATA 3
axi_write $NY_DATA 3
axi_write $NKERNELS_DATA 2
axi_write $SRCSIZE_DATA 16

puts "\n最终读回:"
puts "  LX = [axi_read $LX_DATA] (预期: 4)"
puts "  LY = [axi_read $LY_DATA] (预期: 4)"
puts "  NX = [axi_read $NX_DATA] (预期: 3)"
puts "  NY = [axi_read $NY_DATA] (预期: 3)"
puts "  NKERNELS = [axi_read $NKERNELS_DATA] (预期: 2)"
puts "  SRCSIZE = [axi_read $SRCSIZE_DATA] (预期: 16)"

puts "\n=========================================="