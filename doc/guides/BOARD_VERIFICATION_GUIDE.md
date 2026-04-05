# FPGA-Litho 板级验证指南

> 创建日期: 2026-04-03  
> 目标: 完成FPGA硬件验证，确认实际性能

---

## 一、板级验证流程概览

```
┌─────────────────────────────────────────────────────────────────┐
│                    板级验证完整流程                              │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  Step 1: Vivado IP集成                                          │
│    └── 导入HLS IP到Vivado项目                                   │
│    └── 创建Block Design                                         │
│    └── 连接AXI接口和时钟                                        │
│                                                                 │
│  Step 2: 生成比特流                                             │
│    ├── 综合 (Synthesis)                                         │
│    ├── 实现 (Implementation)                                    │
│    └── 生成比特流 (Generate Bitstream)                          │
│                                                                 │
│  Step 3: 硬件准备                                               │
│    ├── FPGA板卡设置                                             │
│    ├── XRT环境安装                                              │
│    └── 设备检测                                                 │
│                                                                 │
│  Step 4: 内核加载测试                                           │
│    ├── 加载xclbin文件                                           │
│    ├── 内核功能验证                                             │
│    └── 基础执行测试                                             │
│                                                                 │
│  Step 5: 性能基准测试                                           │
│    ├── 执行时间测量                                             │
│    ├── 数据传输延迟                                             │
│    └── 与CPU版本对比                                            │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

## 二、硬件要求

### 2.1 FPGA板卡

本项目针对以下器件开发：

| 项目     | 规格               |
| -------- | ------------------ |
| 目标器件 | xcku3p-ffvb676-2-e |
| FPGA系列 | Kintex UltraScale+ |
| BRAM总量 | 720 (当前使用85%)  |
| DSP总量  | 1368 (当前使用6%)  |

**推荐开发板**:
- Xilinx KCU105 (Kintex UltraScale+ Evaluation Platform)
- 或任何基于xcku3p/xcku5p的开发板

### 2.2 主机系统

| 项目     | 要求                            |
| -------- | ------------------------------- |
| 操作系统 | Linux (Ubuntu 20.04+) / Windows |
| XRT版本  | 2025.2 或匹配Vitis版本          |
| PCIe     | Gen3 x8 或更高                  |
| 内存     | 16GB+ DDR4                      |

### 2.3 软件环境

```bash
# Linux环境检查
xbutil scan                    # 检测FPGA设备
xbutil version                 # 验证XRT版本

# Windows环境检查
xrt-smi.exe scan               # 检测设备
```

---

## 三、Step 1: Vivado IP集成

### 3.1 创建Vivado项目

```tcl
# vivado_create_project.tcl

# 创建项目
create_project litho_vivado ./litho_vivado -part xcku3p-ffvb676-2-e

# 设置IP仓库路径
set_property ip_repo_paths {
    ../hls_litho_system_proj/solution1/impl/ip
} [current_project]
update_ip_catalog

# 创建Block Design
create_bd_design "litho_system"

# 添加Zynq PS或MicroBlaze (根据板卡类型)
# KCU105示例: 使用PCIe接口
```

### 3.2 添加HLS IP

```tcl
# 添加HLS IP到Block Design
create_bd_cell -type ip -vlnv fpga-litho.org:hls:hls_litho_system:1.0 hls_litho_0

# 查看IP接口
report_ip_status [get_ips hls_litho_system_0]
```

### 3.3 接口连接

HLS IP接口说明:

| 接口名        | 类型       | 功能        | 连接目标    |
| ------------- | ---------- | ----------- | ----------- |
| s_axi_control | AXI-Lite   | 控制/配置   | PS/PCIe CPU |
| m0            | AXI-Master | source数据  | DDR/BRAM    |
| m1            | AXI-Master | mask数据    | DDR/BRAM    |
| m2            | AXI-Master | kernels数据 | DDR/BRAM    |
| m3            | AXI-Master | scales数据  | DDR/BRAM    |
| m4            | AXI-Master | tcc输出     | DDR/BRAM    |
| m5            | AXI-Master | imgf输出    | DDR/BRAM    |
| m6            | AXI-Master | img_out输出 | DDR/BRAM    |

```tcl
# 连接AXI接口示例
# 连接控制接口
create_bd_cell -type ip -vlnv xilinx.com:ip:axi_interconnect:2.1 axi_interconnect_0
set_property CONFIG.NUM_MI 1 [get_bd_cells axi_interconnect_0]
connect_bd_intf_net [get_bd_intf_pins s_axi_control] [get_bd_intf_pins axi_interconnect_0/M00_AXI]

# 连接时钟
connect_bd_net [get_bd_pins ap_clk] [get_bd_pins clk_wiz_0/clk_out1]
connect_bd_net [get_bd_pins ap_rst_n] [get_bd_pins proc_sys_reset_0/peripheral_aresetn]

