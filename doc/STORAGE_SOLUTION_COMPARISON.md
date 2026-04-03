# FPGA-Litho存储方案比较分析

> 创建日期: 2026-04-03
> 作者: K-Litho Team
> 目标: 分析不同FPGA存储方案的优缺点，为硬件选择提供参考

---

## 一、存储方案概述

FPGA-Litho光刻模拟工具的核心计算需求包括：
- **数据输入**: 光源数据、掩模数据、TCC矩阵、SOCS核数据
- **中间计算**: 频域变换、复数矩阵运算
- **数据输出**: 光学图像结果

不同FPGA器件提供不同的存储资源，存储方案选择直接影响性能、资源利用率和开发复杂度。

---

## 二、常用存储方案分析

### 方案1: DDR/HBM内存方案 (推荐高性能应用)

#### 架构设计
```
FPGA内部计算单元 ↔ AXI-Master接口 ↔ DDR/HBM控制器 ↔ 外部内存
```

#### 适用硬件
- **Xilinx UltraScale+系列**: VU9P, VU13P (HBM2e)
- **Xilinx Versal系列**: VM1502, VM1802 (HBM)
- **Intel Stratix 10/20**: 板载DDR4/HBM
- **优势器件**: xcu280, xcu50, xcu55c (HBM)

#### 技术特点
- **存储容量**: DDR4: 16-64GB, HBM: 16-32GB
- **带宽**: DDR4: 19-25 GB/s, HBM: 460-900 GB/s
- **延迟**: DDR4: ~50-100ns, HBM: ~10-20ns
- **接口**: AXI-Master (64/128/256-bit)

#### 实现优势
✅ **大容量支持**: TCC矩阵Nx=7/9/11无限制
✅ **高带宽**: 支持实时数据流处理
✅ **灵活性**: 动态数据加载，无容量限制
✅ **标准化**: 成熟的AXI接口协议

#### 实现挑战
❌ **开发复杂度**: 需要AXI-Master接口设计
❌ **时序收敛**: 跨时钟域同步困难
❌ **功耗**: HBM功耗较高
❌ **成本**: HBM器件价格昂贵

#### HLS实现要点
```cpp
// AXI-Master接口定义
#pragma HLS INTERFACE m_axi port=source offset=slave bundle=gmem0
#pragma HLS INTERFACE m_axi port=mask offset=slave bundle=gmem1
#pragma HLS INTERFACE m_axi port=tcc offset=slave bundle=gmem2
#pragma HLS INTERFACE m_axi port=kernels offset=slave bundle=gmem3
#pragma HLS INTERFACE m_axi port=scales offset=slave bundle=gmem4
#pragma HLS INTERFACE m_axi port=imgf offset=slave bundle=gmem5
#pragma HLS INTERFACE m_axi port=img_out offset=slave bundle=gmem6

// 数据预取优化
#pragma HLS DATAFLOW
void prefetch_data(cmpxFloat* source, cmpxFloat* mask) {
    // 异步数据预取到本地缓存
}
```

#### 性能评估
- **TCC模式**: Nx=7/9/11, 完整矩阵计算
- **SOCS模式**: 16-32核并行计算
- **加速比**: 200-500x (vs CPU)
- **资源利用**: BRAM: 20-30%, DSP: 40-60%

---

### 方案2: BRAM-only方案 (资源受限器件)

#### 架构设计
```
AXI-Lite控制接口 ↔ 本地BRAM存储 ↔ 计算单元
数据加载 → BRAM缓存 → 计算 → 结果读取
```

#### 适用硬件
- **Xilinx 7系列**: Kintex-7, Virtex-7
- **Xilinx UltraScale**: Kintex UltraScale, Virtex UltraScale
- **资源受限器件**: xcku3p, xcku5p (无DDR)
- **小型FPGA**: Spartan-7, Artix-7

#### 技术特点
- **存储容量**: 7系列: 400-2000块BRAM (1.5-7.5MB)
- **UltraScale**: 720-1440块BRAM (2.8-5.6MB)
- **访问延迟**: 1-2个时钟周期
- **接口**: AXI-Lite控制 + 本地BRAM

