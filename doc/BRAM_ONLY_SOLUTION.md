# 无DDR板卡的片内存储解决方案

> 创建日期: 2026-04-03
> 目标: 在 xcku3p 无DDR板卡上运行 litho 系统

---

## 一、资源约束分析

### 1.1 xcku3p 可用资源

| 资源类型 | 总量 | 已用 (HLS核心) | 剩余可用 |
|----------|------|----------------|----------|
| BRAM_18K | 720 | 615 (85%) | **105 (15%)** |
| URAM | 0 | 0 | **0** |
| LUT | 162,720 | 15,063 (9%) | **~147K (91%)** |
| FF | 325,440 | 15,738 (4%) | **~310K (96%)** |
| DSP | 1,368 | 85 (6%) | **~1,283 (94%)** |

**关键约束**: 剩余 BRAM ≈ 105 × 18Kb = **~230 KB**

### 1.2 数据存储需求

#### TCC 模式数据流

```
source(32KB) ────┐
                 ├──> [TCC计算] ──> tcc(2KB) ──┐
mask_fft(32KB) ──┘                              ├──> [calcImage] ──> imgf(32KB)
                                                │
                         tcc(预计算)(2KB) ──────┘
```

**最大并发存储**: 32+32+2+32 = **98 KB** ✓ 可容纳

#### SOCS 模式数据流

```
kernels(14KB) ────┐
mask_fft(32KB) ──┼──> [kernel-mask] ──> [IFFT] ──> [square+acc] ──> img_out(3KB)
scales(32B) ─────┘
```

**最大并发存储**: 14+32+3 = **49 KB** ✓ 可容纳

---

## 二、方案对比

| 方案 | 开发工作量 | 性能 | 存储利用率 | 适用场景 |
|------|-----------|------|-----------|----------|
| **A: AXI-Lite+BRAM** | 中 | 中 | 高 | 单模式运行 |
| **B: 数据流架构** | 高 | 高 | 低 | 实时处理 |
| **C: 主机内存** | 低 | 低 | 最低 | 验证测试 |

---

## 三、方案A: AXI-Lite控制 + BRAM存储接口 (推荐)

### 3.1 架构设计

将 AXI-Master 接口改为 BRAM 接口，由主机通过 PCIe/以太网分批传输数据。

```
┌─────────────────────────────────────────────────────────────┐
│                     xcku3p FPGA                              │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  ┌─────────────┐    ┌────────────────────────────────────┐ │
│  │ AXI-Lite    │    │   BRAM Storage (230KB)             │ │
│  │ Control     │    │  ┌──────────┐  ┌──────────┐      │ │
│  │             │    │  │ source   │  │ mask_fft │      │ │
│  │ - 启动/停止  │    │  │ (32KB)   │  │ (32KB)   │      │ │
│  │ - 模式选择   │    │  ├──────────┤  ├──────────┤      │ │
│  │ - 状态查询   │    │  │ tcc/kern │  │ imgf/out │      │ │
│  │             │    │  │ (2-14KB) │  │ (32KB)   │      │ │
│  └──────┬──────┘    │  └──────────┘  └──────────┘      │ │
│         │           │                                    │ │
│         │           └───────────────┬────────────────────┘ │
│         │                           │                      │
│         │           ┌───────────────▼────────────────────┐ │
│         │           │   HLS Litho Core (已验证)           │ │
│         │           │   - TCC: 200MHz, II=4              │ │
│         │           │   - SOCS: 200MHz, II=1             │ │
│         │           └────────────────────────────────────┘ │
│         │                                                   │
│         │           ┌────────────────────────────────────┐ │
│         └───────────▶│ AXI-Lite Registers                  │ │
│                     │  - mode (TCC=1, SOCS=2)             │ │
│                     │  - size_params (Lx, Ly, Nx, Ny)     │ │
│                     │  - status (idle/running/done)        │ │
│                     │  - bram_addr (当前读写地址)          │ │
│                     │  - bram_data (数据端口)              │ │
│                     └────────────────────────────────────┘ │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 3.2 接口修改方案

#### 原始接口 (AXI-Master)

```cpp
void hls_litho_system(
    cmpxFloat *source,    // m_axi gmem0
    cmpxFloat *mask_fft,  // m_axi gmem1
    cmpxFloat *tcc,       // m_axi gmem2
    cmpxFloat *kernels,   // m_axi gmem3
    float *scales,        // m_axi gmem4
    cmpxFloat *imgf,      // m_axi gmem5
    float *img_out        // m_axi gmem6
);
```

#### 新接口 (BRAM存储)

```cpp
// 新的顶层接口
void hls_litho_system_bram(
    // AXI-Lite 控制接口
    int mode,              // 1=TCC, 2=SOCS
    int Lx, int Ly,        // 频域尺寸
    int Nx, int Ny,        // TCC/SOCS尺寸
    int srcSize,           // 光源尺寸
    int nkernels,          // SOCS核数量
    
    // BRAM 数据接口 (通过 AXI-Lite 读写)
    cmpxFloat source_bram[64*64],      // 本地BRAM
    cmpxFloat mask_bram[64*64],        // 本地BRAM
    cmpxFloat tcc_bram[15*15],         // 本地BRAM (TCC模式)
    cmpxFloat kernels_bram[8*15*15],   // 本地BRAM (SOCS模式)
    float scales_bram[8],              // 本地BRAM
    cmpxFloat imgf_bram[64*64],        // 本地BRAM输出
    float img_out_bram[29*29]          // 本地BRAM输出
);
```

### 3.3 实现步骤

#### Step 1: 创建 BRAM 接口版本 HLS 核心

```cpp
// src/hls_litho_system_bram.cpp

