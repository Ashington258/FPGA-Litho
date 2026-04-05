# 硬件调试问题分析与解决方案

## 🔍 问题诊断结果

### ❌ 核心问题：jtag_axi IP不支持Tcl脚本访问

**证据：**
```
WARNING: [Labtoolstcl 44-226] No matching hw_axi were found
Using AXI core:           # AXI变量为空
```

### 🤔 为什么 `test.tcl` 看起来"可以执行"？

#### 错误的理解：
```tcl
# test.tcl 的代码结构
set AXI [lindex [get_hw_axis *] 0]   # AXI = "" (空字符串)
puts "Using AXI core: $AXI"          # 打印空行，不报错

proc axi_read {addr} {
    global AXI
    create_hw_axi_txn rd_txn $AXI ...  # ← 这里会崩溃！
}
```

#### 实际情况：
1. **脚本不会崩溃在启动阶段**
2. **崩溃会在第一次调用 `axi_read()` 时发生**
3. **原因是 `create_hw_axi_txn` 命令需要有效的AXI核心对象**

#### 测试方法：
```bash
# 运行你的脚本看看会在哪里报错
vivado -mode tcl <<'EOF'
open_hw_manager
connect_hw_server -url localhost:3121
current_hw_target [get_hw_targets */xilinx_tcf/Digilent/210251A08870]
open_hw_target
source /root/project/FPGA/vitis/FPGA-Litho/script/test.tcl
EOF

# 你会看到错误：
# ERROR: [Labtoolstcl 44-xx] Cannot create AXI transaction: invalid AXI core
```

---

## ✅ 解决方案对比

### 方案1：Vivado GUI手动操作 ⭐ 推荐

**优点：**
- ✅ 无需重新编译bitstream
- ✅ 立即可用
- ✅ 提供可视化界面

**缺点：**
- ❌ 需要手动操作
- ❌ 不适合自动化测试

**操作步骤：**
参见详细指南：[`doc/VIVADO_GUI_GUIDE.md`](VIVADO_GUI_GUIDE.md:1)

**快速开始：**
```bash
# 启动Vivado GUI
vivado &

# 在GUI中：
# 1. Open Hardware Manager
# 2. Open Target → Auto Connect
# 3. 右键 xcku3p_0 → Debug Probes → JTAG to AXI Master
```

---

### 方案2：添加VIO IP到设计 ⭐⭐ 最适合自动化

**优点：**
- ✅ 支持Tcl脚本控制
- ✅ 可以自动化测试
- ✅ 保留在设计中，未来可用

**缺点：**
- ❌ 需要重新生成bitstream（约30分钟）
- ❌ 需要修改Block Design

**实施步骤：**

#### 步骤1：运行脚本添加VIO IP
```bash
cd /root/project/FPGA/vitis/FPGA-Litho
vivado -mode tcl -source script/vivado/add_vio_debug.tcl
```

#### 步骤2：在Vivado GUI中完成连接
```bash
vivado &
# 打开 Block Design
# 手动连接VIO信号到HLS IP的控制端口
```

#### 步骤3：重新生成bitstream
```bash
# 在Vivado Tcl中：
open_project /root/project/FPGA/vivado/test_bram_litho/test_bram_litho.xpr
reset_run impl_1
launch_runs impl_1 -to_step write_bitstream -jobs 4
wait_on_run impl_1
```

#### 步骤4：使用VIO Tcl脚本控制硬件
```tcl
# 新的验证脚本（使用VIO）
open_hw_manager
connect_hw_server -url localhost:3121
current_hw_target [get_hw_targets */xilinx_tcf/Digilent/210251A08870]
open_hw_target

# 获取VIO设备
set vio [get_hw_vios -of_objects [get_hw_devices xcku3p_0]]

# 启动HLS IP
set_property OUTPUT_VALUE 1 [get_hw_probes ap_start -of_objects $vio]
commit_hw_vio [get_hw_probes ap_start -of_objects $vio]

# 读取状态
set status [get_property INPUT_VALUE [get_hw_probes ap_done -of_objects $vio]]
```