#### 实现优势
✅ **低延迟**: BRAM访问延迟极低
✅ **确定性**: 无外部内存访问不确定性
✅ **简单性**: 无需AXI-Master接口设计
✅ **功耗**: 超低功耗设计

#### 实现挑战
❌ **容量限制**: TCC矩阵Nx≤3 (xcku3p限制)
❌ **数据加载**: 需要预加载所有数据
❌ **灵活性差**: 无法处理大尺寸数据
❌ **资源竞争**: BRAM资源紧张

#### HLS实现要点
```cpp
// BRAM存储绑定
#pragma HLS BIND_STORAGE variable=source_bram type=RAM_2P impl=BRAM
#pragma HLS BIND_STORAGE variable=mask_bram type=RAM_2P impl=BRAM
#pragma HLS ARRAY_PARTITION variable=source_bram complete dim=1

// AXI-Lite接口
#pragma HLS INTERFACE s_axilite port=load_source_data bundle=control
#pragma HLS INTERFACE s_axilite port=start_litho_compute bundle=control
#pragma HLS INTERFACE s_axilite port=get_compute_status bundle=control

// 数据加载函数
void load_source_data(int idx, cmpxFloat val) {
    #pragma HLS INLINE
    #pragma HLS PIPELINE II=1
    if (idx >= 0 && idx < L_SOURCE*L_SOURCE) {
        source_bram[idx] = val;
    }
}
```

#### 性能评估
- **TCC模式**: Nx≤3, 受BRAM容量限制
- **SOCS模式**: 8核计算，完整支持
- **加速比**: 50-200x (vs CPU)
- **资源利用**: BRAM: 80-95%, DSP: 20-40%

---

### 方案3: URAM方案 (UltraScale+优化)

#### 架构设计
```
FPGA计算单元 ↔ URAM控制器 ↔ UltraRAM阵列
AXI接口 ↔ URAM缓存 ↔ 计算核心
```

#### 适用硬件
- **Xilinx UltraScale+系列**: VU9P, VU13P, VU19P
- **URAM特性器件**: Kintex UltraScale+, Virtex UltraScale+
- **优势器件**: xcku15p, xcvu9p, xcvu13p

#### 技术特点
- **存储容量**: 每个URAM: 288Kb, 典型配置: 64-128MB
- **访问模式**: 真双端口RAM
- **延迟**: 2-3个时钟周期
- **接口**: 原生RAM接口 + AXI适配

#### 实现优势
✅ **大容量**: 远超BRAM容量限制
✅ **高密度**: 相同面积下容量是BRAM的4-8倍
✅ **低功耗**: 比BRAM功耗更低
✅ **灵活配置**: 支持多种访问模式

#### 实现挑战
❌ **访问限制**: 最小访问粒度64-bit
❌ **时序复杂**: URAM访问时序要求严格
❌ **软件支持**: HLS对URAM支持相对较新
❌ **成本**: UltraScale+器件价格较高

#### HLS实现要点
```cpp
// URAM存储绑定
#pragma HLS BIND_STORAGE variable=large_matrix type=RAM_2P impl=URAM
#pragma HLS BIND_STORAGE variable=tcc_matrix type=RAM_2P impl=URAM

// 数组分区优化
#pragma HLS ARRAY_PARTITION variable=large_matrix cyclic factor=4 dim=1

// 数据预取策略
void prefetch_uram_data(cmpxFloat* uram_buffer, int size) {
    #pragma HLS PIPELINE II=4
    // 批量预取到URAM
}
```

#### 性能评估
- **TCC模式**: Nx=5/7/9, 大矩阵支持
- **SOCS模式**: 16-64核并行计算
- **加速比**: 150-400x (vs CPU)
- **资源利用**: URAM: 60-80%, BRAM: 30-40%

---

### 方案4: AXI-Stream流式处理方案

#### 架构设计
```
数据源 → AXI-Stream → FIFO缓存 → 计算单元 → AXI-Stream → 输出
```

#### 适用硬件
- **所有现代FPGA**: 支持AXI-Stream接口
- **高速接口器件**: 支持高速SerDes
- **实时处理应用**: 需要连续数据流的场景

