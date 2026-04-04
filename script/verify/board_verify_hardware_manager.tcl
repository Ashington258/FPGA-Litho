# K-Litho BRAM Vivado Hardware Manager Verification
# 
# 此脚本用于Vivado Hardware Manager中访问JTAG AXI Master
# 
# 使用方法:
#   方式1: 在Vivado TCL控制台
#     vivado -mode tcl
#     source board_verify_hardware_manager.tcl
#   
#   方式2: 在Vivado GUI中
#     打开Hardware Manager -> TCL Console
#     source board_verify_hardware_manager.tcl

package require ::tclapp::xilinx::jtag_axi

puts "=========================================="
puts "K-Litho BRAM Hardware Manager 验证"
puts "=========================================="

# =============================================
# 配置参数
# =============================================
set HLS_BASE 0x40000000
set AP_CTRL  [expr {$HLS_BASE + 0x00}]
set OPERATION [expr {$HLS_BASE + 0x1C}]
set IDX_LOW   [expr {$HLS_BASE + 0x24}]
set VAL_IN    [expr {$HLS_BASE + 0x2C}]
set VAL_OUT   [expr {$HLS_BASE + 0x30}]
set N_OFFSET  [expr {$HLS_BASE + 0x40}]
set M_OFFSET  [expr {$HLS_BASE + 0x48}]

# =============================================
# Step 1: 连接硬件服务器
# =============================================
puts "\n[Step 1] 连接硬件服务器..."

# 检查是否已连接
set hw_server [get_hw_servers]
if {[llength $hw_server] == 0} {
    puts "连接本地硬件服务器..."
    connect_hw_server -url localhost:3121
} else {
    puts "硬件服务器已连接: $hw_server"
}

# =============================================
# Step 2: 打开设备
# =============================================
puts "\n[Step 2] 打开目标设备..."

set target_device ""
set hw_devices [get_hw_devices]

foreach dev $hw_devices {
    set dev_name [get_property NAME $dev]
    puts "发现设备: $dev_name"
    if {[string match "*xcku3p*" $dev_name]} {
        set target_device $dev
        break
    }
}

if {$target_device == ""} {
    error "未找到xcku3p设备"
}

puts "目标设备: [get_property NAME $target_device]"
current_hw_device $target_device

# =============================================
# Step 3: 检查Bitstream状态
# =============================================
puts "\n[Step 3] 检查Bitstream..."

# 检查设备是否已编程
set program_status [get_property PROGRAM.HW_DEVICE $target_device]
puts "设备编程状态: $program_status"

# =============================================
# Step 4: 访问JTAG AXI Master
# =============================================
puts "\n[Step 4] 访问JTAG AXI Master..."

# 方法A: 查找hw_axi接口
set hw_axis [get_hw_axis -quiet]
if {[llength $hw_axis] > 0} {
    puts "发现AXI接口: $hw_axis"
    
    # 使用第一个AXI接口
    set axi_if [lindex $hw_axis 0]
    puts "使用AXI接口: [get_property NAME $axi_if]"
    
    # 读取AP_CTRL
    puts "\n读取HLS内核寄存器..."
    
    # 创建读事务
    create_hw_axi_txn rd_ctrl $axi_if -address $AP_CTRL -type read
    run_hw_axi_txn rd_ctrl
    set ap_ctrl_val [get_property DATA [get_hw_axi_txn rd_ctrl]]
    puts "AP_CTRL (0x[format %08X $AP_CTRL]) = 0x[format %08X $ap_ctrl_val]"
    delete_hw_axi_txn rd_ctrl
    
    # 启动内核
    puts "\n启动内核测试..."
    
    # 写入测试参数
    create_hw_axi_txn wr_n $axi_if -address $N_OFFSET -data 00000040 -type write
    create_hw_axi_txn wr_m $axi_if -address $M_OFFSET -data 00000040 -type write
    create_hw_axi_txn wr_op $axi_if -address $OPERATION -data 00000000 -type write
    
    # 启动内核 (AP_START=1)
    create_hw_axi_txn wr_start $axi_if -address $AP_CTRL -data 00000001 -type write
    
    # 执行写入
    run_hw_axi_txn wr_n
    run_hw_axi_txn wr_m
    run_hw_axi_txn wr_op
    run_hw_axi_txn wr_start
    
    puts "内核启动命令已发送"
    
    # 等待完成
    puts "等待内核完成..."
    set done 0
    for {set i 0} {$i < 100} {incr i} {
        after 10
        create_hw_axi_txn rd_done $axi_if -address $AP_CTRL -type read
        run_hw_axi_txn rd_done
        set status [get_property DATA [get_hw_axi_txn rd_done]]
        delete_hw_axi_txn rd_done
        
        # 检查AP_DONE位 (bit 1)
        if {$status & 0x02} {
            set done 1
            puts "内核执行完成! 状态: 0x[format %08X $status]"
            break
        }
    }
    
    if {$done == 0} {
        puts "警告: 内核未在预期时间内完成"
    }
    
    # 清理事务
    delete_hw_axi_txn wr_n
    delete_hw_axi_txn wr_m
    delete_hw_axi_txn wr_op
    delete_hw_axi_txn wr_start
    
} else {
    puts "未发现AXI接口。"
    
    # 方法B: 使用JTAG AXI Master命令
    puts "\n尝试使用JTAG AXI Master命令..."
    
    # 检查JTAG AXI IP状态
    set jtag_axi_found 0
    
    # 尝试使用底层JTAG访问
    catch {
        # 创建AXI Master实例
        set axi_master [create_hw_axi_master]
        puts "创建AXI Master成功: $axi_master"
        set jtag_axi_found 1
    } result
    
    if {$jtag_axi_found == 0} {
        puts "\n=========================================="
        puts "需要手动操作:"
        puts "=========================================="
        puts "方法1: 在Vivado GUI中"
        puts "  1. 打开 Hardware Manager"
        puts "  2. 右键 xcku3p_0 设备"
        puts "  3. 选择 'Run JTAG to AXI Master'"
        puts "  4. 输入地址 0x40000000 读取寄存器"
        puts ""
        puts "方法2: 添加调试核到设计"
        puts "  1. 在Block Design中添加ILA核"
        puts "  2. 连接需要监控的信号"
        puts "  3. 重新生成bitstream"
        puts "  4. Hardware Manager会检测到ILA"
        puts ""
        puts "方法3: 使用XSDB"
        puts "  xsdb> connect"
        puts "  xsdb> targets -set -filter {name =~ \"*xcku3p*\"}"
        puts "  xsdb> mrd 0x40000000"
        puts "=========================================="
    }
}

# =============================================
# Step 5: 清理
# =============================================
puts "\n[Step 5] 验证完成"

# 保持连接以便进一步调试
puts "\nHardware Manager保持连接，可继续调试"
puts "输入 'disconnect_hw_server' 断开连接"