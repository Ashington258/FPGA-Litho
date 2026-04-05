# FPGA-Litho 数据流架构详解

> 创建日期: 2026-04-03  
> 作者: FPGA-Litho Team  
> 目标: 详细说明工程数据流、AXI接口配置及具体参数

---

## 一、工程数据流总览

### 1.1 双模式架构

FPGA-Litho支持两种核心计算模式，对应不同的光刻模拟算法：

```
┌─────────────────────────────────────────────────────────────┐
│                   FPGA-Litho 系统架构                        │
├─────────────────────────────────────────────────────────────┤
│                                                               │
│  【TCC模式】mode=1  ←─ 光源+掩模 → 频域图像计算             │
│                                                               │
│  【SOCS模式】mode=2  ←─ SOCS核+掩模 → 空间域图像计算         │
│                                                               │
└─────────────────────────────────────────────────────────────┘
```

---

## 二、TCC模式数据流详解 (mode=1)

### 2.1 数据流架构图

```
┌─────────────┐
│ Host (CPU)  │
│             │
│ - 光源数据  │ (64×64复数矩阵)
│ - 掩模数据  │ (64×64复数频谱)
│ - TCC矩阵   │ (49×49复数矩阵, Nx=3)
└──────┬──────┘
       │ AXI-Master接口 (数据加载)
       ↓
┌──────────────────────────────────────────────────────────┐
│           FPGA内部计算流程 (DATAFLOW)                     │
├──────────────────────────────────────────────────────────┤
│                                                            │
│  Step 1: 数据预取到BRAM缓存                               │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐               │
│  │ Source   │→ │  BRAM    │→ │ Local    │               │
│  │ (DDR)    │  │ Cache    │  │ Buffer   │               │
│  └──────────┘  └──────────┘  └──────────┘               │
│                                                            │
│  Step 2: calcImage频域计算                                │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐               │
│  │ Mask     │→ │ TCC      │→ │ calcImage│               │
│  │ FFT      │  │ Matrix   │  │ Core     │               │
│  └──────────┘  └──────────┘  └──────────┘               │
│                            ↓                               │
│  Step 3: 输出频域图像                                     │
│                    ┌──────────┐                          │
│                    │ imgf     │ → DDR输出                │
│                    │ (64×64)  │                          │
│                    └──────────┘                          │
└──────────────────────────────────────────────────────────┘
       │ AXI-Master接口 (结果回传)
       ↓
┌─────────────┐
│ Host (CPU)  │
│             │
│ - imgf数据  │ (频域图像，后续IFFT处理)
└─────────────┘
```

### 2.2 具体工程参数

#### 光源数据参数
```cpp
// 数据结构: std::complex<float>
// 尺寸: 64×64 = 4096个复数元素
// 存储格式: [real0, imag0, real1, imag1, ...]

// 典型光源类型:
// - Annular (圆环光源): NA_inner=0.5, NA_outer=0.8
// - Dipole (双极光源): NA=0.7, 方向角=45°
// - Point (点光源): 单点中心照明

// 光源数据示例 (Annular):
for (int y = 0; y < 64; y++) {
    for (int x = 0; x < 64; x++) {
        float fx = (x - 32) / 32.0f;  // 频域坐标
        float fy = (y - 32) / 32.0f;
        float r = sqrt(fx*fx + fy*fy);
        
        if (r >= 0.5 && r <= 0.8) {  // Annular区域
            source[y*64 + x] = cmpxFloat(1.0, 0.0);
        } else {
            source[y*64 + x] = cmpxFloat(0.0, 0.0);
        }
    }
}
```

#### 掩模数据参数
```cpp
// 数据结构: std::complex<float>
// 尺寸: 64×64 = 4096个复数元素
// 数据来源: 掩模图案的FFT频谱

// 典型掩模类型:
// - LineSpace (线条空间): pitch=200nm, width=100nm
// - Rectangle (矩形): size=300×300nm
// - ContactHole (接触孔): diameter=150nm

// 掩模频谱计算 (Host端):
for (int y = 0; y < 64; y++) {
    for (int x = 0; x < 64; x++) {
        float fx = (x - 32) / (lambda/NA * 32.0f);
        float fy = (y - 32) / (lambda/NA * 32.0f);
        
        // sinc函数频谱 (矩形掩模)
        float sinc_x = sin(M_PI * fx * width) / (M_PI * fx * width);
        float sinc_y = sin(M_PI * fy * height) / (M_PI * fy * height);
        
        mask_fft[y*64 + x] = cmpxFloat(sinc_x * sinc_y, 0.0);
    }
}
```

