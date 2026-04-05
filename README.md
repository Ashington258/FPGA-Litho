# FPGA-Litho

**FPGA加速的光刻模拟系统 - Vitis HLS高性能实现**

> 项目状态: **Phase 5 完成 (待板级验证)**  
> 目标器件: xcku3p-ffvb676-2-e (Kintex UltraScale+)  
> 系统频率: 274MHz (超过200MHz目标37%)

---

## 项目简介

FPGA-Litho是一个基于AMD Xilinx Vitis HLS的光刻模拟系统，将FPGA-Litho光学成像算法重构为HLS工程，实现FPGA硬件加速。项目采用**双模式架构**（TCC模式和SOCS模式），覆盖光刻模拟的核心计算流程。

### 目标加速比

| 模块      | 目标加速比 | 预估达成   |
| --------- | ---------- | ---------- |
| calcTCC   | 100-500x   | ✅ 待验证   |
| calcImage | 50-200x    | ✅ 待验证   |
| calcSOCS  | 30-100x    | ✅ 待验证   |
| FFT 2D    | 10-50x     | ✅ 已实现   |
| 整体流程  | 100-500x   | 🔄 板级验证 |

---

## 核心功能模块

### 🔹 双模式系统架构

```cpp
// TCC模式 (mode=1): 完整TCC矩阵计算 → 光学图像生成
//   source → TCC计算 → calcImage → imgf
//
// SOCS模式 (mode=2): 使用预计算SOCS核快速成像  
//   kernels + scales → SOCS计算 → img_out
```

### 🔹 核心计算模块

| 模块                        | 功能                         | 性能指标     |
| --------------------------- | ---------------------------- | ------------ |
| `hls_tcc`                   | TCC矩阵计算 (Pupil函数+累加) | 342MHz, II=1 |
| `hls_calc_image_integrated` | 频域光学图像计算             | 274MHz, II=4 |
| `hls_socs`                  | SOCS核光学图像计算           | 290MHz, II=1 |
| `hls_fft_r2c`               | FFT实数转复数 (1024点)       | IP核封装     |
| `hls_fft_c2r`               | FFT复数转实数 (IFFT)         | IP核封装     |
| `hls_shift`                 | 2D循环移位 (频谱中心化)      | 流水线实现   |

### 🔹 辅助生成模块

| 模块         | 功能                                      |
| ------------ | ----------------------------------------- |
| `hls_source` | 光源生成 (Annular/Dipole/CrossQuad/Point) |
| `hls_mask`   | 掩模生成 (LineSpace/Rectangle/Cross)      |

### 🔹 HLS优化技术

- **流水线化**: PIPELINE II=1 核心循环
- **并行累加**: 8通道树形归约求和
- **数组分区**: ARRAY_PARTITION 减少存储冲突
- **三角函数查找表**: sin/cos 256点插值LUT
- **本地BRAM缓存**: 减少外部存储访问延迟

## 项目结构

```
FPGA-Litho/
├── src/                          # HLS源代码
│   ├── hls_litho_system.cpp      # ⭐ 顶层系统集成 (TCC/SOCS双模式)
│   ├── hls_calc_image_integrated.cpp # 频域图像计算 (TCC模式)
│   ├── hls_socs.cpp              # SOCS图像计算
│   ├── hls_tcc.cpp               # TCC矩阵计算
│   ├── hls_fft_r2c.cpp           # FFT 实数转复数
│   ├── hls_fft_c2r.cpp           # FFT 复数转实数
│   ├── hls_shift.cpp             # 2D循环移位
│   ├── hls_source.cpp            # 光源生成
│   ├── hls_mask.cpp              # 掩模生成
│   └── hls_top.cpp               # FFT顶层接口
│
├── include/                      # 头文件
│   ├── hls_types.h               # 基础类型定义
│   ├── hls_litho_system.h        # 系统配置常量
│   ├── hls_calc_image_integrated.h
│   ├── hls_socs.h / hls_tcc.h   # 模块接口
│   └── hls_fft_*.h / hls_shift.h
│
├── testbench/                    # 测试平台
│   ├── litho_system_tb.cpp       # ⭐ 系统集成测试 (双模式)
│   ├── calc_image_integrated_tb.cpp
│   ├── socs_tb.cpp / tcc_tb_hls.cpp
│   └── fft_tb.cpp / fft_tb_simple.cpp
│
├── script/                       # HLS配置脚本
│   ├── hls_config_system.cfg     # ⭐ 系统级HLS配置
│   ├── run_csynth_system.tcl     # 综合脚本
│   ├── run_cosim_system.tcl      # RTL协同仿真
│   └ run_package_system.tcl      # IP导出脚本
│   └ vitis-comp.json             # Vitis编译配置
│
├── host/                         # 主机程序
│   ├── litho_host.cpp            # ⭐ XRT C++主机 (推荐)
│   ├── litho_host_opencl.cpp     # OpenCL C++主机
│   ├── litho_host.py             # Python XRT主机
│   ├── Makefile                  # 构建脚本
│   └ README.md                   # 使用文档
│
├── doc/                          # 项目文档
│   ├── PROJECT_SUMMARY.md        # 项目总结
│   ├── WORKSPACE_STRUCTURE.md    # ⭐ 工作空间结构说明
│   ├── PHASE_SUMMARY_REPORT.md   # ⭐ 阶段性总结报告
│   └ calc_image_csynth_analysis.md
│   └ calc_image_integration_report.md
│
├── hls_litho_system_proj/        # ⭐ 最终HLS工程
│   └ solution1/impl/ip/          # Vivado IP输出
│
├── hls_calc_image_integrated_proj/
├── hls_socs_proj/                # 模块级HLS工程
├── hls_top_simple/               # FFT测试工程
│
├── data/                         # 测试数据
├── ip_export/                    # IP导出目录
└── archive/                      # 历史归档
```

