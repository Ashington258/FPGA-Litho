# K-Litho BRAM 快速验证脚本
# 在 Vivado Hardware Manager TCL Console 中执行
# 
# 已验证: AP_CTRL = 0x00000004 (内核空闲)
# 基地址: 0x00000000

puts "=========================================="
puts "K-Litho BRAM 快速验证"
puts "=========================================="

# 定义地址
set AP_CTRL   0x00000000
set OPERATION 0x0000001C
set IDX       0x00000024
set VAL_IN    0x0000002C
set VAL_OUT   0x00000030
set N         0x00000040
set M         0x00000048

# =============================================
# 辅助函数
# =============================================
proc axi_rd {addr} {
    create_hw_axi_txn rd [get_hw_axis hw_axi_1] -address $addr -type read -len 1 -force
    run_hw_axi rd
    set val [get_property DATA [get_hw_axi_txns rd]]
    delete_hw_axi_txn rd
    return $val
}

proc axi_wr {addr val} {
    create_hw_axi_txn wr [get_hw_axis hw_axi_1] -address $addr -data [format %08X $val] -type write -len 1 -force
    run_hw_axi wr
    delete_hw_axi_txn wr
}

# =============================================
# 测试1: 读取所有控制寄存器
# =============================================
puts "\n[Test 1] 读取控制寄存器..."
puts "AP_CTRL   = 0x[axi_rd $AP_CTRL]  (应该为 0x04=IDLE)"
puts "OPERATION = 0x[axi_rd $OPERATION]"
puts "N_OFFSET  = 0x[axi_rd $N]"
puts "M_OFFSET  = 0x[axi_rd $M]"

# =============================================
# 测试2: 内核复位
# =============================================
puts "\n[Test 2] 内核复位 (OPERATION=9)..."
axi_wr $OPERATION 9
axi_wr $AP_CTRL 1
puts "启动复位..."
after 100
set status [axi_rd $AP_CTRL]
puts "复位后状态: 0x[format %08X $status]"

# =============================================
# 测试3: 配置参数 (N=16, M=16)
# =============================================
puts "\n[Test 3] 配置参数..."
axi_wr $N 16
axi_wr $M 16
puts "N = 0x[axi_rd $N] (预期 0x10)"
puts "M = 0x[axi_rd $M] (预期 0x10)"

# =============================================
# 测试4: 写入测试数据
# =============================================
puts "\n[Test 4] 写入测试数据..."
axi_wr $IDX 0
axi_wr $VAL_IN 0x12345678
axi_wr $OPERATION 0  ;# LOAD_SOURCE
axi_wr $AP_CTRL 1
puts "启动数据加载..."
after 100
set status [axi_rd $AP_CTRL]
puts "加载后状态: 0x[format %08X $status]"

# =============================================
# 测试5: 读取结果寄存器
# =============================================
puts "\n[Test 5] 读取结果寄存器..."
puts "VAL_IN  = 0x[axi_rd $VAL_IN]"
puts "VAL_OUT = 0x[axi_rd $VAL_OUT]"

puts "\n=========================================="
puts "快速验证完成!"
puts "如果所有状态正常, 内核工作正常"
puts "=========================================="

# =============================================
# 交互式测试区域
# =============================================
puts "\n可用命令:"
puts "  axi_rd <地址>   - 读取寄存器"
puts "  axi_wr <地址> <值> - 写入寄存器"
puts ""
puts "示例:"
puts "  axi_rd 0x00000000    ;# 读AP_CTRL"
puts "  axi_wr 0x00000000 1  ;# 启动内核"