# 连接AXI Master到DDR (通过AXI SmartConnect)
create_bd_cell -type ip -vlnv xilinx.com:ip:smartconnect:1.0 smartconnect_0
set_property CONFIG.NUM_MI 7 [get_bd_cells smartconnect_0]
set_property CONFIG.NUM_SI 1 [get_bd_cells smartconnect_0]

# 逐个连接m0-m6到DDR控制器
foreach i {0 1 2 3 4 5 6} {
    connect_bd_intf_net [get_bd_intf_pins m$i] [get_bd_intf_pins smartconnect_0/S00_AXI]
}
connect_bd_intf_net [get_bd_intf_pins smartconnect_0/M00_AXI] [get_bd_intf_pins axi_noc_0/S00_AXI]
```

### 3.4 自动化脚本

创建完整的Vivado项目脚本:

```tcl
# script/vivado_integration.tcl

# 打开现有项目或创建新项目
if {[file exists ./litho_vivado/litho_vivado.xpr]} {
    open_project ./litho_vivado/litho_vivado.xpr
} else {
    create_project litho_vivado ./litho_vivado -part xcku3p-ffvb676-2-e
}

# 添加IP仓库
set_property ip_repo_paths [list \
    "../FPGA-Litho/hls_litho_system_proj/solution1/impl/ip" \
] [current_project]
update_ip_catalog

# 创建Block Design
source script/create_bd.tcl

# 生成HDL
generate_target all [get_files litho_vivado.srcs/sources_1/bd/litho_system/litho_system.bd]

# 创建HDL Wrapper
make_wrapper -files [get_files litho_vivado.srcs/sources_1/bd/litho_system/litho_system.bd] -top
add_files -norecurse litho_vivado.gen/sources_1/bd/litho_system/hdl/litho_system_wrapper.v

# 保存项目
save_project_as litho_vivado
```

---

## 四、Step 2: 生成比特流

### 4.1 综合与实现

```tcl
# vivado_build.tcl

# 打开项目
open_project ./litho_vivado/litho_vivado.xpr

# 运行综合
launch_runs synth_1 -jobs 4
wait_on_run synth_1

# 检查综合结果
open_run synth_1
report_utilization -file reports/synth_utilization.rpt
report_timing_summary -file reports/synth_timing.rpt

# 运行实现
launch_runs impl_1 -jobs 4
wait_on_run impl_1

# 检查实现结果
open_run impl_1
report_utilization -file reports/impl_utilization.rpt
report_timing_summary -file reports/impl_timing.rpt

# 生成比特流
launch_runs impl_1 -to_step write_bitstream -jobs 4
wait_on_run impl_1

# 输出比特流位置
puts "Bitstream: ./litho_vivado/litho_vivado.runs/impl_1/litho_system_wrapper.bit"
```

### 4.2 Shell执行

```bash
# 在Vivado Tcl Shell中执行
vivado -mode tcl -source script/vivado_build.tcl

# 或在Vivado GUI中
# Flow Navigator -> Generate Bitstream
```

### 4.3 预估时间

| 步骤   | 预估时间  |
| ------ | --------- |
| 综合   | 15-30分钟 |
| 实现   | 30-60分钟 |
| 比特流 | 5-10分钟  |
| 总计   | ~1小时    |

---

## 五、Step 3: XRT内核打包

### 5.1 生成xclbin

对于XRT流程，需要生成xclbin文件：

```bash
# Linux环境

# 1. 从HLS导出XO文件 (如果使用Vitis流程)
# 注意: 当前已导出为Vivado IP格式，需转换

# 2. 使用Vitis Linker生成xclbin
v++ -l -t hw --config vitis_link.cfg \
    -o hls_litho_system.xclbin \
    hls_litho_system.xo

# vitis_link.cfg内容:
[connectivity]
nk=hls_litho_system:1:hls_litho_0
sp=hls_litho_0.m0:DDR[0]
sp=hls_litho_0.m1:DDR[0]
sp=hls_litho_0.m2:DDR[0]
sp=hls_litho_0.m3:DDR[0]
sp=hls_litho_0.m4:DDR[0]
sp=hls_litho_0.m5:DDR[0]
sp=hls_litho_0.m6:DDR[0]
```

### 5.2 Vivado流程替代方案

如果使用传统Vivado流程（PCIe卡）：

```bash
# 生成XRT兼容的xclbin
# 需要使用xclbinutil工具

xclbinutil --input ./litho_vivado.runs/impl_1/litho_system_wrapper.bit \
           --output hls_litho_system.xclbin \
           --add-kernel hls_litho_system \
           --kernel-name hls_litho_system_0
```

---

## 六、Step 4: 硬件准备与内核加载

### 6.1 FPGA设备检测

```bash
# Linux
xbutil scan

# 预期输出:
# Card Type: KCU105
# Card Name: kcu105_xcku3p...
# Status: Ready

# 检查详细信息
xbutil examine
```

### 6.2 加载比特流

```bash
# 方法1: 使用xbutil
xbutil program --device [device_id] --bitstream hls_litho_system.bit

