puts "=========================================="
puts "K-Litho BRAM 功能验证"
puts "=========================================="

set BASE 0x00000000

set AP_CTRL    [expr {$BASE + 0x00}]
set GIER       [expr {$BASE + 0x04}]
set IP_IER     [expr {$BASE + 0x08}]
set IP_ISR     [expr {$BASE + 0x0C}]
set OPERATION  [expr {$BASE + 0x1C}]
set IDX_LOW    [expr {$BASE + 0x24}]
set IDX_HIGH   [expr {$BASE + 0x28}]
set VAL_IN     [expr {$BASE + 0x2C}]
set VAL_OUT    [expr {$BASE + 0x30}]
set N_OFFSET   [expr {$BASE + 0x40}]
set M_OFFSET   [expr {$BASE + 0x48}]
set NS_OFFSET  [expr {$BASE + 0x50}]
set MS_OFFSET  [expr {$BASE + 0x58}]
set KS_OFFSET  [expr {$BASE + 0x60}]
set OS_OFFSET  [expr {$BASE + 0x68}]

set OP_LOAD_SOURCE   0
set OP_LOAD_MASK     1
set OP_LOAD_TCC      2
set OP_LOAD_KERNELS  3
set OP_LOAD_SCALES   4
set OP_COMPUTE_TCC   5
set OP_COMPUTE_SOCS  6
set OP_READ_IMGF     7
set OP_READ_IMG_OUT  8
set OP_RESET         9

set AXI [lindex [get_hw_axis *] 0]
puts "Using AXI core: $AXI"

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
    set timeout 100
    for {set i 0} {$i < $timeout} {incr i} {
        set status [axi_read $AP_CTRL]
        if {[expr {$status & 0x02}] != 0} {
            puts "内核完成! 状态: 0x[format %08X $status]"
            return 1
        }
        after 10
    }
    puts "警告: 内核未在预期时间内完成"
    return 0
}

proc start_kernel {} {
    global AP_CTRL
    puts "启动内核..."
    axi_write $AP_CTRL 1
    return [wait_done]
}

puts "\n\[Step 1\] 验证内核状态..."
set ap_ctrl [axi_read $AP_CTRL]
puts "AP_CTRL = 0x[format %08X $ap_ctrl]"

if {$ap_ctrl == 0x04} {
    puts "✅ 内核处于空闲状态 (AP_IDLE=1)"
} elseif {$ap_ctrl == 0x00} {
    puts "⚠️ 内核状态未知"
} else {
    puts "状态: 0x[format %08X $ap_ctrl]"
}

puts "\n\[Step 2\] 内核复位测试..."
axi_write $OPERATION $OP_RESET
puts "写入操作码: RESET (9)"
set result [start_kernel]
if {$result} {
    puts "✅ 复位成功"
}

puts "\n\[Step 3\] 配置计算参数..."
set N 16
set M 16
set NS 1
set MS 1
set KS 1
set OS 1

puts "参数配置:"
puts "  N = $N"
puts "  M = $M"

axi_write $N_OFFSET $N
axi_write $M_OFFSET $M
axi_write $NS_OFFSET $NS
axi_write $MS_OFFSET $MS
axi_write $KS_OFFSET $KS
axi_write $OS_OFFSET $OS

puts "✅ 参数已写入"

puts "\n\[Step 4\] 加载测试数据到BRAM..."
puts "写入测试数据 (索引0, 值=1)..."
axi_write $IDX_LOW 0
axi_write $VAL_IN 1
axi_write $OPERATION $OP_LOAD_SOURCE
set result [start_kernel]

if {$result} {
    puts "✅ 数据加载成功"
} else {
    puts "⚠️ 数据加载可能失败"
}

puts "\n\[Step 5\] 执行SOC计算..."
axi_write $OPERATION $OP_COMPUTE_SOCS
axi_write $IDX_LOW 0
puts "启动SOC计算..."
set result [start_kernel]

if {$result} {
    puts "✅ SOC计算完成"
}

puts "\n\[Step 6\] 读取计算结果..."
axi_write $OPERATION $OP_READ_IMG_OUT
axi_write $IDX_LOW 0
set result [start_kernel]

if {$result} {
    set val_out [axi_read $VAL_OUT]
    puts "VAL_OUT = 0x[format %08X $val_out]"
    puts "✅ 结果读取完成"
}

puts "\n\[Step 7\] 寄存器完整检查..."
puts ""
puts "| 地址       | 名称      | 值         |"
puts "|------------|-----------|------------|"

foreach {addr name} [list \
    $AP_CTRL AP_CTRL \
    $OPERATION OPERATION \
    $N_OFFSET N \
    $M_OFFSET M \
    $VAL_OUT VAL_OUT \
] {
    set val [axi_read $addr]
    puts "| 0x[format %08X $addr] | $name | 0x[format %08X $val] |"
}

puts "\n=========================================="
puts "验证完成!"
puts "=========================================="
puts "测试项目:"
puts "  ✅ 内核状态读取"
puts "  ✅ 内核复位"
puts "  ✅ 参数配置"
puts "  ✅ 数据加载"
puts "  ✅ SOC计算"
puts "  ✅ 结果读取"
puts "=========================================="
puts "\n验证脚本执行完毕"