#include "../include/hls_litho_system.h"

// BRAM存储数组
static cmpxFloat source_bram[64*64];
static cmpxFloat mask_bram[64*64];
static cmpxFloat tcc_bram[15*15];
static cmpxFloat kernels_bram[8*15*15];
static float scales_bram[8];
static cmpxFloat imgf_bram[64*64];
static float img_out_bram[29*29];

// 状态寄存器
static volatile int status_reg = 0;  // 0=idle, 1=running, 2=done

void hls_litho_system_bram(
    // 控制参数 (AXI-Lite)
    int mode,
    int Lx, int Ly,
    int Nx, int Ny,
    int srcSize,
    int nkernels
) {
#pragma HLS INTERFACE s_axilite port=mode bundle=control
#pragma HLS INTERFACE s_axilite port=Lx bundle=control
#pragma HLS INTERFACE s_axilite port=Ly bundle=control
#pragma HLS INTERFACE s_axilite port=Nx bundle=control
#pragma HLS INTERFACE s_axilite port=Ny bundle=control
#pragma HLS INTERFACE s_axilite port=srcSize bundle=control
#pragma HLS INTERFACE s_axilite port=nkernels bundle=control
#pragma HLS INTERFACE s_axilite port=return bundle=control
    
    // BRAM接口绑定
#pragma HLS BIND_STORAGE variable=source_bram type=RAM_2P impl=BRAM
#pragma HLS BIND_STORAGE variable=mask_bram type=RAM_2P impl=BRAM
#pragma HLS BIND_STORAGE variable=tcc_bram type=RAM_2P impl=BRAM
#pragma HLS BIND_STORAGE variable=kernels_bram type=RAM_2P impl=BRAM
#pragma HLS BIND_STORAGE variable=scales_bram type=RAM_2P impl=BRAM
#pragma HLS BIND_STORAGE variable=imgf_bram type=RAM_2P impl=BRAM
#pragma HLS BIND_STORAGE variable=img_out_bram type=RAM_2P impl=BRAM
    
    status_reg = 1;  // running
    
    if (mode == 1) {
        // TCC模式
        hls_litho_tcc_mode(
            source_bram, mask_bram, imgf_bram,
            mode, Lx, Ly, Nx, Ny, srcSize
        );
    } else if (mode == 2) {
        // SOCS模式
        hls_litho_socs_mode(
            kernels_bram, scales_bram, mask_bram, img_out_bram,
            mode, Lx, Ly, Nx, Ny, nkernels
        );
    }
    
    status_reg = 2;  // done
}
```

#### Step 2: 添加数据加载接口

```cpp
// 数据加载函数 (通过AXI-Lite调用)
void load_source_data(int addr, cmpxFloat data) {
#pragma HLS INTERFACE s_axilite port=addr bundle=control
#pragma HLS INTERFACE s_axilite port=data bundle=control
    source_bram[addr] = data;
}

void load_mask_data(int addr, cmpxFloat data) {
#pragma HLS INTERFACE s_axilite port=addr bundle=control
#pragma HLS INTERFACE s_axilite port=data bundle=control
    mask_bram[addr] = data;
}

// 批量加载接口 (更高效)
void batch_load_source(cmpxFloat data[64*64]) {
#pragma HLS INTERFACE s_axilite port=data bundle=control
    for (int i = 0; i < 64*64; i++) {
        source_bram[i] = data[i];
    }
}
```

#### Step 3: HLS 综合配置

```tcl
# script/hls_config_bram.tcl

# 时钟约束
set_clock_period 5  ;# 200MHz

