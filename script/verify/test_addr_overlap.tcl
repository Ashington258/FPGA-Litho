puts "=========================================="
puts "地址重叠测试"
puts "=========================================="

set BASE 0x00000000
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
puts "测试: 逐字节写入和读取"
puts "=========================================="

puts "\n地址 0x40-0x6F 区域扫描:"
puts "-----------------------------------"

for {set addr 0x40} {$addr <= 0x6F} {incr addr 4} {
    set val [axi_read [expr {$BASE + $addr}]]
    puts "  [format 0x%02X $addr] = [format 0x%08X $val] ([expr {$val}])"
}

puts "\n=========================================="
puts "测试: 写入后立即读回每个地址"
puts "=========================================="

foreach {name addr testval} {
    "LX_DATA"    0x40  100
    "LX_CTRL"    0x44  1
    "LY_DATA"    0x48  200
    "LY_CTRL"    0x4C  1
    "NX_DATA"    0x50  300
    "NX_CTRL"    0x54  1
    "NY_DATA"    0x58  400
    "NY_CTRL"    0x5C  1
    "SRCSIZE"    0x60  500
    "SRCCTRL"    0x64  1
    "NKERNELS"   0x68  600
    "NKCTRL"     0x6C  1
} {
    puts "\n测试 $name (地址 $addr):"
    
    # 写入
    axi_write [expr {$BASE + $addr}] $testval
    puts "  写入: $testval"
    
    # 读回
    set readval [axi_read [expr {$BASE + $addr}]]
    puts "  读回: $readval"
    
    # 检查相邻地址是否受影响
    puts "  相邻地址状态:"
    set prev_addr [expr {$addr - 4}]
    set next_addr [expr {$addr + 4}]
    if {$prev_addr >= 0x40} {
        set prev_val [axi_read [expr {$BASE + $prev_addr}]]
        puts "    [format 0x%02X $prev_addr] = [format 0x%08X $prev_val]"
    }
    if {$next_addr <= 0x6C} {
        set next_val [axi_read [expr {$BASE + $next_addr}]]
        puts "    [format 0x%02X $next_addr] = [format 0x%08X $next_val]"
    }
}

puts "\n=========================================="
puts "最终状态扫描"
puts "=========================================="

puts "\n地址 0x40-0x6F 区域最终状态:"
puts "-----------------------------------"

for {set addr 0x40} {$addr <= 0x6F} {incr addr 4} {
    set val [axi_read [expr {$BASE + $addr}]]
    puts "  [format 0x%02X $addr] = [format 0x%08X $val]"
}

puts "\n=========================================="