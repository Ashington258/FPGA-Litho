# FPGA-Litho HLS优化工作总结

## 项目概述

**目标**: 开发FPGA加速的光刻仿真系统，包括TCC计算和图像计算模块

**器件**: Xilinx Kintex UltraScale+ (xcku3p-ffvb676-2-e)

**工具链**: Vitis HLS 2025.2

---

## 模块开发状态

### 1. TCC模块 (Phase 3.A) - ✅ 完成

| 版本     | 时钟   | II  | Fmax   | 状态 |
| -------- | ------ | --- | ------ | ---- |
| 基础版本 | 250MHz | 1   | 266MHz | ✅    |
| 优化版本 | 250MHz | 1   | 286MHz | ✅    |

**核心文件**:
- `src/hls_tcc.cpp` - TCC计算核心
- `include/hls_tcc.h` - TCC头文件
- `testbench/tcc_tb_hls.cpp` - 测试台

**资源使用**: BRAM=64, DSP=18, FF=2179, LUT=3541

---

### 2. calcImage模块 (Phase 3.B) - ✅ 完成

#### 优化历程

| 尝试           | 目标时钟   | 实际II | 时序状态                 | 结论       |
| -------------- | ---------- | ------ | ------------------------ | ---------- |
| 原始设计       | 250MHz     | 1      | ❌ 失败 (fadd 12ns > 4ns) | 时序违规   |
| II=4设计       | 250MHz     | 4      | ❌ 失败                   | 仍无法满足 |
| **200MHz设计** | **200MHz** | **4**  | **✅ Slack=0.19**         | **成功**   |
| 定点设计       | 250MHz     | 1      | ⚠️ 精度损失               | 需验证精度 |

#### 最终采用方案

**200MHz版本** - 完成集成

```
Target Clock: 5ns (200MHz)
Core Loop II: 4 ✓
Timing Slack: +0.19 ✓

Resources:
  BRAM: 292 (40%)
  DSP: 34 (2%)  
  FF: 15,738 (4%)
  LUT: 15,063 (9%)
```

**核心文件**:
- `src/hls_calc_image_integrated.cpp` - 集成版本核心
- `include/hls_calc_image_integrated.h` - 头文件
- `testbench/calc_image_integrated_tb.cpp` - 测试台

---

### 3. calcSOCS模块 (Phase 3.C) - ⏳ 待开发

---

### 4. BRAM版本 (Phase 6) - ✅ 完成

**目标**: 为xcku3p FPGA板卡开发无DDR依赖的纯BRAM版本

**约束**: 
- TCC模式: Nx ≤ 3
- SOCS模式: nkernels ≤ 8
- 数据大小: Lx, Ly ≤ 64

#### 设计架构

**单函数架构 (Phase 6C)**:
- 操作切换模式 (operation switch): 10种操作
- 静态本地BRAM数组 (static local arrays)
- BIND_STORAGE pragma在函数作用域

#### 操作码定义

| 操作码 | 操作名 | 功能 |
|--------|--------|------|
| 0 | OP_LOAD_SOURCE | 加载源数据 |
| 1 | OP_LOAD_MASK | 加载mask数据 |
| 2 | OP_LOAD_TCC | 加载TCC矩阵 |
| 3 | OP_LOAD_KERNELS | 加载核函数 |
| 4 | OP_LOAD_SCALES | 加载缩放因子 |
| 5 | OP_COMPUTE_TCC | TCC模式计算 |
| 6 | OP_COMPUTE_SOCS | SOCS模式计算 |
| 7 | OP_READ_IMGF | 读取频域图像 |
| 8 | OP_READ_IMG_OUT | 读取输出图像 |
| 9 | OP_RESET | 重置所有BRAM |

#### HLS综合结果

```
Target Clock: 5ns (200MHz)
Estimated Fmax: 287.11 MHz
Timing Slack: +1.52ns ✓

Resources:
  BRAM_18K: 131 (18%)
  DSP: 27 (1%)
  FF: 5,083 (2%)
  LUT: 8,346 (5%)
```

**核心文件**:
- `src/hls_litho_system_bram.cpp` - BRAM版本核心
- `include/hls_litho_system_bram.h` - 头文件
- `testbench/litho_system_bram_tb.cpp` - 测试台 (10测试全过)
- `host/litho_host_bram.py` - Python驱动
- `host/test_bram_interface.py` - Python接口测试 (18/18通过)