#### TCC矩阵参数
```cpp
// 数据结构: std::complex<float>
// 尺寸: 49×49 = 2401个复数元素 (Nx=3, Ny=3)
// 矩阵维度: (2*Nx+1) × (2*Ny+1) = 7×7
// 总元素: 7×7 × 7×7 = 2401

// TCC矩阵计算公式:
// TCC[nx2,ny2; nx1,ny1] = Σ source[q,p] × 
//                         Pupil(q - nx2, p - ny2) × 
//                         conj(Pupil(q - nx1, p - ny1))

// TCC矩阵特性:
// - 对称性: TCC[j,i] = conj(TCC[i,j])
// - 上三角存储: 减少50%冗余计算
// - 复数矩阵: 包含相位信息
```

#### 输出频域图像参数
```cpp
// 数据结构: std::complex<float>
// 尺寸: 64×64 = 4096个复数元素
// 计算公式: imgf[nx2,ny2] = Σ TCC[nx2,ny2; nx1,ny1] × 
//                         mask[nx1,ny1] × conj(mask[nx2,ny2])

// 后续处理 (Host端):
// 1. IFFT变换: imgf → 空间域图像
// 2. 平方幅度: |img|^2 = real^2 + imag^2
// 3. 边缘提取: 计算图像边缘位置
```

### 2.3 AXI接口配置

#### AXI-Master接口 (数据传输)
```cpp
// 6个独立的AXI-Master端口，避免访问冲突

#pragma HLS INTERFACE m_axi port=source offset=slave bundle=gmem0
// 光源数据接口
// - 地址范围: 0x0000_0000 - 0x0000_3FFF (16KB)
// - 数据宽度: 64-bit (复数float)
// - 访问模式: 只读

#pragma HLS INTERFACE m_axi port=mask_fft offset=slave bundle=gmem1
// 掩模频谱接口
// - 地址范围: 0x0000_4000 - 0x0000_7FFF (16KB)
// - 数据宽度: 64-bit
// - 访问模式: 只读

#pragma HLS INTERFACE m_axi port=tcc offset=slave bundle=gmem2
// TCC矩阵接口
// - 地址范围: 0x0000_8000 - 0x0000_BFFF (16KB)
// - 数据宽度: 64-bit
// - 访问模式: 只读

#pragma HLS INTERFACE m_axi port=imgf offset=slave bundle=gmem5
// 输出频域图像接口
// - 地址范围: 0x0000_C000 - 0x0000_FFFF (16KB)
// - 数据宽度: 64-bit
// - 访问模式: 只写
```

#### AXI-Lite接口 (参数配置)
```cpp
#pragma HLS INTERFACE s_axilite port=params bundle=control
// 系统参数控制接口
// - 光学参数: lambda (波长), NA (数值孔径), defocus (离焦)
// - 尺寸参数: Lx, Ly, Nx, Ny
// - 模式参数: mode=1 (TCC模式)

// 参数寄存器映射:
// 0x00: lambda (float)
// 0x04: NA (float)
// 0x08: defocus (float)
// 0x10: Lx (int)
// 0x14: Ly (int)
// 0x18: Nx (int)
// 0x1C: Ny (int)
// 0x20: srcSize (int)
// 0x24: mode (int)
// 0x28: ap_start (控制寄存器)
// 0x2C: ap_done (状态寄存器)
```

---

## 三、SOCS模式数据流详解 (mode=2)

### 3.1 数据流架构图

