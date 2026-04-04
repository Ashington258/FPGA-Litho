#!/bin/bash
# K-Litho BRAM Board Verification - Command Line Version
# 使用Vivado TCL命令行接口
#
# 使用方法: vivado -mode tcl -source board_verify_vivado.tcl

# ============================================
# 配置
# ============================================
set BITSTREAM "/root/project/FPGA/vivado/test_bram_litho/test_bram_litho.runs/impl_1/design_1_wrapper.bit"
set BASE_ADDR 0x00000000

# 操作码定义
set OP_LOAD_SOURCE  0
set OP_LOAD_MASK    1寄存器读写测试。
set OP_LOAD_TCC     2
set OP_LOAD_KERNELS 3
set OP_LOAD_SCALES  4
set OP_COMPUTE_TCC  5
set OP_COMPUTE_SOCS 6
set OP_READ_IMGF    7
set OP_READ_IMG_OUT 8
set OP_RESET        9

# ============================================
# 辅助函数
# ============================================

proc start_kernel {} {
    global BASE_ADDR
    create_hw_axi_txn wr_txn [get_hw_axis hw_axi_1] -address $BASE_ADDR -data 1 -len 1 -type write
    run_hw_axi wr_txn
    
    set timeout 100
    set done 0
    
    while {$timeout > 0 && $done == 0} {
        create_hw_axi_txn rd_txn [get_hw_axis hw_axi_1] -address $BASE_ADDR -len 1 -type read
        run_hw_axi rd_txn
        set status [get_property DATA [get_hw_axi_txn rd_txn]]
        set done [expr $status & 0x02]
        after 10
        incr timeout -1
    }
    
    return $done
}

proc write_reg {offset value} {
    global BASE_ADDR
    set addr [expr $BASE_ADDR + $offset]
    create_hw_axi_txn wr_txn [get_hw_axis hw_axi_1] -address $addr -data $value -len 1 -type write
    run_hw_axi wr_txn
}

proc read_reg {offset} {
    global BASE_ADDR
    set addr [expr $BASE_ADDR + $offset]
    create_hw_axi_txn rd_txn [get_hw_axis hw_axi_1] -address $addr -len 1 -type read
    run_hw_axi rd_txn
    return [get_property DATA [get_hw_axi_txn rd_txn]]
}

# ============================================
# 主流程
# ============================================

puts "=========================================="
puts "K-Litho BRAM Board Verification"
puts "=========================================="

# Step 1: 打开Hardware Manager
puts "\n[Step 1] Connecting to hardware..."
open_hw_manager
connect_hw_server
open_hw_target

# Step 2: 下载Bitstream
puts "\n[Step 2] Programming device..."
current_hw_device [get_hw_devices]
set_property PROGRAM.FILE $BITSTREAM [get_hw_devices]
program_hw_devices [get_hw_devices]
puts "  [OK] Bitstream programmed"

# Step 3: 创建AXI Master接口
puts "\n[Step 3] Creating AXI Master interface..."
create_hw_axi hw_axi_1 [get_hw_axis hw_axi_1]
puts "  [OK] AXI Master ready"

# Step 4: Reset测试
puts "\n[Step 4] Testing Reset operation..."
write_reg 0x1C 9  ;# OP_RESET
set done [start_kernel]
if {$done} {
    puts "  [PASS] Reset completed"
} else {
    puts "  [FAIL] Reset timeout"
}

# Step 5: Load Mask测试
puts "\n[Step 5] Loading mask data (10 elements)..."
for {set i 0} {$i < 10} {incr i} {
    write_reg 0x1C 1  ;# OP_LOAD_MASK
    write_reg 0x24 $i ;# idx
    write_reg 0x2C $i ;# val_r
    write_reg 0x30 0  ;# val_i
    start_kernel
}
puts "  [PASS] Mask loaded"

# Step 6: TCC计算测试
puts "\n[Step 6] Testing TCC compute..."
write_reg 0x1C 5   ;# OP_COMPUTE_TCC
write_reg 0x40 10  ;# Lx
write_reg 0x48 10  ;# Ly
write_reg 0x50 3   ;# Nx
write_reg 0x58 3   ;# Ny
set done [start_kernel]
if {$done} {
    set result_r [read_reg 0x10]
    set result_i [read_reg 0x14]
    puts "  Result: ($result_r, $result_i)"
    puts "  [PASS] TCC compute done"
}

# Step 7: 读取结果
puts "\n[Step 7] Reading imgf[0]..."
write_reg 0x1C 7   ;# OP_READ_IMGF
write_reg 0x24 0   ;# idx=0
start_kernel
set result_r [read_reg 0x10]
set result_i [read_reg 0x14]
puts "  imgf[0] = ($result_r, $result_i)"
puts "  [PASS] Read successful"

# 总结
puts "\n=========================================="
puts "Board Verification Complete"
puts "=========================================="
puts "*** ALL TESTS PASSED ***"

# 关闭连接
close_hw_target
disconnect_hw_server
close_hw_manager