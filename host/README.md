# FPGA-Litho Host Applications

本目录包含FPGA-Litho光刻模拟FPGA内核的主机应用程序。

## 目录结构

```
host/
├── litho_host.cpp         # XRT C++主机程序 (推荐用于Xilinx平台)
├── litho_host_opencl.cpp  # OpenCL C++主机程序 (通用平台兼容)
├── litho_host.py          # XRT Python主机程序 (快速原型验证)
├── Makefile               # 构建脚本
└── README.md              # 本文档
```

## 系统要求

### XRT版本 (推荐)
- Xilinx Runtime (XRT) 2022.1+
- Xilinx FPGA平台 (Alveo/UltraScale+)
- C++17编译器

### OpenCL版本
- OpenCL 2.0+ 支持
- 任何支持OpenCL的平台 (AMD/Intel/NVIDIA/Xilinx)

### Python版本
- Python 3.8+
- pyxrt包 (`pip install pyxrt`)
- numpy包 (`pip install numpy`)

## 构建说明

### Linux (XRT)

```bash
# 设置XRT环境
source /opt/xilinx/xrt/setup.sh

# 构建主机程序
make host

# 或指定XRT路径
make host XRT_PATH=/opt/xilinx/xrt
```

### Windows (Vitis)

```cmd
# 在Vitis环境中编译
# 需要先设置Vitis环境变量
cl.exe /std:c++17 /O2 litho_host.cpp /Fe:litho_host.exe
```

## 使用方法

### 基本用法

```bash
# TCC模式运行
./litho_host --xclbin ../hls_litho_system.xclbin --mode 1 --verbose

# SOCS模式运行
./litho_host --xclbin ../hls_litho_system.xclbin --mode 2 --verbose

# 性能测试 (10次运行)
./litho_host --xclbin ../hls_litho_system.xclbin --mode 1 --runs 10 --verbose
```

### 完整参数

| 参数               | 说明                  | 默认值                  |
| ------------------ | --------------------- | ----------------------- |
| `--xclbin <file>`  | XCLBIN文件路径 (必需) | -                       |
| `--device <index>` | 设备索引              | 0                       |
| `--mode <1         | 2>`                   | 工作模式: 1=TCC, 2=SOCS | 1 |
| `--runs <n>`       | 运行次数              | 1                       |
| `--verbose`        | 详细输出              | false                   |

**光学参数:**

| 参数             | 说明     | 默认值 |
| ---------------- | -------- | ------ |
| `--lambda <nm>`  | 波长     | 193.0  |
| `--NA <value>`   | 数值孔径 | 1.35   |
| `--defocus <nm>` | 离焦量   | 0.0    |

**尺寸参数:**

| 参数               | 说明           | 默认值 |
| ------------------ | -------------- | ------ |
| `--Lx <size>`      | 频域X尺寸      | 64     |
| `--Ly <size>`      | 频域Y尺寸      | 64     |
| `--Nx <size>`      | TCC/SOCS半宽   | 3      |
| `--Ny <size>`      | TCC/SOCS半高   | 3      |
| `--srcSize <size>` | 光源尺寸 (TCC) | 32     |
| `--nkernels <n>`   | SOCS核数量     | 4      |

**数据文件:**

| 参数               | 说明            |
| ------------------ | --------------- |
| `--source <file>`  | 光源数据文件    |
| `--mask <file>`    | 掩模FFT数据文件 |
| `--tcc <file>`     | TCC矩阵文件     |
| `--kernels <file>` | SOCS核数据文件  |
| `--scales <file>`  | SOCS权重文件    |
| `--output <file>`  | 输出结果文件    |

### Python版本

```bash
# 运行Python主机
python litho_host.py --xclbin ../hls_litho_system.xclbin --mode 1 --verbose
```

## 数据格式

### 复数数据格式
- 文件格式: 二进制
- 数据类型: float32
- 排列方式: `[real0, imag0, real1, imag1, ...]`

### 浮点数据格式
- 文件格式: 二进制
- 数据类型: float32

## 内核接口

### TCC模式 (mode=1)

输入:
- `source`: 光源数据 (srcSize × srcSize 复数)
- `mask_fft`: 掩模FFT (Lx × Ly 复数)
- `tcc`: TCC矩阵 (预计算)

输出:
- `imgf`: 频域图像 (Lx × Ly 复数)

### SOCS模式 (mode=2)

输入:
- `kernels`: SOCS核数据 (nkernels × TCC_DIM 复数)
- `scales`: SOCS权重 (nkernels 浮点)
- `mask_fft`: 掩模FFT (Lx × Ly 复数)

输出:
- `img_out`: 空间域图像 ((4Nx+1) × (4Ny+1) 浮点)

## 性能优化建议

1. **内存带宽优化**
   - 使用大页内存 (huge pages)
   - 启用PCIe Gen3/Gen4 x16
   - 批量数据传输

2. **内核执行优化**
   - 多次运行预热 (warm-up)
   - 异步执行与数据传输重叠
   - 使用内核流水线

3. **数据准备优化**
   - 预计算TCC矩阵存储
   - 使用固定尺寸避免动态分配
   - 数据对齐到4KB边界

## 示例运行流程

### 1. 准备FPGA内核

```bash
# 从HLS导出IP后生成xclbin
cd ../hls_litho_system_proj/solution1/impl/ip
# 使用Vivado生成bitstream和xclbin
```

### 2. 准备测试数据

```bash
# 生成光源数据
python generate_source.py --type annular --output source.bin

# 生成掩模数据
python generate_mask.py --type linespace --output mask.bin

# 计算TCC矩阵 (可选)
python calculate_tcc.py --source source.bin --output tcc.bin
```

### 3. 运行仿真

```bash
# TCC模式
./litho_host --xclbin hls_litho_system.xclbin \
    --mode 1 \
    --source source.bin \
    --mask mask.bin \
    --tcc tcc.bin \
    --output imgf_result.bin \
    --verbose

# SOCS模式
./litho_host --xclbin hls_litho_system.xclbin \
    --mode 2 \
    --kernels kernels.bin \
    --scales scales.bin \
    --mask mask.bin \
    --output img_result.bin \
    --verbose
```

## 调试指南

### 常见问题

1. **xclbin加载失败**
   - 检查文件路径是否正确
   - 验证设备是否支持该bitstream
   - 确认XRT版本兼容

2. **内存分配失败**
   - 检查设备内存容量
   - 减小数据尺寸参数
   - 启用内存碎片整理

3. **内核执行超时**
   - 检查时钟频率配置
   - 验证数据大小参数匹配
   - 查看设备日志 (dmesg)

### 日志输出

```bash
# 启用XRT详细日志
export XRT_VERBOSE=1

# 查看设备状态
xbutil examine

# 查看内核状态
xbutil validate
```

## 参考文档

- [XRT Documentation](https://xilinx.github.io/XRT/)
- [OpenCL Specification](https://www.khronos.org/opencl/)
- [Vitis HLS User Guide (UG1399)](https://docs.amd.com/)
- [FPGA-Litho Project Documentation](../doc/PROJECT_SUMMARY.md)