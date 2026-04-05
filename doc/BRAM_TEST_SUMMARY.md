# BRAM Litho 硬件测试总结

## 测试日期
2026年4月5日

## 测试环境
- **FPGA设备**: xcku3p-ffvb676-2-e (Kintex UltraScale+)
- **连接方式**: JTAG (Digilent/210251A08870)
- **硬件服务器**: localhost:3121
- **Vivado版本**: 2025.2

## 测试脚本
基于官方文档创建的测试脚本：
- **脚本路径**: `/root/project/FPGA/vitis/FPGA-Litho/script/verify/bram_test_final.tcl`
- **参考文档**:
  - `doc/reference/Example_Tcl_Command_Script.tcl` (官方示例)
  - `doc/reference/TCL_Command.csv` (Tcl命令参考)
  - `xhls_litho_system_bram_hw.h` (HLS IP寄存器定义)

## 测试结果

### ✓ 成功验证的功能

#### 1. 硬件连接和初始化
- **JTAG连接**: 成功连接到硬件服务器
- **目标设备**: `localhost:3121/xilinx_tcf/Digilent/210251A08870`
- **FPGA设备**: `xcku3p_0` 正常识别
- **Bitstream下载**: 9秒完成，14.72MB文件

#### 2. AXI调试核心访问
- **AXI核心**: `hw_axi_1` 成功发现并访问
- **核心状态**: 可以重置和配置
- **事务创建**: `create_hw_axi_txn` 成功执行

#### 3. CONTROL寄存器验证
```
地址: 0x00
读出值: 0x00000004
解析状态:
  - bit0 ap_start:  0 (未启动)
  - bit1 ap_done:   0 (未完成)
  - bit2 ap_idle:   1 (空闲状态 ✓)
  - bit3 ap_ready:  0 (未就绪)
```
**结论**: IP处于空闲状态，可以正常启动 ✓

#### 4. 重置操作测试 (OP_RESET = 9)
```
寄存器配置:
  - OPERATION (0x1c): 写入 0x00000009 ✓
  - CONTROL (0x00): 写入 0x00000001 (启动) ✓

执行结果:
  - 状态寄存器: 0x0000000e (ap_done=1, ap_idle=1) ✓
  - 返回值 (0x10): 0x000000003f800000 (IEEE754浮点数 1.0) ✓
```
**结论**: 重置操作成功执行，返回值表示成功 ✓

#### 5. 寄存器映射验证
使用正确的HLS IP寄存器映射（来自官方驱动文件）：

| 寄存器名称 | 地址 | 大小 | 功能 |
|-----------|------|------|------|
| AP_CTRL | 0x00 | 32-bit | 控制状态寄存器 |
| GIE | 0x04 | 32-bit | 全局中断使能 |
| AP_RETURN | 0x10-0x14 | 64-bit | 返回值 |
| OPERATION | 0x1c | 32-bit | 操作码选择 |
| IDX | 0x24 | 32-bit | 数组索引 |
| VAL_R | 0x2c-0x30 | 64-bit | 数据值（复数） |
| MODE | 0x38 | 32-bit | 计算模式 |
| LX | 0x40 | 32-bit | 频域X尺寸 |
| LY | 0x48 | 32-bit | 频域Y尺寸 |
| NX | 0x50 | 32-bit | TCC/SOCS参数 |
| NY | 0x58 | 32-bit | TCC/SOCS参数 |
| SRCSIZE | 0x60 | 32-bit | 光源尺寸 |
| NKERNELS | 0x68 | 32-bit | SOCS核数量 |

#### 6. 操作码定义
```tcl
OP_LOAD_SOURCE   = 0  # 加载光源数据
OP_LOAD_MASK     = 1  # 加载mask数据
OP_LOAD_TCC      = 2  # 加载TCC矩阵
OP_LOAD_KERNELS  = 3  # 加载SOCS kernels
OP_LOAD_SCALES   = 4  # 加载SOCS scales
OP_COMPUTE_TCC   = 5  # TCC模式计算
OP_COMPUTE_SOCS  = 6  # SOCS模式计算
OP_READ_IMGF     = 7  # 读取imgf结果
OP_READ_IMG_OUT  = 8  # 读取img_out结果
OP_RESET         = 9  # 重置所有BRAM存储
```

