# calcImage模块HLS综合分析报告

## 1. 综合结果概览

| 指标       | 目标          | 实际                    | 状态       |
| ---------- | ------------- | ----------------------- | ---------- |
| 时钟频率   | 250 MHz (4ns) | **38.2 MHz** (26.144ns) | ⚠️ 严重偏离 |
| 内层循环II | 1             | 1                       | ✓ 达成     |
| 迭代延迟   | -             | 35 cycles               | -          |

## 2. 资源利用率

| 资源     | 使用量 | 可用量  | 占比 | 状态   |
| -------- | ------ | ------- | ---- | ------ |
| BRAM_18K | 76     | 720     | 11%  | ✓ 合理 |
| DSP      | 51     | 1368    | 4%   | ✓ 合理 |
| FF       | 10,232 | 325,440 | 3%   | ✓ 合理 |
| LUT      | 11,597 | 162,720 | 7%   | ✓ 合理 |

## 3. 性能瓶颈分析

### 3.1 关键路径

```
路径: fadd(12.653ns) → select(0.411ns) → store(0.427ns) → load → fadd
总延迟: 26.144 ns (目标4ns)
```

### 3.2 根因分析

**问题**: 内层累加循环的浮点累加器 `val` 形成链式依赖

```cpp
// 当前实现 (有问题)
for (int ny1_idx = 0; ny1_idx < 2*Ny + 1; ny1_idx++) {
    for (int nx1_idx = 0; nx1_idx < 2*Nx + 1; nx1_idx++) {
        #pragma HLS PIPELINE II=1
        // 每次迭代依赖前一次的val值
        val = cmpxFloat(val.real() + result_real, val.imag() + result_imag);
    }
}
```

**原理**: 浮点加法需要多周期完成（约12.653ns），PIPELINE II=1要求每周期开始新迭代，但val累加器需要等待前一迭代完成。

## 4. 优化策略

### 4.1 累加器数组化 (类似TCC优化)

**核心思想**: 使用本地累加器数组替代单个累加变量，消除循环依赖

```cpp
// 优化方案: 累加器数组 + 最后求和
// 1. 累加器数组 (每个输出位置独立的累加通道)
cmpxFloat acc_array[CI_ACC_CHANNELS];  // 多通道并行累加
#pragma HLS ARRAY_PARTITION variable=acc_array complete

// 2. 累加阶段 - 无依赖
for (int iter = 0; iter < total_iters; iter++) {
    #pragma HLS PIPELINE II=1
    int channel = iter % CI_ACC_CHANNELS;
    acc_array[channel] += compute_value(iter);
}

// 3. 最后求和阶段 - 独立操作
cmpxFloat final_result(0,0);
for (int c = 0; c < CI_ACC_CHANNELS; c++) {
    #pragma HLS PIPELINE II=1
    final_result += acc_array[c];
}
```

### 4.2 累加器位宽扩展

防止累加溢出，使用更高精度累加器：

```cpp
// 使用双精度累加器 (可选)
typedef std::complex<double> accFloat;
accFloat acc_array[CI_ACC_CHANNELS];
```

### 4.3 循环分离

将外层循环和内层累加分离，使用DATAFLOW：

```cpp
#pragma HLS DATAFLOW

// 阶段1: 数据预取
prefetch_data(msk, tcc);

// 阶段2: 并行计算
compute_all_outputs(imgf_cache);

// 阶段3: 结果写回
writeback_results(imgf_cache, imgf);
```

## 5. 预期优化效果

参考TCC优化结果 (12.1x时钟提升):

| 优化后预期 | 数值           |
| ---------- | -------------- |
| 时钟频率   | > 250 MHz      |
| 内层循环II | 1              |
| DSP利用率  | 优化后约20-30% |

## 6. 下一步计划

1. 实现累加器数组优化版本
2. 创建优化测试平台
3. 运行HLS综合验证
4. 对比前后性能差异

---

**报告日期**: 2026-04-02
**综合工具**: Vitis HLS 2025.2
**目标器件**: xcku3p-ffvb676-2-e