# 绑定存储到 BRAM
set_directive_bind_storage -type RAM_2P -impl BRAM "hls_litho_system_bram/source_bram"
set_directive_bind_storage -type RAM_2P -impl BRAM "hls_litho_system_bram/mask_bram"
set_directive_bind_storage -type RAM_2P -impl BRAM "hls_litho_system_bram/imgf_bram"

# 资源约束
set_part xcku3p-ffvb676-2-e
```

### 3.4 资源估算

| 存储项 | BRAM数量 (18Kb) |
|--------|----------------|
| source_bram | 32KB / 18Kb = **18** |
| mask_bram | 32KB / 18Kb = **18** |
| tcc_bram | 2KB / 18Kb = **1** |
| kernels_bram | 14KB / 18Kb = **8** |
| imgf_bram | 32KB / 18Kb = **18** |
| img_out_bram | 3KB / 18Kb = **2** |
| **总计** | **65 BRAM** ✓ |

剩余: 105 - 65 = **40 BRAM** 用于其他逻辑

### 3.5 主机驱动程序

```python
# host/litho_host_bram.py

import numpy as np
from pynq import Overlay

class LithoBRAMDriver:
    def __init__(self, bitstream):
        self.overlay = Overlay(bitstream)
        self.ip = self.overlay.hls_litho_system_bram_0
        
    def load_data(self, source, mask_fft, mode='TCC'):
        """分批加载数据到 BRAM"""
        
        # 写入 source 数据
        for i, val in enumerate(source.flatten()):
            self.ip.write(0x100 + i*8, val.real)  # 实部
            self.ip.write(0x104 + i*8, val.imag)  # 虚部
        
        # 写入 mask 数据
        for i, val in enumerate(mask_fft.flatten()):
            self.ip.write(0x10000 + i*8, val.real)
            self.ip.write(0x10004 + i*8, val.imag)
        
        print(f"Data loaded: {source.size} source, {mask_fft.size} mask")
    
    def run_tcc(self, Lx, Ly, Nx, Ny, srcSize):
        """运行 TCC 模式"""
        # 配置参数
        self.ip.write(0x00, 1)        # mode = TCC
        self.ip.write(0x04, Lx)
        self.ip.write(0x08, Ly)
        self.ip.write(0x0C, Nx)
        self.ip.write(0x10, Ny)
        self.ip.write(0x14, srcSize)
        
        # 启动
        self.ip.write(0x20, 1)  # ap_start
        
        # 等待完成
        while (self.ip.read(0x24) & 0x1) == 0:
            pass
        
        # 读取结果
        imgf = np.zeros((Lx, Ly), dtype=complex)
        for i in range(Lx * Ly):
            real = self.ip.read(0x20000 + i*8)
            imag = self.ip.read(0x20004 + i*8)
            imgf.flat[i] = complex(real, imag)
        
        return imgf

# 使用示例
driver = LithoBRAMDriver("hls_litho_bram.bit")
driver.load_data(source_data, mask_data, mode='TCC')
result = driver.run_tcc(Lx=64, Ly=64, Nx=7, Ny=7, srcSize=64)
```

---

## 四、方案B: 数据流架构 (高性能)

### 4.1 设计理念

将大数组拆分为流式处理，逐批处理数据，减少片内存储需求。

```
主机数据流 ────▶ PCIe ────▶ AXI-Stream ────▶ HLS核心 ────▶ AXI-Stream ────▶ PCIe ────▶ 主机
                      │                                │
                      │                                │
                      ▼                                ▼
                 极小片内缓存                     极小片内缓存
                  (~1KB)                          (~1KB)