## 技术架构

### 系统数据流

```
┌─────────────────────────────────────────────────────────────────┐
│                    FPGA-Litho 系统架构                           │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌─────────┐    ┌─────────┐    ┌─────────┐    ┌─────────┐      │
│  │ Source  │───►│  Mask   │───►│FFT R2C  │───►│  TCC    │      │
│  │  Gen    │    │  Gen    │    │         │    │ 计算    │      │
│  └─────────┘    └─────────┘    └─────────┘    └────┬────┘      │
│                                                    │           │
│                    ┌───────────────────────────────┘           │
│                    │                                           │
│                    ▼                                           │
│              ┌───────────┐    ┌───────────┐    ┌─────────┐     │
│              │calcImage  │───►│ FFT C2R   │───►│ Output  │     │
│              │ (频域计算) │    │  (IFFT)   │    │ (imgf)  │     │
│              └───────────┘    └───────────┘    └─────────┘     │
│                                                                 │
│                    【TCC模式: mode=1】                           │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌─────────┐    ┌─────────┐                                     │
│  │ Kernels │───►│  SOCS   │───► img_out                         │
│  │ + Scales│    │  计算   │                                     │
│  └─────────┘    └─────────┘                                     │
│                                                                 │
│                    【SOCS模式: mode=2】                           │
└─────────────────────────────────────────────────────────────────┘

接口: 7x AXI-Master (m0-m6) + 1x AXI-Lite (s_axi_control)
时钟: ap_clk (200MHz目标, 实际274MHz)
```

### 系统资源利用率

| 资源 | 使用量 | 总量    | 利用率  | 状态   |
| ---- | ------ | ------- | ------- | ------ |
| BRAM | 615    | 720     | **85%** | ⚠️ 高   |
| DSP  | 87     | 1368    | 6%      | ✅ 低   |
| FF   | 33,368 | 325,440 | 10%     | ✅ 低   |
| LUT  | 37,315 | 162,720 | 22%     | ✅ 中等 |

> **注意**: BRAM利用率85%，接近饱和。如需扩展功能，建议优化存储策略或选用更大容量器件。

### 模块级性能指标

| 模块                      | 时钟频率 | 循环II | BRAM | DSP |
| ------------------------- | -------- | ------ | ---- | --- |
| hls_tcc                   | 342 MHz  | 1      | 16   | 16  |
| hls_calc_image_integrated | 274 MHz  | 4      | 292  | 34  |
| hls_socs                  | 290 MHz  | 1      | 60   | 43  |

## 开发环境

### 工具要求

- **Vitis HLS**: 2025.2 或更高版本
- **Vivado**: 2025.2 或更高版本
- **XRT**: 2025.2 (运行主机程序)
- **目标平台**: xcku3p-ffvb676-2-e (Kintex UltraScale+)

### 环境配置

运行以下脚本配置Vitis环境：

```bash
# Windows PowerShell
./start_vitis_env.ps1

# Windows CMD
./start_vitis_env.bat
```

---

## 使用方法

### 1. HLS综合与验证

