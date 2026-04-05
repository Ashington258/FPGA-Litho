# FPGA-Litho FPGA开发完整工作流文档

**项目**: FPGA-Litho Lithography Simulation System  
**版本**: v1.0  
**日期**: 2026-04-05  
**状态**: Phase 7验证完成，准备Phase 8资源优化

---

## 目录

1. [工作流概览](#工作流概览)
2. [阶段一：HLS算法开发](#阶段一hls算法开发)
3. [阶段二：HLS综合与验证](#阶段二hls综合与验证)
4. [阶段三：Vivado IP集成](#阶段三vivado-ip集成)
5. [阶段四：硬件实现](#阶段四硬件实现)
6. [阶段五：板级验证](#阶段五板级验证)
7. [Phase 7实战经验](#phase-7实战经验)
8. [常用命令速查表](#常用命令速查表)

---

## 工作流概览

### 整体流程图

```
┌─────────────────────────────────────────────────────────────┐
│                    FPGA-Litho FPGA开发完整流程                   │
└─────────────────────────────────────────────────────────────┘

阶段一：HLS算法开发
┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐
│ C++代码  │→│ 头文件   │→│ Testbench │→│ 参数配置 │
│ 编写     │  │ 定义     │  │ 编写      │  │ 优化     │
└──────────┘  └──────────┘  └──────────┘  └──────────┘
     ↓              ↓              ↓              ↓
   src/          include/      testbench/    script/config/

阶段二：HLS综合与验证
┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐
│ C仿真    │→│ C综合    │→│ Co-Sim   │→│ IP导出   │
│ (csim)   │  │ (csynth) │  │ 验证     │  │ (export) │
└──────────┘  └──────────┘  └──────────┘  └──────────┘
     ↓              ↓              ↓              ↓
 vitis-run      vitis-run      vitis-run      vitis-run
  --csim        --tcl         --cosim        --package

阶段三：Vivado IP集成
┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐
│ 导入IP   │→│ Block    │→│ Address  │→│ Constraints│
│ Catalog  │  │ Design   │  │ Map      │  │ 设置      │
└──────────┘  └──────────┘  └──────────┘  └──────────┘
     ↓              ↓              ↓              ↓
 IP Catalog    BD Diagram    AXI Interconnect  XDC文件

阶段四：硬件实现
┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐
│ Vivado   │→│ Vivado   │→│ Bitstream │→│ Hardware │
│ Synth    │  │ Impl     │  │ 生成      │  │ Manager  │
└──────────┘  └──────────┘  └──────────┘  └──────────┘
     ↓              ↓              ↓              ↓
  RTL生成       时序收敛       .bit文件       JTAG连接

阶段五：板级验证
┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐
│ TCL测试  │→│ 寄存器   │→│ 数据流   │→│ 结果     │
│ 脚本     │  │ 验证     │  │ 测试     │  │ 分析     │
└──────────┘  └──────────┘  └──────────┘  └──────────┘
     ↓              ↓              ↓              ↓
 bram_test.tcl  AXI读写      算法计算      输出验证
```

### 关键技术栈

| 工具 | 版本 | 用途 |
|------|------|------|
| **Vitis HLS** | 2025.2 | 高层次综合 |
| **Vivado** | 2024.1 | RTL综合与实现 |
| **Python** | 3.x | 自动化脚本 |
| **TCL** | 8.6 | 硬件测试脚本 |
| **Git** | 2.x | 版本控制 |

---

## 阶段一：HLS算法开发

### 1.1 代码结构

```
项目根目录/
├── src/                # HLS C++源文件
│   ├── hls_litho_system_bram.cpp    # BRAM版本主函数
│   ├── hls_socs.cpp                 # SOCS算法实现
│   ├── hls_tcc.cpp                  # TCC算法实现
│   └── hls_calc_image_integrated.cpp
├── include/            # 头文件定义
│   ├── hls_litho_system_bram.h      # BRAM版本接口
│   ├── hls_types.h                  # 数据类型定义
│   ├── hls_socs.h                   # SOCS接口
│   └── hls_tcc.h                    # TCC接口
├── testbench/          # 测试平台
│   ├── litho_system_bram_tb.cpp     # BRAM版Testbench
│   ├── socs_tb.cpp                  # SOCS测试
│   └── tcc_tb_hls.cpp               # TCC测试
└── script/             # 脚本目录
    ├── hls/           # HLS脚本
    ├── config/        # 配置文件
    └── verify/        # 验证脚本
```

### 1.2 HLS C++开发规范

**核心设计原则**:

```cpp
// 1. 顶层函数接口设计
void hls_litho_system_bram(
    int operation,     // 操作码 (0-9)
    int idx,           // 数组索引
    int val_r,         // 数据值（实部）
    int val_i,         // 数据值（虚部）
    int mode,          // 计算模式
    int Lx, int Ly,    // 频域尺寸
    int Nx, int Ny,    // TCC/SOCS参数
    int srcsize,       // 光源尺寸
    int nkernels,      // SOCS核数量
    int *status_r,     // 返回状态（实部）
    int *status_i      // 返回状态（虚部）
) {
    // 2. BRAM存储声明（关键优化点）
    complex<int> source_bram[100];         // pragma ARRAY_PARTITION
    complex<int> mask_bram[4096];          // 分区策略决定BRAM使用
    complex<int> tcc_bram[961];
    complex<int> kernels_bram[225];
    float scales_bram[8];
    complex<int> imgf_bram[4096];          // ⚠️ Phase 7发现超标源
    float img_out_bram[4096];
    
    // 3. 操作分支设计
    switch(operation) {
        case OP_LOAD_SOURCE:    // 加载光源数据
        case OP_LOAD_MASK:      // 加载mask数据
        case OP_COMPUTE_SOCS:   // SOCS计算（关键算法）
        case OP_RESET:          // 重置BRAM
        // ... 其他操作
    }
}

// 4. SOCS算法实现（Phase 7修复示例）
void compute_socs_mode(/* 参数 */) {
    // 步骤1: Copy mask到本地缓存
    // 步骤2: 初始化product和累加器
    // 步骤3: Kernel-Mask复数乘法（正确2D索引映射）
    //   int krn_idx = (y + Ny) * (2 * Nx + 1) + (x + Nx);  ✅
    //   int msk_idx = (y + Lyh) * Lx + (x + Lxh);          ✅
    // 步骤4: 平方幅度累加
    // 步骤5: 循环移位 + 输出截取
    hls_shift_array_real_bram(img_accum, img_shifted, ...);  ✅
}
```

### 1.3 关键pragma指令

```cpp
// 性能优化pragma
#pragma HLS PIPELINE II=1              // 流水线化
#pragma HLS UNROLL factor=4           // 循环展开
#pragma HLS ARRAY_PARTITION cyclic factor=8 variable=mask_bram  // 数组分区
#pragma HLS DATAFLOW                   // 数据流优化
#pragma HLS INLINE                     // 函数内联

// 接口pragma
#pragma HLS INTERFACE s_axilite port=operation bundle=control
#pragma HLS INTERFACE s_axilite port=return bundle=control
```

**⚠️ Phase 7经验教训**:
- `ARRAY_PARTITION`策略直接影响BRAM资源使用
- imgf_bram分区过多导致139块BRAM（超标34块）
- 需平衡性能与资源约束

---

## 阶段二：HLS综合与验证

### 2.1 工具链选择

**Vitis HLS 2025.2工具链**:

```bash
# 关键发现：Vitis 2025.2使用vitis-run命令
# 而非传统的vitis_hls命令

# 工具路径
VITIS_ROOT=/root/AMDDesignTools/2025.2/Vitis
VITIS_BIN=$VITIS_ROOT/bin

# 主要命令
vitis-run --mode hls --csim --config <config.cfg>
vitis-run --mode hls --tcl --input_file <script.tcl>
vitis-run --mode hls --cosim --config <config.cfg>
vitis-run --mode hls --package --config <config.cfg>
```

### 2.2 C仿真验证（Phase 7实际流程）

#### 配置文件准备

**`script/config/hls_config_bram_vitis.cfg`**:
```ini
part=xcku3p-ffvb676-2-e

[hls]
clock=5ns
flow_target=vitis
syn.file=src/hls_litho_system_bram.cpp
syn.file=include/hls_litho_system_bram.h
syn.file=include/hls_types.h
syn.top=hls_litho_system_bram
tb.file=testbench/litho_system_bram_tb.cpp
csim.clean=0
```

#### 执行C仿真

```bash
# Phase 7成功执行命令
cd /root/project/FPGA/vitis/FPGA-Litho
$VITIS_BIN/vitis-run \
  --mode hls \
  --csim \
  --config script/config/hls_config_bram_vitis.cfg \
  --work_dir hls_litho_system_bram_proj

# 输出日志位置
hls_litho_system_bram_proj/hls/csim/report/hls_litho_system_bram_csim.log
```

#### Phase 7 C仿真验证结果

```
INFO: [SIM 2] *************** CSIM start ***************
========================================
FPGA-Litho BRAM Single-Function Testbench
========================================

Test 1: Load Source Data (OP_LOAD_SOURCE)          [PASS]
Test 2: Load Mask Data (OP_LOAD_MASK)              [PASS]
Test 3: Load TCC Data (OP_LOAD_TCC)                [PASS]
Test 4: Load Kernels Data (OP_LOAD_KERNELS)        [PASS]
Test 5: Compute TCC Mode (OP_COMPUTE_TCC)          [PASS] (status=1.0)
Test 6: Read imgf Result (OP_READ_IMGF)            [PASS] (imgf[0]=(0,125))
Test 7: Compute SOCS Mode (OP_COMPUTE_SOCS)        [PASS] (status=1.0) ✅
Test 8: Read img_out Result (OP_READ_IMG_OUT)      [PASS] (img_out[0]=43750) ✅
Test 9: Parameter Validation                        [PASS] ⚠️
Test 10: Reset BRAM Storage (OP_RESET)             [PASS]

Passed: 10/10
*** ALL TESTS PASSED ***
```

**关键验证点**:
- Test 8输出值变化：122500 → **43750**（证明算法修复）
- 所有测试100%通过
- 执行时间：11.12秒

### 2.3 C综合验证

#### 执行C综合

```bash
# Phase 7实际执行
$VITIS_BIN/vitis-run \
  --mode hls \
  --tcl \
  --input_file script/hls/run_csynth_bram.tcl \
  --work_dir hls_litho_system_bram_proj

# 或使用配置文件方式
vivado_hls -f script/hls/run_csynth_bram.tcl  # 传统方式（不适用Vitis 2025.2）
```

#### 综合报告分析

**关键性能指标**:

```
================================================================
== Synthesis Summary Report
================================================================
| Performance Metrics           | Value         | Target      | Status |
|-------------------------------|---------------|-------------|--------|
| Estimated Fmax                | 287.11 MHz    | ≥200MHz     | ✅ +87MHz |
| Slack                         | 0.17ns        | ≥0ns        | ✅ Positive |
| Loop Constraints              | All satisfied | -           | ✅ No violations |

================================================================
== Utilization Estimates
================================================================
| Resource  | Used    | Available | Utilization | Status |
|-----------|---------|-----------|-------------|--------|
| BRAM_18K  | 139     | 720       | 19%         | ⚠️ 超标34块 |
| DSP       | 34      | 1368      | 2%          | ✅ OK |
| FF        | 10,183  | 325,440   | 3%          | ✅ OK |
| LUT       | 14,098  | 162,720   | 8%          | ✅ OK |
| URAM      | 0       | 48        | 0%          | ⚠️ 优化空间 |
```

**BRAM超标分析**（Phase 7发现）:

```
Memory Usage Breakdown:
├── imgf_bram:     112块 (80%)  ⚠️ 主要超标源
├── kernels_bram:  16块  (12%)
├── mask_bram:     16块  (12%)
├── tcc_bram:      2块   (1%)
├── scales_bram:   1块   (1%)
└── img_out_bram:  1块   (1%)
└── Total:         139块

超标原因: imgf_bram ARRAY_PARTITION过多
解决方向: Phase 8资源优化
```

#### 综合报告文件位置

```
hls_litho_system_bram_proj/solution1/syn/report/
├── csynth.rpt                          # 综合主报告
├── hls_litho_system_bram_csynth.rpt    # 详细资源报告
├── csynth_design_size.rpt              # 设计规模报告
└── hls_litho_system_bram.ip_user_files/ # IP导出文件
```

### 2.4 Co-Simulation验证（可选）

```bash
# RTL协同仿真（验证HLS生成的RTL正确性）
$VITIS_BIN/vitis-run \
  --mode hls \
  --cosim \
  --config script/config/hls_config_bram_vitis.cfg \
  --work_dir hls_litho_system_bram_proj

# 输出位置
hls_litho_system_bram_proj/solution1/sim/report/
```

### 2.5 IP导出

#### TCL脚本方式

**`script/hls/run_package_bram.tcl`**:
```tcl
open_project hls_litho_system_bram_proj
open_solution solution1

# 导出为Vitis Kernel格式
config_export -format ip_catalog
export_design

exit
```

#### 执行IP导出

```bash
# 使用vitis-run（Vitis 2025.2）
$VITIS_BIN/vitis-run \
  --mode hls \
  --package \
  --config script/config/hls_config_bram_vitis.cfg \
  --work_dir hls_litho_system_bram_proj

# 输出文件位置
hls_litho_system_bram_proj/solution1/impl/export/ip/
├── hls_litho_system_bram.xilinx.com_user_hls_litho_system_bram_1.0.zip
└── xgui/
    └── hls_litho_system_bram.tcl
```

---

## 阶段三：Vivado IP集成

### 3.1 Vivado项目创建

#### 创建新项目

```bash
# 启动Vivado 2024.1
vivado

# TCL方式创建项目
vivado -mode tcl
create_project test_bram_litho ~/project/FPGA/vivado/test_bram_litho -part xcku3p-ffvb676-2-e
```

### 3.2 导入HLS IP

#### IP Catalog导入

```tcl
# 方法1: Vivado GUI
# 1. 打开 IP Catalog (Window -> IP Catalog)
# 2. 点击 "Import IP"
# 3. 选择 HLS导出的ZIP文件:
#    hls_litho_system_bram_proj/solution1/impl/export/ip/*.zip

# 方法2: TCL命令
set_property ip_repo_paths {
  /root/project/FPGA/vitis/FPGA-Litho/hls_litho_system_bram_proj/solution1/impl/export/ip
} [current_project]
update_ip_catalog
```

#### 验证IP导入成功

```tcl
# 检查IP是否在Catalog中
report_ip_status

# 应显示:
# IP: hls_litho_system_bram (1.0)
# Status: Available
```

### 3.3 Block Design创建

#### 创建Block Design

```tcl
# 创建BD
create_bd_design "litho_system_bd"

# 添加Zynq/Kintex处理单元（根据器件选择）
# 对于xcku3p，使用MicroBlaze或AXI主端口
create_bd_cell -type ip -vlnv xilinx.com:ip:axi_interconnect:2.1 axi_interconnect_0

# 添加HLS IP核心
create_bd_cell -type ip -vlnv xilinx.com:user:hls_litho_system_bram:1.0 hls_litho_system_bram_0

# 添加AXI BRAM控制器（可选）
create_bd_cell -type ip -vlnv xilinx.com:ip:axi_bram_ctrl:4.1 axi_bram_ctrl_0
```

#### BD连接配置

```
Block Design拓扑结构:
┌─────────────────────────────────────────────────────┐
│                   AXI Interconnect                   │
│  (M00_AXI) ──→ [hls_litho_system_bram_0] (S00_AXI) │
│                                                      │
│  配置:                                               │
│  - AXI Lite接口 (控制寄存器)                         │
│  - 时钟域同步                                        │
│  - Address Map配置                                   │
└─────────────────────────────────────────────────────┘

关键连接:
1. clock: processing_system_reset → hls_ip.ap_clk
2. reset: proc_sys_reset → hls_ip.ap_rst_n
3. axi_lite: axi_interconnect.M00_AXI → hls_ip.s_axi_control
```

#### Address Map配置

```tcl
# 自动分配地址
assign_bd_address [get_bd_addr_segs hls_litho_system_bram_0/s_axi_control/Reg]
validate_bd_design

# 查看地址映射
report_bd_address

# 输出示例:
# hls_litho_system_bram_0/s_axi_control/Reg: 0x40000000-0x40000FFF
```

### 3.4 生成HDL Wrapper

```tcl
# 生成顶层HDL
generate_target all [get_files litho_system_bd.bd]
make_wrapper -files [get_files litho_system_bd.bd] -top
add_files -norecurse ~/project/FPGA/vivado/test_bram_litho/litho_system_bd_wrapper.v

# 验证HDL生成成功
report_compile_order
```

---

## 阶段四：硬件实现

### 4.1 Vivado Synthesis

#### Synthesis设置

```tcl
# Synthesis策略
set_property strategy "Vivado Synthesis Defaults" [get_runs synth_1]
set_property STEPS.SYNTH_DESIGN.ARGS.FLATTEN_HIERARCHY rebuilt [get_runs synth_1]

# 启动Synthesis
launch_runs synth_1 -jobs 4
wait_on_run synth_1

# 检查Synthesis结果
report_timing_summary -file synth_timing.rpt
report_utilization -file synth_utilization.rpt
```

#### Synthesis验证

```bash
# 输出文件位置
~/project/FPGA/vivado/test_bram_litho/test_bram_litho.runs/synth_1/
├── runme.log                # 运行日志
├── hls_litho_system_bram_0_synth.rpt  # IP综合报告
└── synthesis.dcp           # 综合checkpoint
```

### 4.2 Vivado Implementation

#### Implementation设置

```tcl
# Implementation策略
set_property strategy "Vivado Implementation Defaults" [get_runs impl_1]

# 启动Implementation
launch_runs impl_1 -jobs 4
wait_on_run impl_1

# Open Implemented Design
open_run impl_1

# 时序检查
report_timing_summary -delay_type min_max -report_type full \
  -file impl_timing_summary.rpt

# 资源检查
report_utilization -file impl_utilization.rpt
```

#### Implementation验证

```
Timing Summary (应满足):
├── Setup Slack: ≥0ns
├── Hold Slack:  ≥0ns
├── Pulse Width: All positive
└── Fmax: ≥200MHz (目标频率)

Utilization Summary (应满足约束):
├── BRAM: ≤105块 (⚠️ Phase 7发现超标需优化)
├── DSP: ≤200块
├── FF: ≤100,000
└── LUT: ≤50,000
```

### 4.3 Bitstream生成

#### 生成Bitstream

```tcl
# 生成.bit文件
launch_runs impl_1 -to_step write_bitstream -jobs 4
wait_on_run impl_1

# 输出文件位置
~/project/FPGA/vivado/test_bram_litho/test_bram_litho.runs/impl_1/
├── test_bram_litho.bit      # Bitstream文件
├── test_bram_litho.ltx      # Debug probes文件
└── hw_handoff/              # Hardware handoff文件
```

#### 导出Hardware

```tcl
# 导出硬件定义（用于Vitis软件开发）
write_hw_platform -fixed -force \
  -include_bit -include_ip_info \
  test_bram_litho.xsa

# Hardware Handoff文件位置
hw_handoff/
├── test_bram_litho.hdf      # Hardware Definition File
├── test_bram_litho_sysdef.xml
└── ps7_init.tcl             # 处理器初始化脚本（Zynq系列）
```

---

## 阘段五：板级验证

### 5.1 Hardware Manager连接

#### 启动Hardware Manager

```bash
# 方法1: Vivado GUI
# Tools -> Open Hardware Manager

# 方法2: TCL模式
vivado -mode tcl
open_hw_manager
```

#### 连接硬件服务器

```tcl
# 连接本地硬件服务器
connect_hw_server -url localhost:3121 -allow_automatic_start

# 打开硬件目标
open_hw_target [lindex [get_hw_targets] 0]

# 验证连接成功
current_hw_device [lindex [get_hw_devices] 0]
refresh_hw_device [current_hw_device]

# 输出示例:
# Hardware Target: localhost:3121/xilinx_tcf
# Hardware Device: xcku3p
# Status: Connected ✅
```

### 5.2 下载Bitstream

```tcl
# 编程FPGA
set_property PROGRAM.FILE {
  ~/project/FPGA/vivado/test_bram_litho/test_bram_litho.runs/impl_1/test_bram_litho.bit
} [get_hw_devices xcku3p]

# 下载Debug Probes（可选）
set_property PROGRAM.LTX {
  ~/project/FPGA/vivado/test_bram_litho/test_bram_litho.runs/impl_1/test_bram_litho.ltx
} [get_hw_devices xcku3p]

# 编程FPGA
program_hw_devices [get_hw_devices xcku3p]

# 验证编程成功
# 应显示: Device programmed successfully ✅
```

### 5.3 TCL测试脚本执行

#### 推荐测试脚本：`script/verify/bram_test.tcl`

**基于Phase 7验证的实际测试流程**:

```bash
# 方法1: Vivado TCL模式（推荐）
cd /root/project/FPGA/vitis/FPGA-Litho
vivado -mode tcl -source script/verify/bram_test.tcl

# 方法2: Hardware Manager TCL Console
# 在Vivado GUI中: Window -> Hardware Manager -> Tcl Console
source script/verify/bram_test.tcl
```

#### 测试脚本核心功能

```tcl
# script/verify/bram_test.tcl 结构

# 1. 硬件初始化
puts "步骤1-7: 硬件初始化"
connect_hw_server -url localhost:3121
open_hw_target
program_hw_devices [get_hw_devices]

# 2. AXI核心访问
set hw_axi_1 [get_hw_axis hw_axi_1]
create_hw_axi_txn wr_txn [get_hw_axis hw_axi_1] \
  -address 0x40000000 -data 0x00000004 -len 1 -type write

# 3. CONTROL寄存器验证
puts "步骤8: CONTROL寄存器验证"
# 读AP_CTRL (地址0x00)
# 验证 ap_idle=1 (bit[2]=1)
# Expected: 0x00000004 ✅

# 4. 重置操作测试 (OP_RESET=9)
puts "步骤9: 重置BRAM存储"
# 写OPERATION寄存器 (0x1c) = 9
# 启动IP (写AP_START bit[0]=1)
# 读返回值 status_r, status_i
# Expected: status=1.0 (IEEE754: 0x3f800000) ✅

# 5. 数据加载测试
puts "步骤10-11: 光源数据加载"
# OP_LOAD_SOURCE (operation=0)
# 写入测试数据: val_r=100, val_i=50
# 读回验证

# 6. 参数配置验证
puts "步骤12: 参数写入与读回"
# 写Lx=64, Ly=64, Nx=3, Ny=3
# 读回验证参数正确性
```

#### 寄存器映射参考

**基于 `xhls_litho_system_bram_hw.h` 官方定义**:

| 寄存器 | 地址 | 功能 | Phase 7验证值 |
|--------|------|------|--------------|
| AP_CTRL | 0x00 | 控制状态 | 0x04 (ap_idle=1) ✅ |
| OPERATION | 0x1c | 操作码选择 | 9 (OP_RESET) ✅ |
| AP_RETURN | 0x10 | 返回值 | 0x3f800000 (1.0) ✅ |
| LX | 0x40 | 频域X尺寸 | 64 ✅ |
| LY | 0x48 | 频域Y尺寸 | 64 ✅ |
| NX | 0x50 | TCC参数 | 3 ✅ |

### 5.4 完整数据流测试

#### Phase 7推荐的完整测试脚本

**`script/verify/bram_dataflow_complete.tcl`**:

```tcl
# 完整SOCS算法验证流程

# 1. 加载测试数据
source data/bram_test/config.json
load_data_from_hex data/bram_test/

# 2. 重置BRAM存储
operation_write 9  # OP_RESET
ap_start_trigger
wait_for_ap_done

# 3. 加载光源数据
operation_write 0  # OP_LOAD_SOURCE
for {set i 0} {$i < 100} {incr i} {
    idx_write $i
    val_write [lindex $source_data $i]
}

# 4. 加载Mask数据
operation_write 1  # OP_LOAD_MASK
for {set i 0} {$i < 4096} {incr i} {
    idx_write $i
    val_write [lindex $mask_data $i]
}

# 5. 加载Kernels数据
operation_write 3  # OP_LOAD_KERNELS
for {set k 0} {$k < 8} {incr k} {
    for {set i 0} {$i < 225} {incr i} {
        kernel_write $k $i [lindex $kernels_data($k) $i]
    }
}

# 6. SOCS计算
operation_write 6  # OP_COMPUTE_SOCS
Lx_write 64
Ly_write 64
nkernels_write 8
ap_start_trigger
wait_for_ap_done

# 7. 读取输出
operation_write 8  # OP_READ_IMG_OUT
for {set i 0} {$i < 100} {incr i} {
    idx_write $i
    val_read
    puts "img_out[$i] = $val_r"
}

# 8. 验证非零输出（Phase 7修复验证）
puts "验证: img_out[0] 应为 43750（非零）✅"
if {$img_out_0 != 43750} {
    puts "ERROR: 输出全零问题未解决"
} else {
    puts "✅ Phase 7修复验证成功！"
}
```

---

## Phase 7实战经验

### 7.1 问题诊断流程

**"输出全零"问题诊断路径**:

```
Phase 6发现 → Phase 7诊断 → 修复验证
└─────────────────────────────────────┘

2026-04-04: Phase 6板级测试发现输出全零
├── Test 8: img_out[0] = 0 ❌
├── 数据加载验证: 通过 ✅
└── 怀疑: 计算逻辑问题

2026-04-05: Phase 7算法诊断
├── 对比hls_socs.cpp与hls_litho_system_bram.cpp
├── 发现: BRAM版本使用简化算法
├── 根因:
│   ├── kernel_idx = i % 225 (循环取模，错误)
│   ├── 缺少循环移位步骤
│   └── 缺少正确Kernel-Mask索引映射
└── 解决: 移植完整5步算法

2026-04-05: Phase 7修复验证
├── 添加hls_shift_array_real_bram()函数
├── 替换OP_COMPUTE_SOCS分支逻辑
├── Git提交: d2463b7
└── C仿真验证: img_out[0] = 43750 ✅
```

### 7.2 工具链使用经验

**Vitis HLS 2025.2关键发现**:

```bash
# ❌ 错误方式（传统命令）
vitis_hls -f script.tcl  # 命令不存在

# ✅ 正确方式（Vitis 2025.2）
vitis-run --mode hls --csim --config config.cfg
vitis-run --mode hls --tcl --input_file script.tcl

# 工具路径
/root/AMDDesignTools/2025.2/Vitis/bin/vitis-run
```

**C仿真执行成功命令**:
```bash
cd /root/project/FPGA/vitis/FPGA-Litho
/root/AMDDesignTools/2025.2/Vitis/bin/vitis-run \
  --mode hls \
  --csim \
  --config script/config/hls_config_bram_vitis.cfg \
  --work_dir hls_litho_system_bram_proj
```

### 7.3 资源超标问题发现

**HLS C综合报告分析经验**:

```
资源检查关键步骤:
1. 查看 csynth.rpt 文件
2. 分析 Utilization Estimates 部分
3. 检查 Memory Breakdown
4. 找出超标来源

Phase 7发现:
├── imgf_bram: 112块 (80%) ⚠️
│   ├── ARRAY_PARTITION过多
│   ├── complex<int>位宽32bit
│   └── 40个分区实例
└── Total: 139块 (超标34块)

解决方向（Phase 8）:
├── 减少imgf_bram分区数量
├── 使用URAM替代部分BRAM
├── 调整数据精度 complex<short>
└── 目标: BRAM≤105块
```

### 7.4 测试策略建议

**多级验证策略**:

```
验证金字塔:
┌─────────────────────────────────┐
│   Level 4: FPGA Board Test      │  Phase 10
│   (完整数据流验证)               │  → 待执行
├─────────────────────────────────┤
│   Level 3: Vivado Hardware      │  Phase 6
│   (寄存器访问验证)               │  ✓ 完成
├─────────────────────────────────┤
│   Level 2: HLS C Synthesis       │  Phase 7
│   (资源与时序验证)               │  ✓ 完成
├─────────────────────────────────┤
│   Level 1: HLS C Simulation      │  Phase 7
│   (算法逻辑验证)                 │  ✓ 完成
└─────────────────────────────────┘

Phase 7执行顺序（成功经验）:
C仿真 → C综合 → 发现问题 → 记录待优化 → 等待资源优化
✅      ✅       ⚠️          ✅              ⏳ Phase 8
```

---

## 常用命令速查表

### HLS命令速查

```bash
# C仿真
vitis-run --mode hls --csim --config script/config/hls_config_bram_vitis.cfg --work_dir hls_proj

# C综合
vitis-run --mode hls --tcl --input_file script/hls/run_csynth_bram.tcl --work_dir hls_proj

# Co-Simulation（可选）
vitis-run --mode hls --cosim --config script/config/hls_config_bram_vitis.cfg --work_dir hls_proj

# IP导出
vitis-run --mode hls --package --config script/config/hls_config_bram_vitis.cfg --work_dir hls_proj
```

### Vivado命令速查

```tcl
# 项目创建
create_project test_bram_litho ~/project/FPGA/vivado/test_bram_litho -part xcku3p-ffvb676-2-e

# IP导入
set_property ip_repo_paths {hls_proj/solution1/impl/export/ip} [current_project]
update_ip_catalog

# Block Design
create_bd_design "litho_bd"
create_bd_cell -type ip -vlnv xilinx.com:user:hls_litho_system_bram:1.0 hls_ip_0

# Synthesis
launch_runs synth_1 -jobs 4
wait_on_run synth_1

# Implementation
launch_runs impl_1 -jobs 4
wait_on_run impl_1

# Bitstream
launch_runs impl_1 -to_step write_bitstream -jobs 4
program_hw_devices [get_hw_devices]
```

### Hardware Manager命令速查

```tcl
# 硬件连接
connect_hw_server -url localhost:3121
open_hw_target [lindex [get_hw_targets] 0]

# AXI读写
create_hw_axi_txn wr_txn [get_hw_axis hw_axi_1] -address 0x40000000 -data 0x04 -len 1 -type write
run_hw_axi_txn wr_txn

create_hw_axi_txn rd_txn [get_hw_axis hw_axi_1] -address 0x40000000 -len 1 -type read
run_hw_axi_txn rd_txn
set val [get_property DATA rd_txn]

# 测试脚本执行
source script/verify/bram_test.tcl
```

### Git版本控制

```bash
# Phase 7修复提交
git add src/hls_litho_system_bram.cpp
git commit -m "fix(hls): 修复BRAM版本SOCS算法输出全零问题"

# 查看提交历史
git log --oneline --all -10

# 查看修复内容
git show d2463b7:src/hls_litho_system_bram.cpp
```

---

## 文档维护

**更新日志**:

| 日期 | 更新内容 | 作者 |
|------|----------|------|
| 2026-04-05 | 创建完整工作流文档，基于Phase 7实战经验 | FPGA-Litho Team |
| 2026-04-05 | 添加Vitis HLS 2025.2工具链使用方法 | FPGA-Litho Team |
| 2026-04-05 | 补充BRAM资源超标分析与Phase 8优化计划 | FPGA-Litho Team |

**相关文档参考**:

- `Vitis_HLS_Refactor_TODO.md` - 项目主TODO文档
- `script/README.md` - 脚本目录分类说明
- `doc/reports/PHASE7_VERIFICATION_REPORT.md` - Phase 7验证报告
- `doc/BRAM_INTERFACE_MAPPING.md` - BRAM接口映射说明

---

**文档版本**: v1.0  
**最后更新**: 2026-04-05  
**状态**: Phase 7验证完成，准备Phase 8资源优化