```

### 4.2 流式接口设计

```cpp
void hls_litho_stream(
    // 输入流 (AXI-Stream)
    hls::stream<axis_data> &s_source,
    hls::stream<axis_data> &s_mask,
    
    // 输出流 (AXI-Stream)
    hls::stream<axis_data> &s_output,
    
    // 控制参数 (AXI-Lite)
    int mode,
    int Lx, int Ly, int Nx, int Ny
) {
#pragma HLS INTERFACE axis port=s_source
#pragma HLS INTERFACE axis port=s_mask
#pragma HLS INTERFACE axis port=s_output
#pragma HLS INTERFACE s_axilite port=mode
#pragma HLS INTERFACE s_axilite port=return
    
    // 本地行缓存 (仅需少量BRAM)
    cmpxFloat source_line[64];  // 单行缓存
    cmpxFloat mask_line[64];
#pragma HLS BIND_STORAGE variable=source_line type=RAM_2P impl=BRAM
#pragma HLS BIND_STORAGE variable=mask_line type=RAM_2P impl=BRAM
    
    // 流式处理
    for (int y = 0; y < Ly; y++) {
        for (int x = 0; x < Lx; x++) {
#pragma HLS PIPELINE II=1
            
            // 读取输入流
            axis_data src_pkt = s_source.read();
            axis_data msk_pkt = s_mask.read();
            
            // 本地处理
            cmpxFloat result = process_pixel(src_pkt.data, msk_pkt.data);
            
            // 写入输出流
            axis_data out_pkt;
            out_pkt.data = result;
            out_pkt.last = (x == Lx-1 && y == Ly-1);
            s_output.write(out_pkt);
        }
    }
}
```

### 4.3 资源优势

- **存储需求**: 仅需 ~1KB 行缓存
- **BRAM使用**: < 5 个
- **适用场景**: 实时视频流处理

---

## 五、方案C: 主机内存模式 (快速验证)

### 5.1 设计理念

让主机CPU直接访问BRAM数据，通过PCIe/以太网以轮询方式读写。

### 5.2 简化架构

```
┌──────────────────────────────────────────────┐
│              xcku3p FPGA                      │
│                                              │
│  ┌──────────┐    ┌──────────────────────┐   │
│  │ PCIe     │    │   BRAM (双端口)      │   │
│  │ AXI-Lite │◄──▶│   - Port A: CPU访问  │   │
│  │          │    │   - Port B: HLS核心   │   │
│  └──────────┘    └──────────────────────┘   │
│                           │                  │
│                           ▼                  │
│                  ┌──────────────────┐       │
│                  │  HLS Litho Core  │       │
│                  └──────────────────┘       │
└──────────────────────────────────────────────┘
```

### 5.3 主机轮询访问

```python
# 伪代码
def run_litho_on_fpga():
    # 1. CPU写数据到BRAM
    for addr in range(0, 64*64):
        fpga.write_bram(addr, source_data[addr])
    
    # 2. 启动HLS核心
    fpga.start_core(mode=1, params={...})
    
    # 3. 等待完成
    while not fpga.is_done():
        time.sleep(0.001)
    
    # 4. CPU读取结果
    result = []
    for addr in range(0, 64*64):
        result.append(fpga.read_bram(addr))
    
    return result
```

**优点**: 开发工作量最小  
**缺点**: 数据传输开销大，性能低

---

## 六、推荐实施路径

### 第一阶段: 快速验证 (方案C)

1. **目标**: 在板上运行，验证功能正确性
2. **工作量**: 1-2天
3. **步骤**:
   - 修改 HLS 接口为 AXI-Lite + BRAM
   - 生成比特流
   - 编写简单主机程序测试

### 第二阶段: 性能优化 (方案A)

1. **目标**: 提升数据传输效率
2. **工作量**: 3-5天
3. **步骤**:
   - 实现 DMA 传输或批量加载接口
   - 优化 BRAM 访问时序
   - 性能基准测试

### 第三阶段: 实时处理 (方案B) - 可选

1. **目标**: 支持连续流式处理
2. **工作量**: 1-2周
3. **步骤**:
   - 重构算法为流式架构
   - 实现 AXI-Stream 接口
   - 与前端数据源集成

---

## 七、立即可执行的修改

### 修改 HLS 顶层接口

```bash
# 创建新的BRAM接口文件
touch src/hls_litho_system_bram.cpp
touch include/hls_litho_system_bram.h

# 创建新的HLS配置
touch script/hls_config_bram.cfg
```

### 运行 HLS 综合

```bash
# 在Vitis HLS中
vitis_hls
> open_project hls_litho_system_bram_proj
> add_files src/hls_litho_system_bram.cpp
> add_files -tb testbench/litho_system_tb.cpp
> set_top hls_litho_system_bram
> open_solution solution1
> set_part xcku3p-ffvb676-2-e
> create_clock -period 5 -name default
> csynth_design
> export_design -format ip_catalog
```

---

## 八、风险评估

| 风险项 | 级别 | 缓解措施 |
|--------|------|----------|
| BRAM容量不足 | 中 | 减少数组尺寸 (Lx/Ly=32) |
| 数据传输慢 | 低 | 使用DMA或批量传输 |
| 时序不满足 | 低 | 已验证200MHz可用 |
| 工具链问题 | 低 | 使用相同版本Vitis |

---

## 九、总结

**推荐方案**: **方案A (AXI-Lite + BRAM)**

**理由**:
1. ✅ 片内存储充足 (需求98KB vs 可用230KB)
2. ✅ 接口修改简单 (只改顶层)
3. ✅ 性能适中 (PCIe传输开销可接受)
4. ✅ 验证过的核心 (200MHz II=4)

**预估性能**:
- 数据加载: ~10ms (PCIe Gen3)
- 计算: ~5ms (200MHz)
- 总延迟: ~15ms/帧

**下一步**: 创建 `hls_litho_system_bram.cpp` 并运行HLS综合