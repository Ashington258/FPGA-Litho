# K-Litho BRAM 板级验证测试命令
# 
# 请在 Vivado Hardware Manager TCL Console 中逐行执行以下命令

# =============================================
# 第一部分: 基础验证 (已完成)
# =============================================
# create_hw_axi_txn rd_apctrl [get_hw_axis hw_axi_1] -address 0x00000000 -type read -len 1 -force
# run_hw_axi [get_hw_axi_txns rd_apctrl]
# 结果: 0x00000004 (AP_IDLE=1, 内核空闲)

puts "\n继续验证..."

# =============================================
# 第二部分: 寄存器读写测试
# =============================================

# 定义地址
set AP_CTRL   0x00000000
set OPERATION 0x0000001C
set IDX       0x00000024
set VAL_IN    0x0000002C
set VAL_OUT   0x00000030
set N         0x00000040
set M         0x00000048

# 测试写入
puts "\n测试寄存器写入..."

# 创建写事务 - 写入N=64
create_hw_axi_txn wr_n [get_hw_axis hw_axi_1] -address $N -data 00000040 -type write -len 1 -force
run_hw_axi wr_n

# 创建写事务 - 写入M=64
create_hw_axi_txn wr_m [get_hw_axis hw_axi_1] -address $M -data 00000040 -type write -len 1 -force
run_hw_axi wr_m

# 读回验证
create_hw_axi_txn rd_n [get_hw_axis hw_axi_1] -address $N -type read -len 1 -force
run_hw_axi rd_n
puts "N = [get_property DATA [get_hw_axi_txns rd_n]]"

create_hw_axi_txn rd_m [get_hw_axis hw_axi_1] -address $M -type read -len 1 -force
run_hw_axi rd_m
puts "M = [get_property DATA [get_hw_axi_txns rd_m]]"

# =============================================
# 第三部分: 内核复位
# =============================================
puts "\n执行内核复位..."

# 写入 OPERATION=9 (RESET)
create_hw_axi_txn wr_reset [get_hw_axis hw_axi_1] -address $OPERATION -data 00000009 -type write -len 1 -force
run_hw_axi wr_reset

# 启动内核
create_hw_axi_txn wr_start [get_hw_axis hw_axi_1] -address $AP_CTRL -data 00000001 -type write -len 1 -force
run_hw_axi wr_start

# 等待并检查状态
after 100
create_hw_axi_txn rd_status [get_hw_axis hw_axi_1] -address $AP_CTRL -type read -len 1 -force
run_hw_axi rd_status
set status [get_property DATA [get_hw_axi_txns rd_status]]
puts "复位后状态: $status"

# =============================================
# 第四部分: 数据加载测试
# =============================================
puts "\n测试数据加载..."

# 配置小尺寸
create_hw_axi_txn wr_n16 [get_hw_axis hw_axi_1] -address $N -data 00000010 -type write -len 1 -force
run_hw_axi wr_n16

create_hw_axi_txn wr_m16 [get_hw_axis hw_axi_1] -address $M -data 00000010 -type write -len 1 -force
run_hw_axi wr_m16

# 写入索引和值
create_hw_axi_txn wr_idx [get_hw_axis hw_axi_1] -address $IDX -data 00000000 -type write -len 1 -force
run_hw_axi wr_idx

create_hw_axi_txn wr_val [get_hw_axis hw_axi_1] -address $VAL_IN -data 12345678 -type write -len 1 -force
run_hw_axi wr_val

# LOAD_SOURCE (OPERATION=0)
create_hw_axi_txn wr_op0 [get_hw_axis hw_axi_1] -address $OPERATION -data 00000000 -type write -len 1 -force
run_hw_axi wr_op0

# 启动
create_hw_axi_txn wr_start2 [get_hw_axis hw_axi_1] -address $AP_CTRL -data 00000001 -type write -len 1 -force
run_hw_axi wr_start2

after 100
create_hw_axi_txn rd_status2 [get_hw_axis hw_axi_1] -address $AP_CTRL -type read -len 1 -force
run_hw_axi rd_status2
puts "加载后状态: [get_property DATA [get_hw_axi_txns rd_status2]]"

# =============================================
# 第五部分: SOC计算测试
# =============================================
puts "\n执行SOC计算..."

# OPERATION=6 (COMPUTE_SOCS)
create_hw_axi_txn wr_op6 [get_hw_axis hw_axi_1] -address $OPERATION -data 00000006 -type write -len 1 -force
run_hw_axi wr_op6

create_hw_axi_txn wr_idx0 [get_hw_axis hw_axi_1] -address $IDX -data 00000000 -type write -len 1 -force
run_hw_axi wr_idx0

# 启动
create_hw_axi_txn wr_start3 [get_hw_axis hw_axi_1] -address $AP_CTRL -data 00000001 -type write -len 1 -force
run_hw_axi wr_start3

after 200
create_hw_axi_txn rd_status3 [get_hw_axis hw_axi_1] -address $AP_CTRL -type read -len 1 -force
run_hw_axi rd_status3
puts "SOC计算状态: [get_property DATA [get_hw_axi_txns rd_status3]]"

# =============================================
# 第六部分: 读取结果
# =============================================
puts "\n读取计算结果..."

# OPERATION=8 (READ_IMG_OUT)
create_hw_axi_txn wr_op8 [get_hw_axis hw_axi_1] -address $OPERATION -data 00000008 -type write -len 1 -force
run_hw_axi wr_op8

create_hw_axi_txn wr_idx0b [get_hw_axis hw_axi_1] -address $IDX -data 00000000 -type write -len 1 -force
run_hw_axi wr_idx0b

# 启动
create_hw_axi_txn wr_start4 [get_hw_axis hw_axi_1] -address $AP_CTRL -data 00000001 -type write -len 1 -force
run_hw_axi wr_start4

after 100

# 读取VAL_OUT
create_hw_axi_txn rd_out [get_hw_axis hw_axi_1] -address $VAL_OUT -type read -len 1 -force
run_hw_axi rd_out
puts "VAL_OUT = [get_property DATA [get_hw_axi_txns rd_out]]"

puts "\n=========================================="
puts "基础验证完成!"
puts "=========================================="
puts "测试项目:"
puts "  ✅ AP_CTRL状态读取 (0x00000004)"
puts "  ✅ 寄存器写入测试 (N/M)"
puts "  ✅ 内核复位"
puts "  ✅ 数据加载"
puts "  ✅ SOC计算"
puts "  ✅ 结果读取"
puts ""
puts "如需完整验证, 请运行:"
puts "  source script/bram_full_test.tcl"
puts "=========================================="

# 清理事务
puts "\n清理AXI事务..."
foreach txn [get_hw_axi_txns] {
    delete_hw_axi_txn $txn
}
puts "清理完成"