### 关键发现

#### 1. AXI核心命名问题解决
- **问题**: 之前的脚本假设AXI核心名称不正确
- **解决**: 使用 `get_hw_axis` 发现实际核心名为 `hw_axi_1`
- **方法**: 参考官方文档的动态获取方法

#### 2. 寄存器地址映射正确
- **之前的错误**: 使用了错误的寄存器地址（SCALE=0x10等）
- **正确映射**: 来自官方HLS驱动文件 `xhls_litho_system_bram_hw.h`
- **验证结果**: CONTROL寄存器和OPERATION寄存器读写正常

#### 3. Tcl脚本语法要求
- **关键点**: Tcl不支持行内注释（`# 注释`）
- **正确方法**: 所有注释必须单独一行或使用分号分隔
- **数据格式**: 地址和数据必须为十六进制字符串格式

## 测试脚本关键代码

### 硬件连接流程（基于官方文档）
```tcl
# 1. 初始化硬件管理器
open_hw_manager

# 2. 连接到硬件服务器
connect_hw_server -url localhost:3121

# 3. 打开硬件目标
set targets [get_hw_targets]
current_hw_target [lindex $targets 0]
open_hw_target

# 4. 配置FPGA设备
set devices [get_hw_devices]
current_hw_device [lindex $devices 0]
set_property PROGRAM.FILE $bit_file [lindex $devices 0]
set_property PROBES.FILE $ltx_file [lindex $devices 0]

# 5. 下载bitstream
program_hw_devices [lindex $devices 0]
refresh_hw_device [lindex $devices 0]

# 6. 获取AXI核心
set axi_cores [get_hw_axis]
set AXI [lindex $axi_cores 0]  # hw_axi_1

# 7. 重置AXI核心
reset_hw_axi $AXI
```

### AXI读写操作流程（官方文档方法）
```tcl
# 创建AXI读事务
create_hw_axi_txn rd_txn $AXI -type read -address 00000000 -len 1

# 执行事务
run_hw_axi [get_hw_axi_txns rd_txn]

# 获取数据
set data [get_property DATA [get_hw_axi_txns rd_txn]]

# 创建AXI写事务
create_hw_axi_txn wr_txn $AXI -type write -address 0000001c -len 1 -data 00000009

# 执行写事务
run_hw_axi [get_hw_axi_txns wr_txn]

# 清理事务
delete_hw_axi_txn [get_hw_axi_txns]
```

## 后续测试建议

### 需要进一步验证的功能
1. **完整数据流测试**
   - 加载完整光源数据（4096个复数值）
   - 加载完整mask数据
   - 执行完整的TCC/SOCS计算

2. **性能测试**
   - 测试数据加载速度
   - 测试计算执行时间
   - 测试BRAM存储容量

3. **边界条件测试**
   - 测试最大数据尺寸（Lx=64, Ly=64）
   - 测试最大SOCS核数量（8个）
   - 测试错误参数处理

### 建议的测试脚本改进
1. **批量数据加载**
   - 创建循环加载4096个数据点
   - 使用Burst模式提高效率

2. **数据验证**
   - 添加已知输入数据的计算测试
   - 比对输出结果与预期值

3. **自动化测试**
   - 创建完整的测试套件
   - 生成测试报告

## 结论

### ✓ 硬件验证成功
基于官方文档的测试脚本成功验证了：
1. **FPGA硬件连接**正常
2. **JTAG-AXI接口**可以访问HLS IP
3. **BRAM IP寄存器映射**正确
4. **基本读写操作**正常执行
5. **控制流程**（启动、等待、读取返回值）正常

### 关键成果
- **找到了正确的AXI核心名称**: `hw_axi_1`
- **验证了正确的寄存器映射**: 来自官方HLS驱动文件
- **建立了标准的硬件测试流程**: 基于官方Tcl文档

### 下一步
可以进行完整的BRAM算法功能验证和性能测试。

---
**测试完成时间**: 2026年4月5日 09:24
**测试状态**: 基础硬件验证成功 ✓
**测试脚本**: `script/verify/bram_test_final.tcl`