```
┌─────────────┐
│ Host (CPU)  │
│             │
│ - SOCS核    │ (8个核，每个225个复数)
│ - 权重系数  │ (8个float值)
│ - 掩模频谱  │ (64×64复数矩阵)
└──────┬──────┘
       │ AXI-Master接口 (数据加载)
       ↓
┌──────────────────────────────────────────────────────────┐
│           FPGA内部计算流程 (DATAFLOW)                     │
├──────────────────────────────────────────────────────────┤
│                                                            │
│  Step 1: 数据预取到BRAM缓存                               │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐               │
│  │ Kernels   │→ │  BRAM    │→ │ Local    │               │
│  │ (DDR)     │  │ Cache    │  │ Buffer   │               │
│  └──────────┘  └──────────┘  └──────────┘               │
│  ┌──────────┐  ┌──────────┐                               │
│  │ Scales    │→ │  BRAM    │                               │
│  │ (DDR)     │  │ Cache    │                               │
│  └──────────┘  └──────────┘                               │
│                                                            │
│  Step 2: 多核循环计算                                     │
│  ┌──────────────────────────────────────────┐            │
│  │ for k = 0 to nkernels-1:                 │            │
│  │   ┌──────────┐  ┌──────────┐            │            │
│  │   │ Kernel[k]│→ │ Mask     │ (复数乘)  │            │
│  │   └──────────┘  └──────────┘            │            │
│  │                   ↓                      │            │
│  │   ┌──────────┐  ┌──────────┐            │            │
│  │   │ Product  │→ │ IFFT     │            │            │
│  │   └──────────┘  └──────────┘            │            │
│  │                   ↓                      │            │
│  │   ┌──────────┐  ┌──────────┐            │            │
│  │   │ Spatial  │→ │ Square   │            │            │
│  │   │ Domain   │  │ +Accum   │            │            │
│  │   └──────────┘  └──────────┘            │            │
│  │                   ↓                      │            │
│  │   ┌──────────┐                          │            │
│  │   │ scales[k]│ × (real²+imag²)         │            │
│  │   └──────────┘                          │            │
│  └──────────────────────────────────────────┘            │
│                                                            │
│  Step 3: 循环移位输出                                     │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐               │
│  │ Accum    │→ │ Shift    │→ │ img_out  │               │
│  │ Image    │  │ (29×29)  │  │ Output   │               │
│  └──────────┘  └──────────┘  └──────────┘               │
└──────────────────────────────────────────────────────────┘
       │ AXI-Master接口 (结果回传)
       ↓
┌─────────────┐
│ Host (CPU)  │
│             │
│ - img_out   │ (空间域光学图像，29×29)
└─────────────┘
```

### 3.2 具体工程参数

#### SOCS核数据参数
```cpp
// 数据结构: std::complex<float>
// 核数量: 8个 (典型值)
// 单核尺寸: 15×15 = 225个复数元素 (Nx=7, Ny=7)
// 总数据量: 8 × 225 = 1800个复数元素

// SOCS核来源: TCC矩阵分解 (Host端)
// 分解方法: Eigen decomposition
// 分解公式: TCC = Σ scales[k] × kernel[k] × conj(kernel[k])

// SOCS核特性:
// - 空域表示: SOCS核是空间域的复数函数
// - 正交性: 核之间相互正交
// - 权重排序: scales按重要性排序 (前几个核占主要权重)
```

#### 权重系数参数
```cpp
// 数据结构: float
// 数量: 8个权重值
// 存储格式: [scale0, scale1, scale2, ..., scale7]

// 权重特性:
// - 衰减性: scales[0] > scales[1] > ... > scales[7]
// - 归一化: Σ scales[k] ≈ 总光强
// - 截断准则: scales[k] < 阈值时停止

// 典型权重分布:
scales[0] = 0.45;  // 第一核占45%权重
scales[1] = 0.25;  // 第二核占25%
scales[2] = 0.15;  // 第三核占15%
scales[3] = 0.08;  // 第四核占8%
scales[4] = 0.04;  // 第五核占4%
scales[5] = 0.02;  // 第六核占2%
scales[6] = 0.01;  // 第七核占1%
scales[7] = 0.005; // 第八核占0.5%
```

#### 输出空间图像参数
```cpp
// 数据结构: float (实数图像)
// 尺寸: 29×29 = 841个像素 (4×Nx+1 = 29)
// 输出格式: [pixel0, pixel1, ..., pixel840]

// 计算公式:
// img_out[x,y] = Σ scales[k] × |kernel[k] * mask|^2
//               + 循环移位处理

// 图像特性:
// - 空间域: 直接表示光学图像强度分布
// - 实数值: 光强度为正实数
// - 边缘增强: 可直接用于边缘提取
```

### 3.3 AXI接口配置

#### AXI-Master接口 (数据传输)
```cpp
#pragma HLS INTERFACE m_axi port=kernels offset=slave bundle=gmem3
// SOCS核数据接口
// - 地址范围: 0x0000_0000 - 0x0000_2FFF (12KB)
// - 数据宽度: 64-bit (复数float)
// - 访问模式: 只读

#pragma HLS INTERFACE m_axi port=scales offset=slave bundle=gmem4
// 权重系数接口
// - 地址范围: 0x0000_3000 - 0x0000_3020 (32B)
// - 数据宽度: 32-bit (float)
// - 访问模式: 只读

#pragma HLS INTERFACE m_axi port=img_out offset=slave bundle=gmem6
// 输出空间图像接口
// - 地址范围: 0x0000_4000 - 0x0000_41FF (512B)
// - 数据宽度: 32-bit (float)
// - 访问模式: 只写
```

