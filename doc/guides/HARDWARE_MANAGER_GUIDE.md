# K-Litho BRAM Hardware Manager 验证指南

## 地址映射（关键信息）

根据Address Editor：
- **jtag_axi_0 Master Base Address**: `0x00000000`
- **HLS内核偏移**: `0x00000000`（无偏移）

所以正确的HLS内核地址是 **0x00000000**，不是 0x40000000！

---

## 寄存器映射（基于地址 0x00000000）

| 偏移 | 地址 | 名称 | 功能 | R/W |
|------|------|------|------|-----|
| 0x00 | 0x00000000 | AP_CTRL | 启动/状态 | R/W |
| 0x04 | 0x00000004 | GIER | 全局中断使能 | R/W |
| 0x08 | 0x00000008 | IP_IER | IP中断使能 | R/W |
| 0x0C | 0x0000000C | IP_ISR | IP中断状态 | R/W |
| 0x1C | 0x0000001C | OPERATION | 操作码 | W |
| 0x24 | 0x00000024 | IDX_LOW | 索引低位 | W |
| 0x28 | 0x00000028 | IDX_HIGH | 索引高位 | W |
| 0x2C | 0x0000002C | VAL_IN | 输入值 | W |
| 0x30 | 0x00000030 | VAL_OUT | 输出值 | R |
| 0x40 | 0x00000040 | N_OFFSET | N尺寸 | W |
| 0x48 | 0x00000048 | M_OFFSET | M尺寸 | W |
| 0x50 | 0x00000050 | NS_OFFSET | N步长 | W |
| 0x58 | 0x00000058 | MS_OFFSET | M步长 | W |
| 0x60 | 0x00000060 | KS_OFFSET | K步长 | W |
| 0x68 | 0x00000068 | OS_OFFSET | 输出步长 | W |

---

## AP_CTRL 寄存器位定义

| 位 | 名称 | 功能 |
|----|------|------|
| 0 | AP_START | 写1启动内核 |
| 1 | AP_DONE | 内核完成标志（读1表示完成） |
| 2 | AP_IDLE | 内核空闲标志 |
| 3 | AP_READY | 内核就绪标志 |

---

## 验证命令（正确的地址）

### 1. 读取AP_CTRL状态

```tcl
create_hw_axi_txn rd_apctrl [get_hw_axis hw_axi_1] -address 0x00000000 -type read -len 1 -force
run_hw_axi [get_hw_axi_txns rd_apctrl]
get_property DATA [get_hw_axi_txns rd_apctrl]
```

**预期结果**: `0x00000004` (AP_IDLE=1) 或 `0x00000000`

### 2. 写入参数

```tcl
# 设置尺寸 N=64, M=64
create_hw_axi_txn wr_n [get_hw_axis hw_axi_1] -address 0x00000040 -data 00000040 -len 1 -type write -force
create_hw_axi_txn wr_m [get_hw_axis hw_axi_1] -address 0x00000048 -data 00000040 -len 1 -type write -force
run_hw_axi wr_n
run_hw_axi wr_m
```

### 3. 启动内核

```tcl
# 写入 AP_START=1
create_hw_axi_txn wr_start [get_hw_axis hw_axi_1] -address 0x00000000 -data 00000001 -len 1 -type write -force
run_hw_axi wr_start
```

### 4. 等待完成并读取结果

```tcl
# 轮询 AP_DONE (bit 1)
create_hw_axi_txn rd_status [get_hw_axis hw_axi_1] -address 0x00000000 -type read -len 1 -force
run_hw_axi rd_status
set status [get_property DATA [get_hw_axi_txns rd_status]]
# 检查 bit 1: if ($status & 0x02) == 1，表示完成

# 读取输出
create_hw_axi_txn rd_result [get_hw_axis hw_axi_1] -address 0x00000030 -type read -len 1 -force
run_hw_axi rd_result
get_property DATA [get_hw_axi_txns rd_result]
```

---

## 为什么之前读取0x40000000返回dec0dee3？

| 问题 | 原因 |
|------|------|
| 地址错误 | 0x40000000 不存在于设计中 |
| 返回值 | `dec0dee3` 是AXI默认响应（类似0xDEADBEEF的调试模式值） |
| 正确地址 | jtag_axi_0 映射到 0x00000000 |

---

## 操作码定义

| 操作码 | 名称 | 功能 |
|--------|------|------|
| 0 | LOAD_SOURCE | 加载源数据到BRAM |
| 1 | LOAD_MASK | 加载掩模数据 |
| 2 | LOAD_TCC | 加载TCC参数 |
| 3 | LOAD_KERNELS | 加载FFT核 |
| 4 | LOAD_SCALES | 加载缩放因子 |
| 5 | COMPUTE_TCC | 计算TCC |
| 6 | COMPUTE_SOCS | 计算SOC |
| 7 | READ_IMGF | 读结果图像 |
| 8 | READ_IMG_OUT | 读输出图像 |
| 9 | RESET | 重置状态 |

---

## 快速测试流程

```tcl
# 1. 确认地址正确
create_hw_axi_txn rd_test [get_hw_axis hw_axi_1] -address 0x00000000 -type read -len 1 -force
run_hw_axi rd_test
# 预期: 正常值（不是 dec0dee3）

# 2. 设置简单参数
create_hw_axi_txn wr_n [get_hw_axis hw_axi_1] -address 0x00000040 -data 00000010 -len 1 -type write -force
run_hw_axi wr_n
# N=16

# 3. 重置内核
create_hw_axi_txn wr_reset [get_hw_axis hw_axi_1] -address 0x0000001C -data 00000009 -len 1 -type write -force
run_hw_axi wr_reset
# OPERATION=9 (RESET)

# 4. 启动
create_hw_axi_txn wr_start [get_hw_axis hw_axi_1] -address 0x00000000 -data 00000001 -len 1 -type write -force
run_hw_axi wr_start

# 5. 检查状态
create_hw_axi_txn rd_status [get_hw_axis hw_axi_1] -address 0x00000000 -type read -len 1 -force
run_hw_axi rd_status
get_property DATA [get_hw_axi_txns rd_status]
```

---

**注意**: 所有地址使用 **0x00000000** 作为基地址！