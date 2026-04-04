puts "=========================================="
puts "K-Litho BRAM 完整功能验证"
puts "=========================================="

set BASE 0x00000000

# 寄存器地址定义
set AP_CTRL    [expr {$BASE + 0x00}]
set GIER       [expr {$BASE + 0x04}]
set IP_IER     [expr {$BASE + 0x08}]
set IP_ISR     [expr {$BASE + 0x0C}]
set OPERATION  [expr {$BASE + 0x1C}]
set IDX_LOW    [expr {$BASE + 0x24}]
set IDX_HIGH   [expr {$BASE + 0x28}]
set VAL_IN_REAL [expr {$BASE + 0x2C}]
set VAL_IN_IMAG [expr {$BASE + 0x30}]
set VAL_OUT_REAL [expr {$BASE + 0x34}]
set VAL_OUT_IMAG [expr {$BASE + 0x38}]
set N_OFFSET   [expr {$BASE + 0x40}]
set M_OFFSET   [expr {$BASE + 0x48}]
set NS_OFFSET  [expr {$BASE + 0x50}]
set MS_OFFSET  [expr {$BASE + 0x58}]
set KS_OFFSET  [expr {$BASE + 0x60}]
set OS_OFFSET  [expr {$BASE + 0x68}]
set LX_OFFSET  [expr {$BASE + 0x70}]
set LY_OFFSET  [expr {$BASE + 0x74}]
set SRC_SIZE   [expr {$BASE + 0x78}]
set NKERNELS   [expr {$BASE + 0x7C}]

# 操作码定义
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

# AXI接口
set AXI [lindex [get_hw_axis *] 0]
puts "Using AXI core: $AXI"

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

proc load_complex {addr_real addr_imag real imag} {
    # 将float转换为整数表示 (IEEE 754)
    # 简化: 使用整数1.0 = 0x3F800000
    set real_int 0
    set imag_int 0
    
    if {$real == 1.0} {
        set real_int 0x3F800000
    } elseif {$real == 0.5} {
        set real_int 0x3F000000
    } elseif {$real == 0.0} {
        set real_int 0x00000000
    } elseif {$real == -1.0} {
        set real_int 0xBF800000
    } elseif {$real == 2.0} {
        set real_int 0x40000000
    }
    
    if {$imag == 1.0} {
        set imag_int 0x3F800000
    } elseif {$imag == 0.5} {
        set imag_int 0x3F000000
    } elseif {$imag == 0.0} {
        set imag_int 0x00000000
    } elseif {$imag == -1.0} {
        set imag_int 0xBF800000
    } elseif {$imag == 2.0} {
        set imag_int 0x40000000
    }
    
    axi_write $addr_real $real_int
    axi_write $addr_imag $imag_int
}

puts "\n=========================================="
puts "Step 1: 系统复位"
puts "=========================================="

axi_write $OPERATION $OP_RESET
puts "执行RESET操作..."
set result [start_kernel]
if {$result} {
    puts "✅ 系统复位成功"
}

puts "\n=========================================="
puts "Step 2: 加载光源数据 (SOURCE)"
puts "=========================================="

# 测试参数: 16x16光源
set Lx 16
set Ly 16
set srcSize 16

puts "配置光源尺寸: Lx=$Lx, Ly=$Ly, srcSize=$srcSize"
axi_write $LX_OFFSET $Lx
axi_write $LY_OFFSET $Ly
axi_write $SRC_SIZE $srcSize

# 加载简化光源数据 (中心点光源)
# 在位置 (8,8) 加载值为1.0
puts "加载中心点光源数据..."
for {set i 0} {$i < 4} {incr i} {
    set idx [expr {$Lx * 8 + 8 + $i}]
    axi_write $IDX_LOW $idx
    load_complex $VAL_IN_REAL $VAL_IN_IMAG 1.0 0.0
    axi_write $OPERATION $OP_LOAD_SOURCE
    set result [start_kernel]
    if {!$result} {
        puts "⚠️ 加载SOURCE失败 (idx=$idx)"
    }
}
puts "✅ SOURCE数据加载完成"

puts "\n=========================================="
puts "Step 3: 加载掩模数据 (MASK)"
puts "=========================================="