#### 关键技术点

1. **BIND_STORAGE作用域**: 必须在函数作用域内，全局数组不生效
2. **静态本地数组**: 保证BRAM数据在函数调用间持久化
3. **ARRAY_PARTITION**: 循环分区(cyclic factor=4/8)实现并行访问
4. **参数验证**: 运行时检查Nx≤3, nkernels≤8约束

#### Python驱动接口

```python
from litho_host_bram import BRAMKernel, LithoBRAMApp

# 初始化
kernel = BRAMKernel('litho_bram.xclbin')

# TCC模式
app = LithoBRAMApp(mode='tcc')
result = app.run_tcc_mode(source, mask, tcc_data)

# SOCS模式
app = LithoBRAMApp(mode='socs')
result = app.run_socs_mode(mask, kernels, scales)
```

---

## 目录结构 (整理后)

```
FPGA-Litho/
├── src/                        # 源代码
│   ├── hls_top.cpp             # 顶层模块
│   ├── hls_tcc.cpp             # TCC计算
│   ├── hls_calc_image_integrated.cpp  # calcImage (最终版本)
│   ├── hls_fft_*.cpp           # FFT模块
│   └── ...
│
├── include/                    # 头文件
│   ├── hls_top.h
│   ├── hls_tcc.h
│   ├── hls_calc_image_integrated.h
│   └── ...
│
├── testbench/                  # 测试台
│   ├── tcc_tb_hls.cpp
│   ├── calc_image_integrated_tb.cpp
│   └── ...
│
├── script/                     # HLS配置
│   ├── hls_config.cfg          # 主配置 (5ns/200MHz)
│   ├── hls_integrated.cfg      # calcImage配置
│   └── run_csynth*.tcl         # 综合脚本
│
├── doc/                        # 文档
│   ├── PROJECT_SUMMARY.md      # 本文档
│   └── calc_image_csynth_analysis.md
│
├── data/                       # 测试数据
│
├── archive/                    # 归档旧版本
│   ├── hls_projects_old/       # 旧HLS项目目录
│   ├── docs_old/               # 旧分析报告
│   └── ...
│
└── hls_calc_image_integrated_proj/  # 最终综合项目
```

---

## 关键技术决策

### 1. 时钟频率调整

从250MHz(4ns)调整为200MHz(5ns)，以容纳浮点加法器(fadd ~12ns)的多周期延迟。

**理由**:
- 浮点运算延迟是硬件特性，无法通过优化缩短
- II=4在200MHz下，每次迭代20ns，足够容纳fadd
- 保持全精度计算，避免定点化的精度风险

### 2. 流水线设计

**calcImage核心循环优化**:
- 8通道并行累积器
- BRAM分区 (cyclic factor=4)
- 树状归约减少延迟

---

## 使用指南

### 快速运行

```bash
# 启动Vitis环境
.\start_vitis_env.ps1

# C-Simulation (calcImage)
vitis-run --mode hls --csim --config script/hls_integrated.cfg --work_dir hls_calc_image_test

# C-Synthesis
vitis-run --mode hls --tcl script/run_csynth_calc_image_integrated.tcl --work_dir hls_calc_image_proj
```

### 时钟约束

当前主配置文件 `script/hls_config.cfg`:
```
clock = 5ns    # 200MHz
part  = xcku3p-ffvb676-2-e
```

---

## 归档文件说明

`archive/` 目录包含所有优化过程中的中间版本：

- **calc_image相关**: 19个不同优化尝试的HLS项目目录
- **tcc相关**: TCC模块的多个优化版本
- **文档**: 各阶段的分析报告和对比文档

保留这些文件作为优化历史记录，供后续参考。

---

## 下一步计划

1. **Phase 6E**: 创建xclbin并测试硬件
2. **Vivado集成**: HLS IP导出与硬件集成
3. **性能验证**: 端到端功能与性能测试
4. **文档完善**: 用户手册和API文档

---

**报告日期**: 2026年4月4日
**版本**: Vitis HLS 2025.2
**状态**: Phase 3.A/3.B/6完成