```bash
# C仿真测试
vitis-run --mode hls --csim --config script/hls_config_system.cfg --work_dir hls_litho_system_proj

# HLS综合
vitis-run --mode hls --csynth --config script/hls_config_system.cfg --work_dir hls_litho_system_proj

# RTL协同仿真
vitis-run --mode hls --cosim --config script/hls_config_system.cfg --work_dir hls_litho_system_proj

# Vivado IP导出
vitis-run --mode hls --export_design --format ip_catalog --config script/hls_config_system.cfg
```

### 2. Vivado集成

```tcl
# 在Vivado中添加IP
set_property ip_repo_paths {hls_litho_system_proj/solution1/impl/ip} [current_project]
update_ip_catalog

# 添加IP到Block Design
create_ip -name hls_litho_system -vendor fpga-litho.org -library hls -version 1.0
```

### 3. 主机程序运行

```bash
# XRT C++主机 (推荐)
cd host
make
./litho_host -x ./hls_litho_system.xo -k hls_litho_system -m 1

# Python主机 (快速原型)
python litho_host.py --xclbin hls_litho_system.xclbin --mode 1
```

---

## 开发进度

详细开发计划请参考 `doc/PROJECT_SUMMARY.md`

### 阶段完成情况

| Phase   | 内容               | 状态     | 完成率 |
| ------- | ------------------ | -------- | ------ |
| Phase 0 | 环境准备与架构设计 | ✅ 完成   | 100%   |
| Phase 1 | FFT模块重构        | ✅ 完成   | 100%   |
| Phase 2 | 辅助模块重构       | ✅ 完成   | 100%   |
| Phase 3 | 核心计算模块重构   | ✅ 完成   | 100%   |
| Phase 4 | 顶层集成与系统优化 | ✅ 完成   | 100%   |
| Phase 5 | RTL验证与IP导出    | ✅ 完成   | 100%   |
| Phase 6 | 板级验证           | 🔄 待硬件 | 10%    |

### 验证成果

#### C仿真 ✅
- TCC模式: 70非零TCC元素，最大值19.4088
- SOCS模式: 49像素累加验证
- 所有测试用例通过

#### RTL协同仿真 ✅
- **执行时间**: 18分13秒
- **测试结果**: 3/3 PASS
- **仿真工具**: XSIM (UVM框架)
- **精度验证**: RTL与C模型一致

#### Vivado IP导出 ✅
- **导出格式**: Vivado IP Catalog
- **IP名称**: hls_litho_system_1_0
- **生成时间**: 26秒

### 下一步工作

- 🔄 板级验证 (需FPGA硬件)
- ⏳ 实际性能测量与CPU对比
- ⏳ PCIe数据传输优化

## 参考实现

本项目FFT实现参考了Xilinx官方 `interface_stream` 方案：

- 使用 `hls::fft<>` IP核封装
- 定点数类型配置: `ap_fixed<16, 1>`
- 流式接口设计 (`hls::stream`)
- natural_order输出模式

### 光刻模拟算法

基于FPGA-Litho模型实现：

- **TCC (Transmission Cross Coefficients)**: 光学系统传输特性矩阵
- **SOCS (Sum of Coherent Systems)**: 快速光学成像方法
- **光源类型**: Annular/Dipole/CrossQuadrupole/Point
- **掩模类型**: LineSpace/Rectangle/Cross

---

## 文档索引

| 文档                                | 内容               |
| ----------------------------------- | ------------------ |
| `doc/WORKSPACE_STRUCTURE.md`        | 工作空间文件树说明 |
| `doc/PHASE_SUMMARY_REPORT.md`       | 阶段性开发总结报告 |
| `doc/PROJECT_SUMMARY.md`            | 项目总体概述       |
| `doc/calc_image_csynth_analysis.md` | calcImage综合分析  |
| `host/README.md`                    | 主机程序使用说明   |

---

## 许可证

MIT License

## 贡献

欢迎提交Issue和Pull Request！

## 作者

Copyright 2026

---

**注意**: 本项目为学术研究用途，光刻模拟算法基于FPGA-Litho模型简化实现。当前核心开发已完成，待板级验证确认实际性能。
- 流式接口设计 (`hls::stream`)
- natural_order输出模式

## 许可证

MIT License

## 贡献

欢迎提交Issue和Pull Request！

## 作者

Copyright 2026

---

**注意**: 本项目为学术研究用途，光刻模拟算法基于FPGA-Litho模型简化实现。