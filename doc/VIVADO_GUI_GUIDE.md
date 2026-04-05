# Vivado GUI硬件验证操作指南

**目标器件**: xcku3p-ffvb676-2-e  
**Bitstream**: design_1_wrapper.bit (已下载)  
**验证目标**: 通过Vivado GUI手动测试BRAM算法功能

---

## 📋 准备工作

### 1. 启动Vivado并连接硬件

#### 方法A: 从终端启动
```bash
vivado &
```

#### 方法B: 从应用程序菜单启动
- 在应用程序菜单中找到 "Vivado 2025.2"
- 点击启动

---

## 🔌 Step 1: 打开Hardware Manager

### 1.1 打开Hardware Manager窗口

**操作步骤**:
```
方法1: 菜单栏
  Flow Navigator → Program and Debug → Open Hardware Manager

方法2: 菜单栏
  Tools → Open Hardware Manager

方法3: Tcl Console
  open_hw_manager
```

### 1.2 连接到硬件服务器

**在Hardware Manager窗口中**:
```
1. 点击绿色的 "Open target" 链接
2. 选择 "Auto Connect" (自动连接)
   
   或者:
   
1. 点击 "Open target" → "Open New Hardware Target"
2. 在弹出的对话框中:
   - Host: localhost
   - Port: 3121
3. 点击 "OK"
```

**预期结果**:
- ✅ Hardware窗口显示服务器连接: `localhost:3121`
- ✅ 显示硬件目标: `Digilent/210251A08870`
- ✅ 显示FPGA器件: `xcku3p_0`

---

## 🔍 Step 2: 验证Bitstream已加载

### 2.1 检查器件状态

**在Hardware Manager中**:
```
1. 在左侧硬件树中展开:
   localhost:3121
     └─ Digilent/210251A08870
         └─ xcku3p_0  (应该有绿色对勾 ✓)

2. 右键点击 xcku3p_0 → Device Properties
   - Part: xcku3p-ffvb676-2-e
   - Status: Programmed (已编程)
```

**如果器件未编程**:
```
1. 右键点击 xcku3p_0 → Program Device
2. 选择 bitstream 文件:
   /root/project/FPGA/vivado/test_bram_litho/test_bram_litho.runs/impl_1/design_1_wrapper.bit
3. 点击 "Program"
```

---

## 🛠️ Step 3: 手动调试方法

由于jtag_axi IP无法通过标准AXI接口访问，我们提供以下调试方法：

### 方法1: 使用JTAG to AXI Master (如果可用)

#### 3.1 检查是否支持JTAG to AXI Master

**操作步骤**:
```
1. 在Hardware Manager中，右键点击器件 xcku3p_0
2. 查看菜单中是否有:
   - "Debug Probes" 
   - "JTAG to AXI Master"
   
如果看到这些选项，继续以下步骤:
```

#### 3.2 打开JTAG to AXI Master

**操作步骤**:
```
1. 右键 xcku3p_0 → Debug Probes
   (或者 Tools → Debug → JTAG to AXI Master)
   
2. 在打开的窗口中:
   - 选择 "Create AXI Transaction"
   - 配置AXI读写操作
```

#### 3.3 配置AXI读操作

**测试读取AP_CTRL寄存器**:
```
在AXI Transaction窗口中:

1. Operation: Read
2. Address: 0x40000000  (假设基地址，根据实际情况调整)
3. Size: 4 bytes (32-bit)
4. 点击 "Execute"

观察返回的数据值，记录结果
```

#### 3.4 配置AXI写操作

**测试写入数据**:
```
1. Operation: Write
2. Address: 0x40000000 + 偏移量
3. Data: 输入要写入的数据 (例如: 0x00000001)
4. 点击 "Execute"

然后执行读操作验证写入是否成功
```

---

### 方法2: 添加ILA (Integrated Logic Analyzer) 调试

如果当前bitstream中包含ILA IP，可以使用此方法。

#### 3.1 检查是否有ILA

**操作步骤**:
```
1. 在Hardware Manager中，展开 xcku3p_0
2. 查看是否有 "Debug Cores" 或 "ILA" 项
   
如果有ILA，继续以下步骤:
```

#### 3.2 配置ILA捕获

**操作步骤**:
```
1. 右键点击 ILA → Settings
2. 设置触发条件:
   - Trigger Position: 选择合适的触发位置
   - Trigger Condition: 设置触发信号条件
   
3. 点击 "Run" 开始捕获
4. 观察波形窗口中的信号变化
```

---

### 方法3: 使用VIO (Virtual Input/Output)

如果bitstream中包含VIO IP，可以进行交互式测试。

#### 3.1 检查VIO

**操作步骤**:
```
1. 在Hardware Manager中，展开 xcku3p_0
2. 查看是否有 "VIO" 项

如果有VIO:
```

#### 3.2 使用VIO控制信号