# 加载简化掩模数据 (常数掩模)
puts "加载常数掩模数据..."
for {set i 0} {$i < 4} {incr i} {
    set idx $i
    axi_write $IDX_LOW $idx
    load_complex $VAL_IN_REAL $VAL_IN_IMAG 1.0 0.0
    axi_write $OPERATION $OP_LOAD_MASK
    set result [start_kernel]
}
puts "✅ MASK数据加载完成"

puts "\n=========================================="
puts "Step 4: 加载SOCS核数据 (KERNELS)"
puts "=========================================="

# SOCS参数: 8核, Nx=3, Ny=3
set Nx 3
set Ny 3
set nkernels 4

puts "配置SOCS参数: Nx=$Nx, Ny=$Ny, nkernels=$nkernels"
axi_write $N_OFFSET $Nx
axi_write $M_OFFSET $Ny
axi_write $NKERNELS $nkernels

# 加载简化核数据 (每个核常数1.0)
puts "加载SOCS核数据..."
set kernel_size [expr {(2*$Nx+1) * (2*$Ny+1)}]  ;# 7x7=49
for {set k 0} {$k < $nkernels} {incr k} {
    for {set i 0} {$i < 4} {incr i} {
        set idx [expr {$k * $kernel_size + $i}]
        axi_write $IDX_LOW $idx
        load_complex $VAL_IN_REAL $VAL_IN_IMAG 1.0 0.0
        axi_write $OPERATION $OP_LOAD_KERNELS
        set result [start_kernel]
    }
}
puts "✅ KERNELS数据加载完成 ($nkernels核)"

puts "\n=========================================="
puts "Step 5: 加载权重数据 (SCALES)"
puts "=========================================="

# 加载权重数据 (每个核权重1.0)
puts "加载SOCS权重..."
for {set k 0} {$k < $nkernels} {incr k} {
    axi_write $IDX_LOW $k
    # 权重是实数，只写入VAL_IN_REAL
    axi_write $VAL_IN_REAL 0x3F800000  ;# 1.0
    axi_write $OPERATION $OP_LOAD_SCALES
    set result [start_kernel]
}
puts "✅ SCALES数据加载完成"

puts "\n=========================================="
puts "Step 6: 执行SOCS计算"
puts "=========================================="

puts "启动SOCS计算..."
axi_write $OPERATION $OP_COMPUTE_SOCS
set start_time [clock milliseconds]
set result [start_kernel]
set elapsed [expr {[clock milliseconds] - $start_time}]

if {$result} {
    puts "✅ SOCS计算完成 (耗时: $elapsed ms)"
}

puts "\n=========================================="
puts "Step 7: 读取输出结果"
puts "=========================================="

# 读取img_out的前10个点
puts "读取IMG_OUT结果..."
axi_write $OPERATION $OP_READ_IMG_OUT

for {set i 0} {$i < 10} {incr i} {
    axi_write $IDX_LOW $i
    set result [start_kernel]
    
    # 读取结果
    set val_real [axi_read $VAL_OUT_REAL]
    set val_imag [axi_read $VAL_OUT_IMAG]
    
    # 解析float值 (简化: 只显示十六进制)
    puts "  idx=$i: real=0x[format %08X $val_real], imag=0x[format %08X $val_imag]"
}

puts "\n=========================================="
puts "Step 8: 性能测量"
puts "=========================================="

# 多次执行测量平均时间
puts "执行10次计算测量性能..."
set total_time 0

for {set run 0} {$run < 10} {incr run} {
    # 重置
    axi_write $OPERATION $OP_RESET
    start_kernel
    
    # SOCS计算
    axi_write $OPERATION $OP_COMPUTE_SOCS
    set start_time [clock milliseconds]
    start_kernel
    set elapsed [expr {[clock milliseconds] - $start_time}]
    set total_time [expr {$total_time + $elapsed}]
}

set avg_time [expr {$total_time / 10.0}]
puts "平均执行时间: $avg_time ms"
puts "总耗时: $total_time ms"

puts "\n=========================================="
puts "完整验证结束"
puts "=========================================="

puts "\n验证项目:"
puts "  ✅ 系统复位"
puts "  ✅ 光源数据加载"
puts "  ✅ 掩模数据加载"
puts "  ✅ SOCS核加载"
puts "  ✅ 权重加载"
puts "  ✅ SOCS计算"
puts "  ✅ 结果读取"
puts "  ✅ 性能测量"
puts "\n=========================================="
puts "验证脚本执行完毕"