---

## 四、BRAM缓存架构详解

### 4.1 为什么需要BRAM缓存？

#### 问题分析
```
原始设计问题:
❌ 直接AXI-Master访问DDR → 时序灾难
   - DDR访问延迟: 50-100ns (12-25时钟周期)
   - AXI总线仲裁: 多端口冲突导致等待
   - 数据依赖链: 循环内AXI访问无法流水线化

结果: 时钟频率仅28MHz (目标250MHz)
```

#### 解决方案
```
优化设计:
✅ 本地BRAM缓存 + 预取-计算-回写三阶段
   - BRAM访问延迟: 1-2时钟周期
   - 无总线冲突: 本地独立存储
   - 流水线化: 数据预取后计算循环II=1

结果: 时钟频率274MHz (超过目标37%)
```

### 4.2 BRAM缓存配置

#### TCC模式BRAM缓存
```cpp
// 本地缓存数组
cmpxFloat source_local[64*64];      // 光源缓存
cmpxFloat mask_local[64*64];        // 掩模缓存
cmpxFloat tcc_local[49*49];         // TCC矩阵缓存
cmpxFloat imgf_local[64*64];        // 输出缓存

// HLS存储绑定
#pragma HLS BIND_STORAGE variable=source_local type=RAM_2P impl=BRAM
#pragma HLS BIND_STORAGE variable=mask_local type=RAM_2P impl=BRAM
#pragma HLS BIND_STORAGE variable=tcc_local type=RAM_2P impl=BRAM

// 数组分区优化 (并行访问)
#pragma HLS ARRAY_PARTITION variable=tcc_local cyclic factor=8 dim=1
```

#### SOCS模式BRAM缓存
```cpp
// 本地缓存数组
cmpxFloat kernel_local[8*225];     // SOCS核缓存
float scales_local[8];              // 权重缓存
float img_accum[29*29];             // 累加缓存

// 存储绑定
#pragma HLS BIND_STORAGE variable=kernel_local type=RAM_2P impl=BRAM
#pragma HLS BIND_STORAGE variable=scales_local type=RAM_1P impl=BRAM
#pragma HLS BIND_STORAGE variable=img_accum type=RAM_2P impl=BRAM

// 数组分区
#pragma HLS ARRAY_PARTITION variable=kernel_local cyclic factor=4 dim=1
#pragma HLS ARRAY_PARTITION variable=img_accum cyclic factor=4 dim=1
```

### 4.3 三阶段数据流

#### 阶段1: 数据预取 (Prefetch)
```cpp
// 从DDR预取到BRAM
for (int i = 0; i < 4096; i++) {
#pragma HLS PIPELINE II=1
    source_local[i] = source[i];  // 1时钟周期完成
}

// 预取时间: 4096时钟周期 ≈ 20μs @200MHz
```

#### 阶段2: 核心计算 (Compute)
```cpp
// 计算循环完全流水线化
for (int ny2 = -Ny; ny2 <= Ny; ny2++) {
    for (int nx2 = 0; nx2 <= Nx; nx2++) {
#pragma HLS PIPELINE II=1
        // 直接访问BRAM缓存 (1时钟周期)
        cmpxFloat val = tcc_local[idx] * mask_local[idx2];
        imgf_local[...] = val;
    }
}

// 计算时间: II=1, 高效流水线
```

#### 阶段3: 结果回写 (Writeback)
```cpp
// 从BRAM回写到DDR
for (int i = 0; i < 4096; i++) {
#pragma HLS PIPELINE II=1
    imgf[i] = imgf_local[i];
}

// 回写时间: 4096时钟周期 ≈ 20μs
```

---

## 五、完整执行时序