#### 技术特点
- **数据流**: 连续数据流处理，无需大容量存储
- **FIFO深度**: 可配置1K-64K深度
- **接口标准**: AXI-Stream协议
- **时序**: 同步时钟域

#### 实现优势
✅ **实时性**: 支持连续数据流处理
✅ **低延迟**: 无存储访问延迟
✅ **模块化**: 易于流水线设计
✅ **可扩展**: 易于添加预处理/后处理

#### 实现挑战
❌ **数据依赖**: 计算结果依赖历史数据
❌ **缓存设计**: 需要设计合适FIFO深度
❌ **同步复杂**: 多路数据流同步困难
❌ **调试困难**: 流式数据调试复杂

#### HLS实现要点
```cpp
// AXI-Stream接口
#pragma HLS INTERFACE axis port=input_stream
#pragma HLS INTERFACE axis port=output_stream

// FIFO设计
#pragma HLS STREAM variable=input_fifo depth=1024
#pragma HLS STREAM variable=output_fifo depth=1024

// 流式计算函数
void stream_compute(hls::stream<cmpxFloat>& input,
                   hls::stream<float>& output) {
    #pragma HLS PIPELINE II=1
    // 流式FFT/SOCS计算
}
```

#### 性能评估
- **适用场景**: 连续图像处理流水线
- **TCC模式**: 不适用 (需要完整矩阵)
- **SOCS模式**: 适用于核卷积计算
- **加速比**: 100-300x (vs CPU)
- **资源利用**: BRAM: 10-20% (FIFO), DSP: 30-50%

---

### 方案5: 混合存储方案 (推荐平衡设计)

#### 架构设计
```
外部DDR/HBM + 本地BRAM/URAM缓存 + 流式接口
多级存储层次: 外部存储 ↔ 本地缓存 ↔ 计算单元
```

#### 适用硬件
- **高端FPGA**: VU9P/VU13P (HBM + URAM + BRAM)
- **平衡性能器件**: xcu280, xcu50 (DDR + BRAM)
- **Versal系列**: AI Engine + HBM + BRAM

#### 技术特点
- **存储层次**: HBM/DDR (大容量) + URAM/BRAM (高速缓存)
- **缓存策略**: 预取 + 局部性优化
- **接口组合**: AXI-Master + AXI-Stream + 本地RAM
- **智能调度**: 硬件/软件协同调度

#### 实现优势
✅ **最佳性能**: 结合各方案优点
✅ **灵活扩展**: 支持多种应用场景
✅ **资源平衡**: 充分利用所有存储资源
✅ **未来兼容**: 易于升级和扩展

#### 实现挑战
❌ **设计复杂**: 多存储层次管理复杂
❌ **时序挑战**: 跨存储域同步困难
❌ **调试困难**: 多接口交互调试复杂
❌ **开发周期**: 开发时间较长

#### HLS实现要点
```cpp
// 多接口设计
#pragma HLS INTERFACE m_axi port=external_data offset=slave bundle=gmem
#pragma HLS INTERFACE s_axilite port=cache_config bundle=control

// 本地缓存
#pragma HLS BIND_STORAGE variable=local_cache type=RAM_2P impl=URAM
#pragma HLS BIND_STORAGE variable=fast_buffer type=RAM_2P impl=BRAM

// 缓存管理
void cache_manager(cmpxFloat* external, cmpxFloat* local_cache) {
    #pragma HLS DATAFLOW
    // 智能预取策略
    // LRU替换算法
    // 局部性优化
}
```

#### 性能评估
- **TCC模式**: Nx=7/9/11, 完整大矩阵支持
- **SOCS模式**: 32-128核并行计算
- **加速比**: 300-800x (vs CPU)
- **资源利用**: 综合优化，各资源均衡使用

---

## 三、方案选择指南

### 硬件平台选择矩阵