---

### 方案3：创建MicroBlaze处理器系统

**优点：**
- ✅ 最完整的解决方案
- ✅ 支持软件驱动
- ✅ 可以运行完整的测试程序

**缺点：**
- ❌ 需要大量设计工作（2小时以上）
- ❌ 需要重新编译整个系统

**适用场景：**
- 需要完整的嵌入式测试
- 未来需要部署生产级应用

---

## 📊 方案对比总结

| 方案 | 时间成本 | 自动化支持 | 修改设计 | 推荐度 |
|------|---------|-----------|---------|--------|
| GUI手动操作 | 5分钟 | ❌ 无 | ❌ 不需要 | ⭐⭐⭐ |
| 添加VIO IP | 30分钟 | ✅ 完全支持 | ✅ 需要 | ⭐⭐⭐⭐⭐ |
| MicroBlaze系统 | 2小时+ | ✅ 完全支持 | ✅ 大量修改 | ⭐⭐ |

---

## 🎯 推荐行动方案

### 立即可用：方案1（GUI）
**现在就可以执行：**
```bash
# 查看GUI操作指南
cat doc/VIVADO_GUI_GUIDE.md

# 启动Vivado GUI
vivado &
```

### 长期方案：方案2（VIO）
**为未来自动化测试做准备：**
```bash
# 1. 添加VIO IP
vivado -mode tcl -source script/vivado/add_vio_debug.tcl

# 2. 在GUI中完成连接
# 3. 重新生成bitstream
# 4. 使用新的VIO测试脚本
```

---

## 🔧 快速验证方案

### 验证方案1（GUI）是否可用：
```bash
# 启动Vivado GUI
vivado &

# 在Tcl Console中运行：
open_hw_manager
connect_hw_server -url localhost:3121
current_hw_target [get_hw_targets */xilinx_tcf/Digilent/210251A08870]
open_hw_target

# 检查是否有JTAG to AXI Master功能
# 在GUI中：Hardware → Hardware Manager → 右键 xcku3p_0
```

### 验证方案2（VIO）实施步骤：
```bash
# 运行VIO添加脚本
cd /root/project/FPGA/vitis/FPGA-Litho
vivado -mode tcl -source script/vivado/add_vio_debug.tcl
```

---

## 📝 经验总结

### 教训：
1. **jtag_axi IP ≠ 标准AXI调试接口**
   - jtag_axi是为特定调试流程设计的
   - 不支持通过 `get_hw_axis` 命令访问

2. **VIO IP是最佳调试方案**
   - 支持Tcl脚本控制
   - 实时读写信号
   - 不需要处理器

3. **脚本错误处理很重要**
   - 你的 `test.tcl` 缺少错误检查
   - 应该在获取AXI接口后验证是否为空

### 最佳实践：
```tcl
# ✅ 正确的错误处理
set axi_cores [get_hw_axis *]
if {[llength $axi_cores] == 0} {
    puts "ERROR: No AXI cores found"
    puts "建议：使用VIO IP或GUI操作"
    return
}
set AXI [lindex $axi_cores 0]

# ❌ 错误的方式
set AXI [lindex [get_hw_axis *] 0]  # 可能为空
create_hw_axi_txn txn $AXI ...      # 会崩溃！
```

---

## 🚀 下一步行动

**选择你的方案：**

### 方案A：立即使用GUI验证
```bash
# 1. 查看GUI指南
cat doc/VIVADO_GUI_GUIDE.md

# 2. 启动Vivado GUI
vivado &
```

### 方案B：准备自动化测试环境（推荐）
```bash
# 1. 添加VIO IP到设计
vivado -mode tcl -source script/vivado/add_vio_debug.tcl

# 2. 后续需要：
#    - 在GUI中完成信号连接
#    - 重新生成bitstream
#    - 创建VIO测试脚本
```

**我的建议：先尝试方案A验证功能，然后实施方案B为自动化测试做准备。**