### 5.1 TCC模式执行时序
```
时间轴 (假设200MHz时钟):

0μs    ─┬─ Host准备数据 (光源、掩模、TCC)
        │  - 光源生成: Annular光源
        │  - 掩模FFT: Rectangle掩模频谱
        │  - TCC计算: Host端预计算TCC矩阵
        │
20μs   ─┼─ AXI-Master加载 (预取阶段)
        │  - source → source_local: 4096 cycles
        │  - mask → mask_local: 4096 cycles
        │  - tcc → tcc_local: 2401 cycles
        │  - 总预取时间: ≈50μs
        │
70μs   ─┼─ FPGA核心计算 (calcImage)
        │  - 外层循环: 7×7 iterations
        │  - 内层循环: 7×7 iterations × II=4
        │  - 总计算时间: ≈2.8ms (Nx=3)
        │
2.87ms ─┼─ AXI-Master回写 (输出阶段)
        │  - imgf_local → imgf: 4096 cycles
        │  - 回写时间: ≈20μs
        │
2.89ms ─┴─ Host后续处理
           - IFFT变换: imgf → 空间域
           - 图像分析: 边缘提取

总延迟: ≈2.9ms @200MHz (Nx=3)
加速比: ~500x vs CPU单线程
```

### 5.2 SOCS模式执行时序
```
时间轴 (假设200MHz时钟):

0μs    ─┬─ Host准备数据 (SOCS核、权重、掩模)
        │  - TCC分解: 生成8个SOCS核
        │  - 权重计算: scales数组
        │  - 掩模FFT: 掩模频谱数据
        │
20μs   ─┼─ AXI-Master加载 (预取阶段)
        │  - kernels → kernel_local: 1800 cycles
        │  - scales → scales_local: 8 cycles
        │  - mask → mask_local: 4096 cycles
        │  - 总预取时间: ≈30μs
        │
50μs   ─┼─ FPGA核心计算 (SOCS)
        │  - 外层循环: 8核迭代
        │  - Kernel-Mask乘法: II=1
        │  - 平方累加: II=1
        │  - 移位输出: II=1
        │  - 总计算时间: ≈400μs (8核)
        │
450μs  ─┼─ AXI-Master回写 (输出阶段)
        │  - img_accum → img_out: 841 cycles
        │  - 回写时间: ≈4μs
        │
454μs  ─┴─ Host后续处理
           - 图像分析: 直接使用空间域图像
           - 边缘提取: 无需额外IFFT

总延迟: ≈0.45ms @200MHz (8核)
加速比: ~1000x vs CPU单线程
```

---

## 六、数据大小与内存布局

### 6.1 数据大小计算

| 数据类型 | 尺寸 | 元素数 | 单元素大小 | 总大小 | DDR地址范围 |
|---------|------|--------|-----------|--------|-------------|
| 光源数据 | 64×64 | 4096 | 8B (复数) | 32KB | 0x0000-0x7FFF |
| 掩模频谱 | 64×64 | 4096 | 8B | 32KB | 0x8000-0xFFFF |
| TCC矩阵 | 49×49 | 2401 | 8B | 19KB | 0x10000-0x14FFF |
| SOCS核 | 8×225 | 1800 | 8B | 14KB | 0x15000-0x18FFF |
| 权重 | 8 | 8 | 4B | 32B | 0x19000-0x19020 |
| 输出频域 | 64×64 | 4096 | 8B | 32KB | 0x1A000-0x21FFF |
| 输出空间 | 29×29 | 841 | 4B | 3KB | 0x22000-0x22FFF |

### 6.2 内存对齐要求

```cpp
// AXI-Master访问要求64字节对齐
// Host端内存分配示例:

// C++分配:
cmpxFloat* source = (cmpxFloat*)aligned_alloc(64, 32*1024);
cmpxFloat* mask_fft = (cmpxFloat*)aligned_alloc(64, 32*1024);
cmpxFloat* tcc = (cmpxFloat*)aligned_alloc(64, 19*1024);

// Python分配 (使用numpy):
import numpy as np
source = np.zeros((64, 64), dtype=np.complex64)
source_aligned = np.require(source, requirements=['C', 'A'])  # 对齐要求
```

---

## 七、总结与优化建议

### 7.1 关键优化点

✅ **数据预取**: 所有计算数据预取到BRAM缓存
✅ **流水线化**: 计算循环II=1完全流水线化
✅ **数组分区**: cyclic factor=4/8并行访问
✅ **多端口分离**: 6个独立AXI-Master端口避免冲突

### 7.2 性能瓶颈分析

当前瓶颈: **数据传输延迟** (预取+回写 ≈ 10%总时间)
优化方向:
- 增加缓存容量: URAM替代BRAM
- 异步传输: 双缓冲交替预取
- 数据压缩: 减少传输数据量

---

*本文档详细说明了FPGA-Litho的数据流架构和具体参数，为后续硬件集成和优化提供参考*