| 硬件平台 | DDR/HBM | BRAM | URAM | 推荐方案 | 适用场景 |
|---------|---------|------|------|----------|----------|
| xcku3p | ❌ | 720块 | ❌ | BRAM-only | 原型验证 |
| xcku15p | ❌ | 1440块 | 960个 | URAM+BRAM | 中等规模 |
| xcu280 | ✅ DDR | 1440块 | 960个 | 混合存储 | 高性能计算 |
| xcvu9p | ✅ HBM | 1440块 | 1280个 | HBM+URAM | 超高性能 |
| Versal | ✅ HBM | 动态 | 动态 | AI+HBM | 异构计算 |

### 应用场景选择指南

#### 1. 原型开发阶段
- **推荐**: BRAM-only方案
- **理由**: 开发简单，快速验证算法
- **限制**: TCC Nx≤3, 数据尺寸固定

#### 2. 产品化开发
- **推荐**: DDR/HBM + BRAM缓存方案
- **理由**: 性能最优，功能完整
- **要求**: 硬件成本预算充足

#### 3. 资源受限应用
- **推荐**: URAM方案 (UltraScale+)
- **理由**: 容量大，功耗低，性价比高
- **适用**: 中等规模计算需求

#### 4. 实时流处理
- **推荐**: AXI-Stream方案
- **理由**: 低延迟，连续处理
- **适用**: 视频/图像实时处理

### 性能对比总结

| 方案 | 存储容量 | 访问延迟 | 开发难度 | 功耗 | 成本 | TCC支持 | SOCS支持 |
|------|----------|----------|----------|------|------|----------|----------|
| DDR/HBM | 16-64GB | 中等 | 高 | 高 | 高 | 完整Nx | 完整核数 |
| BRAM-only | 1-6MB | 极低 | 低 | 低 | 低 | Nx≤3 | 8核 |
| URAM | 32-128MB | 低 | 中 | 低 | 中 | Nx≤9 | 32核 |
| AXI-Stream | 无限 | 低 | 中 | 低 | 中 | 有限 | 流式 |
| 混合存储 | 16-64GB | 低-中等 | 高 | 中 | 高 | 完整Nx | 完整核数 |

---

## 四、迁移指南

### 从BRAM方案升级到DDR方案

1. **接口升级**:
   ```cpp
   // 从AXI-Lite改为AXI-Master
   #pragma HLS INTERFACE s_axilite port=load_data bundle=control
   // 改为:
   #pragma HLS INTERFACE m_axi port=data offset=slave bundle=gmem
   ```

2. **存储管理**:
   ```cpp
   // 从本地BRAM改为外部内存
   cmpxFloat source_bram[L*L];  // 本地BRAM
   // 改为:
   cmpxFloat* source;  // 外部指针
   ```

3. **数据流优化**:
   ```cpp
   // 添加DATAFLOW和预取
   #pragma HLS DATAFLOW
   void prefetch_data(cmpxFloat* source) {
       // 异步数据预取
   }
   ```

### 扩展性考虑

1. **模块化设计**: 存储接口抽象层，便于方案切换
2. **参数化配置**: 存储容量和接口类型可配置
3. **性能监控**: 各存储层的访问统计和性能分析
4. **容错设计**: 存储访问错误处理和恢复机制

---

## 五、总结与建议

### 核心建议

1. **原型阶段**: 从BRAM-only方案开始，快速验证算法正确性
2. **性能优化**: 根据目标硬件选择DDR/HBM或URAM方案
3. **成本平衡**: URAM方案在UltraScale+上提供最佳性价比
4. **未来扩展**: 设计时考虑混合存储架构，便于升级

### 技术发展趋势

- **HBM普及**: Versal和UltraScale+系列HBM将成为主流
- **URAM成熟**: URAM将成为BRAM的重要补充
- **AI集成**: 与AI Engine结合的异构存储架构
- **3D堆叠**: 晶圆级3D堆叠技术提升存储密度

### 开发最佳实践

1. **分层设计**: 存储接口、计算核心、控制逻辑分离
2. **性能建模**: 早期建立性能模型，指导方案选择
3. **模块复用**: 设计可复用的存储管理模块
4. **文档完整**: 详细记录存储方案选择理由和性能数据

---

*本文档将根据硬件更新和技术发展持续维护*