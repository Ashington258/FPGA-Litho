# BRAM硬件验证脚本使用指南

**文件**: `script/verify/board_verify_complete_v2.tcl`  
**目标**: 在实际硬件(xcku3p)上验证BRAM算法功能  
**方法**: 通过JTAG AXI接口直接访问HLS IP寄存器

---

## 📋 使用步骤

### 1️⃣ 准备环境

确保已完成：
- ✅ Vivado已安装 (2025.2)
- ✅ 硬件已连接 (JTAG到xcku3p)
- ✅ Bitstream已下载到FPGA

### 2️⃣ 打开Vivado Tcl Console

有两种方式：

**方式A: 在Vivado GUI中**
```
1. 打开Vivado
2. 点击菜单: Tools → Tcl Console
3. 在Tcl Console窗口底部输入命令
```

**方式B: 在终端中使用Vivado Tcl模式**
```bash
vivado -mode tcl
```

### 3️⃣ 执行验证脚本

在Tcl Console中执行：

```tcl
# 切换到项目目录
cd /root/project/FPGA/vitis/FPGA-Litho

# 运行验证脚本
source script/verify/board_verify_complete_v2.tcl
```

或者直接在bash中运行：

```bash
vivado -mode tcl <<'EOF'
cd /root/project/FPGA/vitis/FPGA-Litho
source script/verify/board_verify_complete_v2.tcl
EOF
```

---

## 📊 脚本执行流程

脚本会按以下步骤执行：

### Step 0: 硬件连接初始化
- 打开硬件管理器
- 连接JTAG硬件服务器
- 打开硬件目标 (Digilent/210251A08870)
- 识别FPGA器件 (xcku3p_0)
- 查找AXI Master接口

### Step 1: 验证内核状态
- 扫描AXI地址空间定位HLS IP
- 读取AP_CTRL寄存器
- 解析内核状态位 (AP_START/DONE/IDLE/READY)

### Step 2: 内核复位测试
- 写入RESET操作码 (operation=9)
- 启动内核执行复位
- 验证复位完成

### Step 3: 配置计算参数
- 设置测试参数 (Lx=16, Ly=16, Nx=3, Ny=3)
- 写入参数寄存器
- 读回验证参数配置

### Step 4: 加载测试数据
- 写入测试数据 (索引0, 值≈1.0)
- 执行LOAD_SOURCE操作
- 读回验证数据加载

### Step 5: 执行计算测试
- 执行SOCS计算 (operation=6)
- 等待计算完成
- 验证内核响应

### Step 6: 读取计算结果
- 读取IMG_OUT结果
- 解析输出数据
- 验证结果有效性

### Step 7: 寄存器完整检查
- 读取所有关键寄存器
- 生成寄存器状态报告

---

## 📤 预期输出示例

脚本会输出详细的测试信息，包括：

```
==========================================
FPGA-Litho BRAM 完整硬件验证 (v2.0)
==========================================
日期: 2026-04-04
器件: xcku3p-ffvb676-2-e
==========================================

[Step 0] 硬件连接初始化...
✓ 硬件服务器已连接
硬件目标: localhost:3121/xilinx_tcf/Digilent/210251A08870
✓ 硬件目标已打开
FPGA器件: xcku3p_0 (xcku3p)
✓ 目标器件正确: xcku3p detected

[Step 1] 验证内核状态和AXI连接...
AP_CTRL = 0x00000004
状态位解析:
  AP_START = 0
  AP_DONE  = 0
  AP_IDLE  = 4
  AP_READY = 0
✓ 内核处于空闲状态 (AP_IDLE=1)

[Step 2] 内核复位测试...
✓ OPERATION寄存器写入成功
✓ 写入值验证成功
✅ 复位操作执行成功

...

==========================================
硬件验证总结
==========================================
✓ 硬件连接初始化
✓ 内核状态读取
✓ 内核复位操作
✓ 参数配置测试
✓ 数据加载测试
✓ 计算执行测试
✓ 结果读取测试
✓ 寄存器完整性检查
==========================================
```

---

## ⚠️ 可能遇到的问题

### 问题1: 未找到AXI接口

**错误信息**: `✗ 错误: 未找到AXI接口`

**原因**: Block Design中jtag_axi IP未正确连接

**解决方案**:
1. 检查Vivado项目 `test_bram_litho/test_bram_litho.xpr`
2. 打开Block Design，确认jtag_axi_0存在
3. 验证jtag_axi连接到AXI Interconnect
4. 重新生成bitstream

### 问题2: 地址扫描失败

**错误信息**: `⚠ 未扫描到可访问地址`

**原因**: HLS IP基地址不在常见范围内

**解决方案**:
1. 在Vivado中打开Block Design
2. 打开Address Editor
3. 查找 `hls_litho_system_bram_1` 的基地址
4. 手动更新脚本中的 `BASE` 变量

### 问题3: 内核状态异常

**错误信息**: `AP_CTRL = 0x00000000`

**原因**: HLS IP未正确启动或地址映射错误

**解决方案**:
1. 确认bitstream已正确下载
2. 检查HLS IP是否在Block Design中正确连接
3. 验证时钟和复位信号连接

---

## 🔍 验证成功的标志

测试成功的标志：

- ✅ **硬件连接**: JTAG识别器件，AXI接口可用
- ✅ **内核响应**: AP_CTRL读取到有效状态 (非全0或全F)
- ✅ **寄存器读写**: 写入值能正确读回
- ✅ **操作执行**: 内核能完成RESET/LOAD/COMPUTE操作
- ✅ **结果输出**: VAL_OUT有非零数据

---

## 📝 后续工作建议

验证通过后，建议进行：

1. **完整数据测试**
   - 使用完整的测试数据集 (`data/bram_test/`)
   - 加载真实的source/mask/kernels数据
   - 验证计算结果的精度

2. **性能测试**
   - 测量计算执行时间
   - 计算吞吐量 (elem/s)
   - 对比CPU性能

3. **算法验证**
   - 对比HLS C仿真结果
   - 验证TCC模式功能
   - 验证SOCS模式功能

---

## 🛠️ 调试技巧

### 保存输出日志

```bash
vivado -mode tcl <<'EOF' > validation_log.txt 2>&1
cd /root/project/FPGA/vitis/FPGA-Litho
source script/verify/board_verify_complete_v2.tcl
EOF
```

### 查看寄存器映射

在Vivado Tcl Console中：

```tcl
open_project /root/project/FPGA/vivado/test_bram_litho/test_bram_litho.xpr
open_bd_design design_1
# 查看Address Editor
report_property [get_bd_addr_segs -of_objects [get_bd_cells hls_litho_system_bram_1]]
```

### 单步调试

可以修改脚本，只执行单个测试步骤：

```tcl
# 只测试Step 1
source script/verify/board_verify_complete_v2.tcl -step 1
```

---

## 📚 相关文档

- [BRAM接口映射](../../doc/BRAM_INTERFACE_MAPPING.md)
- [硬件验证指南](../../doc/BRAM_BOARD_VALIDATION_GUIDE.md)
- [HLS BRAM头文件](../../include/hls_litho_system_bram.h)

---

**更新记录**:
- 2026-04-04: 创建改进版脚本，支持完整硬件初始化和调试