**操作步骤**:
```
1. 右键 VIO → Open VIO Console
2. 在VIO Console中:
   - 可以看到输入信号 (显示当前值)
   - 可以看到输出信号 (可以切换值)
   
3. 测试步骤:
   a. 切换输出信号 (例如: start信号)
   b. 观察输入信号变化 (例如: done信号)
   c. 记录测试结果
```

---

## 📝 Step 4: 手动测试流程 (如果以上方法都不可用)

### 4.1 使用Device Properties查看寄存器

**操作步骤**:
```
1. 右键 xcku3p_0 → Device Properties
2. 查看是否有 "JTAG Instruction" 选项卡
3. 可以尝试发送JTAG指令进行调试
```

### 4.2 使用Tcl Console进行交互

**在Vivado Tcl Console中**:

#### 连接硬件
```tcl
# 打开硬件管理器
open_hw_manager

# 连接硬件服务器 (如果未连接)
connect_hw_server -url localhost:3121

# 打开硬件目标
open_hw_target [lindex [get_hw_targets] 0]

# 获取器件
set device [lindex [get_hw_devices] 0]
```

#### 查看硬件属性
```tcl
# 查看器件信息
get_property PART $device

# 查看可用的调试核心
get_hw_debug_cores

# 查看AXI接口 (如果有)
get_hw_axis *
```

#### 手动创建AXI事务 (尝试)
```tcl
# 尝试查找AXI接口
set axi_masters [get_hw_axis * -quiet]

if {[llength $axi_masters] > 0} {
    puts "找到AXI接口: $axi_masters"
    set axi [lindex $axi_masters 0]
    
    # 尝试读取地址
    catch {
        create_hw_axi_txn test_rd $axi -address 0x40000000 -type read -len 1
        run_hw_axi_txn test_rd
        puts "读取成功"
    }
} else {
    puts "未找到可用的AXI接口"
}
```

---

## 🎯 Step 5: 实际测试建议

### 5.1 如果以上GUI方法都无法使用

**建议采用以下方案之一**:

#### 方案A: 添加VIO IP (推荐)
```
优点:
- 可以在GUI中直接控制
- 实时观察结果
- 30分钟内完成

步骤:
1. 打开Block Design
2. 添加VIO IP
3. 连接到HLS IP的控制信号
4. 重新生成bitstream
5. 使用VIO Console测试
```

#### 方案B: 添加ILA IP
```
优点:
- 可以捕获波形
- 适合调试时序问题
- 无需软件

步骤:
1. 打开Block Design
2. 添加ILA IP
3. 连接关键信号
4. 重新生成bitstream
5. 使用ILA捕获数据
```

---

## 📊 测试记录模板

### 测试环境
- Vivado版本: 2025.2
- 器件型号: xcku3p-ffvb676-2-e
- Bitstream: design_1_wrapper.bit
- 测试日期: 2026-04-04

### 测试项目检查表

| 测试项 | 可用性 | 测试结果 | 备注 |
|--------|--------|----------|------|
| JTAG连接 | ✅ | 正常 | Digilent/210251A08870 |
| Bitstream加载 | ✅ | 正常 | 14.72MB |
| JTAG to AXI Master | ⬜ | 待测试 | |
| ILA调试核心 | ⬜ | 待测试 | |
| VIO控制接口 | ⬜ | 待测试 | |
| Tcl AXI访问 | ❌ | 不可用 | jtag_axi不支持 |

### 寄存器测试记录

**AP_CTRL寄存器** (偏移量 0x00):
- 地址: 0x____________
- 读取值: 0x____________
- 状态: ____________

**其他寄存器**:
- 记录测试结果...

---

## 🔧 故障排查

### 问题1: 无法连接硬件服务器

**解决方案**:
```
1. 检查JTAG连接
2. 重启Vivado
3. 检查硬件服务器状态:
   ps aux | grep hw_server
```

### 问题2: 器件未找到

**解决方案**:
```
1. 检查JTAG链路
2. 重新连接JTAG
3. 在Tcl Console中执行:
   get_hw_targets
```

### 问题3: 无法找到AXI接口

**说明**:
- jtag_axi IP不支持通过标准AXI接口访问
- 需要使用其他调试方法 (VIO/ILA)

---

## 📞 需要帮助?

如果GUI操作遇到问题，可以：

1. 查看Vivado Console的详细错误信息
2. 参考 `doc/HARDWARE_DEBUG_ALTERNATIVES.md` 了解替代方案
3. 考虑添加VIO或ILA IP进行更方便的调试

---

## 📚 相关文档

- [硬件调试替代方案](HARDWARE_DEBUG_ALTERNATIVES.md)
- [BRAM验证指南](BRAM_BOARD_VALIDATION_GUIDE.md)
- [HLS IP接口映射](../doc/BRAM_INTERFACE_MAPPING.md)

---

**提示**: 如果GUI方法受限，建议采用添加VIO IP的方案，可以大幅简化测试流程。