# 方法2: 使用xclbin
xbutil program --device [device_id] --xclbin hls_litho_system.xclbin
```

### 6.3 验证内核加载

```bash
# 检查内核状态
xbutil examine --device [device_id] --report compute-unit

# 应显示:
# Compute Units: hls_litho_system_0
# Status: Enabled
```

---

## 七、Step 5: 运行主机程序

### 7.1 编译主机程序

```bash
cd host

# XRT C++主机 (Linux)
make clean
make

# 编译输出: litho_host
```

### 7.2 执行测试

```bash
# 基础执行测试
./litho_host -x ../hls_litho_system.xclbin -k hls_litho_system -m 1

# 参数说明:
# -x: xclbin文件路径
# -k: 内核名称
# -m: 模式 (1=TCC, 2=SOCS)
# -d: 设备ID (可选)

# 预期输出:
# Loading xclbin...
# Device opened: kcu105_xcku3p
# Kernel created: hls_litho_system
# Running TCC mode...
# Execution time: XXX ms
# Output validation: PASS
```

### 7.3 Python主机测试

```bash
# Python快速测试
python litho_host.py --xclbin ../hls_litho_system.xclbin --mode 1

# 调试模式
python litho_host.py --xclbin ../hls_litho_system.xclbin --mode 1 --debug
```

---

## 八、Step 6: 性能基准测试

### 8.1 执行时间测量

```bash
# 多次运行取平均
for i in {1..10}; do
    ./litho_host -x hls_litho_system.xclbin -k hls_litho_system -m 1 --benchmark
done

# 记录结果:
# - 内核执行时间 (Kernel Execution)
# - 数据传输时间 (Data Transfer)
# - 总执行时间 (Total)
```

### 8.2 性能分析脚本

```python
# host/benchmark.py

import pyxrt
import time
import numpy as np

def benchmark_litho(xclbin_path, mode, iterations=100):
    # 加载设备
    device = pyxrt.device(0)
    xclbin = pyxrt.xclbin(xclbin_path)
    uuid = device.load_xclbin(xclbin)
    kernel = pyxrt.kernel(device, uuid, "hls_litho_system")
    
    # 准备数据
    # ...
    
    # 计时测试
    times = []
    for i in range(iterations):
        start = time.time()
        # 执行内核
        kernel.call(...)
        end = time.time()
        times.append(end - start)
    
    # 统计结果
    print(f"Average time: {np.mean(times)*1000:.2f} ms")
    print(f"Min time: {np.min(times)*1000:.2f} ms")
    print(f"Max time: {np.max(times)*1000:.2f} ms")
    print(f"Std dev: {np.std(times)*1000:.2f} ms")

if __name__ == "__main__":
    benchmark_litho("hls_litho_system.xclbin", 1, 100)
```

### 8.3 CPU对比测试

```bash
# 运行CPU参考实现
cd ../CPP_project/FPGA-Litho-TCC
./klitho_tcc --benchmark

# 记录CPU执行时间
# 与FPGA结果对比计算加速比
```

---

## 九、常见问题排查

### 9.1 设备未检测到

```bash
# 检查PCIe连接
lspci | grep Xilinx

# 检查驱动
lsmod | grep xocl

# 重装驱动
sudo /opt/xilinx/xrt/bin/xbutil reset --device all
```

### 9.2 比特流加载失败

```bash
# 检查比特流格式
file hls_litho_system.bit

# 检查设备状态
xbutil examine --device 0 --report hardware

# 清空设备重新加载
xbutil reset --device 0
xbutil program --device 0 --bitstream hls_litho_system.bit
```

### 9.3 内核执行超时

```bash
# 检查时钟配置
xbutil examine --device 0 --report clocks

# 检查内存分配
xbutil examine --device 0 --report memory

# 增加超时时间
./litho_host --timeout 60000  # 60秒超时
```

---

## 十、检查清单

板级验证完成标准:

| 检查项                  | 状态 |
| ----------------------- | ---- |
| Vivado项目创建成功      | [ ]  |
| HLS IP导入成功          | [ ]  |
| Block Design连接正确    | [ ]  |
| 综合通过 (无错误)       | [ ]  |
| 实现通过 (时序满足)     | [ ]  |
| 比特流生成成功          | [ ]  |
| FPGA设备检测成功        | [ ]  |
| 比特流加载成功          | [ ]  |
| 内核功能验证 (输出正确) | [ ]  |
| 性能基准测试完成        | [ ]  |
| 加速比计算完成          | [ ]  |

---

## 十一、下一步优化方向

完成板级验证后，可考虑以下优化：

1. **数据传输优化**
   - 使用多缓冲技术减少传输延迟
   - PCIe带宽优化

2. **内存带宽优化**
   - DDR访问模式优化
   - 使用HBM (如有)

3. **精度验证**
   - 与CPU版本误差分析
   - 数值稳定性检查

4. **功耗分析**
   - 使用Vivado Power Report
   - 功耗优化建议

---

> **提示**: 如无FPGA硬件，可考虑使用Vitis HLS的C/RTL协同仿真结果作为初步验证，等待硬件资源后再进行完整板级验证。