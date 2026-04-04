# K-Litho BRAM 测试命令
# 
# 请在 Vivado Hardware Manager TCL Console 中逐行执行以下命令
# 确保你已经创建了 hw_axi_1 接口

# =============================================
# 第一部分: 基础测试 (你已完成)
# =============================================
# AP_CTRL = 0x00000004 (内核空闲)

puts "\n=========================================="
puts "继续功能测试"
puts "=========================================="

# =============================================
# 第二部分: 参数配置测试
# =============================================
puts "\n--- 参数配置测试 ---"

# 写入N=64 (地址 0x40)
create_hw_axi_txn wr_n [get_hw_axis hw_axi_1] -address 0x00000040 -data 00000040 -type write -len 1 -force
run_hw_axi wr_n
puts "N参数写入完成"

# 写入M=64 (地址 0x48)
create_hw_axi_txn wr_m [get_hw_axis hw_axi_1] -address 0x00000048 -data 00000040 -type write -len 1 -force
run_hw_axi wr_m
puts "M参数写入完成"

# 读回验证
create_hw_axi_txn rd_n [get_hw_axis hw_axi_1] -address 0x00000040 -type read -len 1 -force
run_hw_axi rd_n
set n_val [get_property DATA [get_hw_axi_txns rd_n]]
puts "N_OFFSET = 0x$n_val (预期 0x40)"

create_hw_axi_txn rd_m [get_hw_axis hw_axi_1] -address 0x00000048 -type read -len 1 -force
run_hw_axi rd_m
set m_val [get_property DATA [get_hw_axi_txns rd_m]]
puts "M_OFFSET = 0x$m_val (预期 0x40)"

# =============================================
# 第三部分: 内核复位测试
# =============================================
puts "\n--- 内核复位测试 ---"

# 写入 OPERATION=9 (RESET)
create_hw_axi_txn wr_reset_op [get_hw_axis hw_axi_1] -address 0x0000001C -data 00000009 -type write -len 1 -force
run_hw_axi wr_reset_op
puts "OPERATION = 9 (RESET)"

# 启动内核
create_hw_axi_txn wr_start_reset [get_hw_axis hw_axi_1] -address 0x00000000 -data 00000001 -type write -len 1 -force
run_hw_axi wr_start_reset
puts "内核启动..."

# 等待100ms后检查状态
after 100
create_hw_axi_txn rd_status_reset [get_hw_axis hw_axi_1] -address 0x00000000 -type read -len 1 -force
run_hw_axi rd_status_reset
set status [get_property DATA [get_hw_axi_txns rd_status_reset]]
puts "复位后状态: 0x$status"

# =============================================
# 第四部分: 数据加载测试
# =============================================
puts "\n--- 数据加载测试 ---"

# 配置小尺寸参数
create_hw_axi_txn wr_n16 [get_hw_axis hw_axi_1] -address 0x00000040 -data 00000010 -type write -len 1 -force
run_hw_axi wr_n16
create_hw_axi_txn wr_m16 [get_hw_axis hw_axi_1] -address 0x00000048 -data 00000010 -type write -len 1 -force
run_hw_axi wr_m16
puts "参数: N=16, M=16"

# 写入测试数据
create_hw_axi_txn wr_idx0 [get_hw_axis hw_axi_1] -address 0x00000024 -data 00000000 -type write -len 1 -force
run_hw_axi wr_idx0
create_hw_axi_txn wr_val_in [get_hw_axis hw_axi_1] -address 0x0000002C -data 12345678 -type write -len 1 -force
run_hw_axi wr_val_in
puts "索引=0, VAL_IN=0x12345678"

# OPERATION=0 (LOAD_SOURCE)
create_hw_axi_txn wr_op_load [get_hw_axis hw_axi_1] -address 0x0000001C -data 00000000 -type write -len 1 -force
run_hw_axi wr_op_load

# 启动
create_hw_axi_txn wr_start_load [get_hw_axis hw_axi_1] -address 0x00000000 -data 00000001 -type write -len 1 -force
run_hw_axi wr_start_load
puts "数据加载启动..."

after 100
create_hw_axi_txn rd_status_load [get_hw_axis hw_axi_1] -address 0x00000000 -type read -len 1 -force
run_hw_axi rd_status_load
puts "加载状态: [get_property DATA [get_hw_axi_txns rd_status_load]]"

# =============================================
# 第五部分: SOC计算测试
# =============================================
puts "\n--- SOC计算测试 ---"

# OPERATION=6 (COMPUTE_SOCS)
create_hw_axi_txn wr_op_soc [get_hw_axis hw_axi_1] -address 0x0000001C -data 00000006 -type write -len 1 -force
run_hw_axi wr_op_soc
puts "OPERATION = 6 (SOC)"

create_hw_axi_txn wr_idx_soc [get_hw_axis hw_axi_1] -address 0x00000024 -data 00000000 -type write -len 1 -force
run_hw_axi wr_idx_soc

# 启动
create_hw_axi_txn wr_start_soc [get_hw_axis hw_axi_1] -address 0x00000000 -data 00000001 -type write -len 1 -force
run_hw_axi wr_start_soc
puts "SOC计算启动..."

after 200
create_hw_axi_txn rd_status_soc [get_hw_axis hw_axi_1] -address 0x00000000 -type read -len 1 -force
run_hw_axi rd_status_soc
puts "SOC状态: [get_property DATA [get_hw_axi_txns rd_status_soc]]"

# =============================================
# 第六部分: 结果读取测试
# =============================================
puts "\n--- 结果读取测试 ---"

# OPERATION=8 (READ_IMG_OUT)
create_hw_axi_txn wr_op_read [get_hw_axis hw_axi_1] -address 0x0000001C -data 00000008 -type write -len 1 -force
run_hw_axi wr_op_read

create_hw_axi_txn wr_idx_read [get_hw_axis hw_axi_1] -address 0x00000024 -data 00000000 -type write -len 1 -force
run_hw_axi wr_idx_read

# 启动
create_hw_axi_txn wr_start_read [get_hw_axis hw_axi_1] -address 0x00000000 -data 00000001 -type write -len 1 -force
run_hw_axi wr_start_read

after 100

# 读取VAL_OUT
create_hw_axi_txn rd_val_out [get_hw_axis hw_axi_1] -address 0x00000030 -type read -len 1 -force
run_hw_axi rd_val_out
puts "VAL_OUT = [get_property DATA [get_hw_axi_txns rd_val_out]]"

puts "\n=========================================="
puts "所有测试完成!"
puts "=========================================="

# =============================================
# 清理事务
# =============================================
puts "\n清理AXI事务..."
foreach txn [get_hw_axi_txns] {
    delete_hw_axi_txn $txn
}
puts "清理完成"