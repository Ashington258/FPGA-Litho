# FPGA-Litho

**FPGA加速的光刻模拟系统 - High-Level Synthesis实现**

## 项目简介

FPGA-Litho是一个基于Xilinx Vitis HLS的光刻模拟系统，利用FFT算法实现光学图像处理。本项目旨在通过FPGA硬件加速，提高光刻仿真计算效率。

### 核心功能

- **FFT R2C变换**: 实数到复数的快速傅里叶变换
- **FFT C2R变换**: 复数到实数的逆傅里叶变换  
- **频域移位**: 中心化频谱数据的shift操作
- **Mask生成**: 光刻掩模图案生成模块
- **Source生成**: 光源模型生成模块
- **TCC/SOCS计算**: 传输交叉系数计算

## 项目结构

```
FPGA-Litho/
├── include/           # 头文件目录
│   ├── hls_types.h    # 数据类型定义
│   ├── hls_top.h      # 顶层模块接口
│   ├── hls_fft_r2c.h  # FFT R2C接口
│   ├── hls_fft_c2c.h  # FFT C2R接口
│   ├── hls_shift.h    # 数据移位接口
│   └── hls_fft_simple.h # 简化版FFT接口
│
├── src/               # 源文件目录
│   ├── hls_top.cpp    # 顶层集成模块
│   ├── hls_fft_r2c.cpp # FFT R2C实现
│   ├── hls_fft_c2c.cpp # FFT C2R实现
│   ├── hls_shift.cpp  # 移位操作实现
│   ├── hls_mask.cpp   # Mask生成实现
│   ├── hls_source.cpp # Source生成实现
│   └── hls_fft_simple.cpp # 简化版FFT实现
│
├── testbench/         # 测试平台
│   ├── fft_tb.cpp     # 完整FFT测试
│   └ fft_tb_simple.cpp # 简化版FFT测试
│
├── script/            # HLS配置脚本
│   └ hls_config.cfg   # HLS综合配置
│
└── TODO.md            # 开发计划与重构任务
```

## 技术架构

### 数据流架构

```
Source Gen → Mask Gen → FFT R2C → calcTCC/calcSOCS → FFT C2R → Output
```

### FFT实现方案

- 采用 `interface_stream` 官方推荐的FFT实现
- 使用定点数类型 (`ap_fixed`) 以获得更好的FPGA资源效率
- 支持scaled缩放模式，可配置每级FFT缩放参数
- FFT长度: 1024点 (可配置)

## 开发环境

### 工具要求

- **Vitis HLS**: 2025.2 或更高版本
- **Vivado**: 2025.2 或更高版本
- **目标平台**: Xilinx FPGA (需支持FFT IP核)

### 环境配置

运行以下脚本配置Vitis环境：

```bash
# Windows PowerShell
./start_vitis_env.ps1

# Windows CMD
./start_vitis_env.bat
```

## 使用方法

### HLS综合

```bash
# 进入HLS目录
cd hls_top_simple

# 运行C仿真
vitis_hls -f hls_config.cfg
```

### Testbench运行

```bash
# 编译并运行简化版FFT测试
cd hls_top_simple/hls/csim
./sim.bat
```

## 开发进度

详细开发计划请参考 [TODO.md](TODO.md)

### 当前状态

- ✅ 基础数据类型定义
- ✅ FFT IP核封装 (定点数版本)
- ✅ 简化版FFT测试通过
- 🔄 FFT R2C/C2R重构 (进行中)
- ⏳ TCC/SOCS计算模块
- ⏳ 完整系统集成

## 参考实现

本项目FFT实现参考了Xilinx官方 `interface_stream` 方案：

- 使用 `hls::fft<>` IP核封装
- 定点数类型配置: `ap_fixed<16, 1>`
- 流式接口设计 (`hls::stream`)
- natural_order输出模式

## 许可证

MIT License

## 贡献

欢迎提交Issue和Pull Request！

## 作者

Copyright 2026

---

**注意**: 本项目为学术研究用途，光刻模拟算法基于K-Litho模型简化实现。