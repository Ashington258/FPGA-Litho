# FPGA-Litho FFT 重构计划

**目标**: 采用官方 `interface_stream` 的 FFT 实现方案，修复当前实现的缺陷  
**参考**: `../interface_stream/fft_top.h` 和 `../interface_stream/fft_top.cpp`  
**日期**: 2026-04-02

---

## Phase 1: 类型定义重构

### [ ] 1.1 更新 `hls_types.h` 使用定点数

将浮点类型改为与官方一致的定点类型：

```cpp
// 当前 (错误)
typedef float realFloat;
typedef std::complex<float> cmpxFloat;

// 目标 (官方方案)
const char FFT_INPUT_WIDTH = 16;
typedef ap_fixed<FFT_INPUT_WIDTH, 1> fft_data_t;
typedef std::complex<fft_data_t> cmpxFixed;
```

**文件**: `include/hls_types.h`  
**依赖**: 无  
**风险**: 低 - 仅类型定义变更

---

### [ ] 1.2 保留浮点接口用于外部交互

添加浮点 <-> 定点转换辅助函数：

```cpp
// 浮点转定点 (用于输入接口)
inline cmpxFixed float_to_fixed(cmpxFloat f) {
    return cmpxFixed(fft_data_t(f.real()), fft_data_t(f.imag()));
}

// 定点转浮点 (用于输出接口)
inline cmpxFloat fixed_to_float(cmpxFixed f) {
    return cmpxFloat(f.real().to_float(), f.imag().to_float());
}
```

**文件**: `include/hls_types.h`  
**依赖**: 1.1 完成后进行  
**风险**: 低

---

## Phase 2: FFT 核心重构

### [ ] 2.1 简化 `fft_ip_core` 函数

移除不必要的封装，直接采用官方实现：

```cpp
// 当前 (复杂)
void fft_ip_core(ap_uint<1> dir, ap_uint<15> scaling_schedule, 
                 hls::stream<cmpxFixed> &xn, hls::stream<cmpxFixed> &xk, bool* status)

// 目标 (官方简化版)
void fft_top(ap_uint<1> dir, ap_uint<15> scaling,
             hls::stream<cmpxFixed> &xn, hls::stream<cmpxFixed> &xk, bool* status)
```

**文件**: `src/hls_fft_simple.cpp`  
**依赖**: 1.1 完成后进行  
**风险**: 中 - 需确保接口兼容

---

### [ ] 2.2 修复 `fft_config_t` 配置结构

确保配置参数与官方完全一致：

```cpp
struct fft_config_t : hls::ip_fft::params_t {
    static const unsigned input_width = FFT_INPUT_WIDTH;
    static const unsigned output_width = FFT_OUTPUT_WIDTH;
    static const unsigned output_ordering = hls::ip_fft::natural_order;
    // 添加缺失的配置项
    static const unsigned fft_length = FFT_LENGTH;
    static const unsigned num_channels = FFT_CHANNELS;
    static const unsigned scaling = hls::ip_fft::scaled;
};
```

**文件**: `include/hls_types.h`  
**依赖**: 1.1  
**风险**: 中 - 配置错误可能导致综合失败

---

## Phase 3: R2C/C2R 重构

### [ ] 3.1 移除手动频域重组逻辑

删除 `hls_fft_r2c.cpp` 中的复杂重组代码：
- 删除 `R[wt * sizeY]` 和 `L[wt * sizeY]` 数组
- 删除 `fft_output_reorder()` 函数中的共轭对称处理
- 直接使用 FFT IP 的 `natural_order` 输出

**文件**: `src/hls_fft_r2c.cpp`  
**依赖**: 2.1 完成后进行  
**风险**: 高 - 需验证输出正确性

---

### [ ] 3.2 简化 R2C 流程

采用官方简单流式处理：

```cpp
void hls_fft_r2c(hls::stream<realFloat> &real_in, 
                 hls::stream<cmpxFixed> &cmplx_out,
                 int sizeX, int sizeY) {
    // Step 1: 实数转定点复数
    // Step 2: 调用 fft_top
    // Step 3: 输出 (无需手动重组)
}
```

**文件**: `src/hls_fft_r2c.cpp`  
**依赖**: 3.1  
**风险**: 中

---

### [ ] 3.3 简化 C2R 流程

同样移除 `fft_input_reorder()` 中的复杂逻辑：

```cpp
void hls_fft_c2r(hls::stream<cmpxFixed> &cmplx_in,
                 hls::stream<realFloat> &real_out,
                 int sizeX, int sizeY) {
    // Step 1: 调用 fft_top (dir=1 为 IFFT)
    // Step 2: 提取实部
    // Step 3: 定点转浮点
}
```

**文件**: `src/hls_fft_c2r.cpp`  
**依赖**: 3.1  
**风险**: 中

---

## Phase 4: 缩放机制修复

### [ ] 4.1 实现动态缩放调度

根据输入数据动态范围计算 `scaling_schedule`：

```cpp
// scaling_schedule: 15 位，每 2 位控制一级 FFT 缩放
// 00=无缩放，01=缩放 1bit, 10=缩放 2bit, 11=不缩放
ap_uint<15> compute_scaling_schedule(float input_max, int fft_stages) {
    ap_uint<15> schedule = 0;
    // 根据输入幅度和级数计算每级缩放
    // 1024 点 FFT 有 10 级，每级可能需要缩放防止溢出
    return schedule;
}
```

**文件**: `include/hls_types.h` (新增辅助函数)  
**依赖**: 2.1  
**风险**: 中 - 需理解 FFT 缩放原理

---

### [ ] 4.2 更新所有 FFT 调用点

修改所有 `fft_ip_core()` 调用，传入正确的缩放参数：

| 文件                    | 当前调用                     | 目标调用                           |
| ----------------------- | ---------------------------- | ---------------------------------- |
| `hls_fft_simple.cpp:73` | `fft_ip_core(0, 0, ...)`     | `fft_ip_core(0, scaling, ...)`     |
| `hls_fft_r2c.cpp:42`    | `hls::fft<...>(..., 0, ...)` | `hls::fft<...>(..., scaling, ...)` |
| `hls_fft_c2r.cpp:95`    | `hls::fft<...>(..., 0, ...)` | `hls::fft<...>(..., scaling, ...)` |

**文件**: 多个源文件  
**依赖**: 4.1  
**风险**: 低

---

## Phase 5: 测试框架升级

### [ ] 5.1 添加官方 stimulus 文件格式支持

创建 `.dat` 文件生成器，格式匹配官方：

```
// data/stimulus_00.dat 格式
<NFFT>          // FFT 点数 (十六进制)
<CP_LEN>        // 循环前缀长度
<FWD_INV>       // 0=正向，1=逆向
<sc_sch>        // 缩放调度
<re_hex> <im_hex> <re_float> <im_float>  // 每行数据
...
```

**文件**: `testbench/fft_tb.cpp` (新增函数)  
**依赖**: 无  
**风险**: 低

---

### [ ] 5.2 添加黄金结果对比

从官方 `data/stimulus_*.res` 导入黄金结果，进行逐点对比：

```cpp
bool verify_against_golden(cmpxFixed* output, const char* golden_file) {
    // 读取 .res 文件
    // 逐点对比
    // 返回误差统计
}
```

**文件**: `testbench/fft_tb.cpp`  
**依赖**: 5.1  
**风险**: 低

---

### [ ] 5.3 增加溢出检测测试

验证 `status` 信号在溢出时的行为：

```cpp
void test_overflow_detection() {
    // 输入大幅值信号
    // 检查 status 溢出标志
    // 验证 scaled 模式是否有效防止溢出
}
```

**文件**: `testbench/fft_tb.cpp`  
**依赖**: 4.2  
**风险**: 低

---

## Phase 6: 顶层集成

### [ ] 6.1 更新 `hls_top_simple` 接口

保持外部浮点接口，内部使用定点处理：

```cpp
void hls_top_simple(hls::stream<realFloat> &data_in,
                    hls::stream<realFloat> &data_out,
                    int sizeX, int sizeY) {
    // 输入：float -> fixed 转换
    // FFT: 定点处理
    // 输出：fixed -> float 转换
}
```

**文件**: `src/hls_top.cpp`  
**依赖**: 1.2, 3.2, 3.3  
**风险**: 中

---

### [ ] 6.2 更新 `hls_top` 多模式支持

确保 FFT 测试模式 (mode=0) 正常工作：

```cpp
void hls_top(..., int mode) {
    if (mode == 0) {
        // FFT 测试：使用重构后的 FFT 流程
        fft_r2c_pipeline(...);
        fft_c2r_pipeline(...);
    }
    // mode 1, 2 保持不变
}
```

**文件**: `src/hls_top.cpp`  
**依赖**: 6.1  
**风险**: 中

---

### [ ] 6.3 更新接口pragma

确保 AXI 接口配置正确：

```cpp
#pragma HLS INTERFACE axis port=data_in
#pragma HLS INTERFACE axis port=data_out
#pragma HLS INTERFACE s_axilite port=sizeX,sizeY
#pragma HLS INTERFACE s_axilite port=return
```

**文件**: 所有顶层函数  
**依赖**: 6.1  
**风险**: 低

---

## Phase 7: 验证与回归测试

### [ ] 7.1 C 仿真验证

运行 `csim` 验证功能正确性：

```bash
cd FPGA-Litho
# 运行测试
./testbench/fft_tb.exe
```

**验收标准**:
- 正弦波测试：max_error < 1e-3
- 随机数据测试：max_error < 1e-3
- 常数输入测试：max_error < 1e-3

**依赖**: 5.2  
**风险**: 中

---

### [ ] 7.2 C 综合验证

运行 `csynth` 检查资源消耗：

```bash
# 检查资源使用
- DSP slices
- BRAM
- LUT
```

**验收标准**:
- 资源消耗不超过官方实现的 150%
- 时序满足 2ns (500MHz) 要求

**依赖**: 7.1  
**风险**: 高 - 可能需要优化

---

### [ ] 7.3 与原始实现对比

验证重构后输出与原始实现一致：

```cpp
// 保存原始实现输出
// 运行重构后实现
// 对比两者输出
```

**验收标准**: 输出差异 < 1e-4 (考虑定点数量化误差)

**依赖**: 7.1  
**风险**: 低

---

## 文件变更清单

| 文件                       | 变更类型 | 说明                       |
| -------------------------- | -------- | -------------------------- |
| `include/hls_types.h`      | 重大修改 | 定点类型定义、配置结构     |
| `include/hls_fft_simple.h` | 修改     | 函数签名更新               |
| `include/hls_fft_r2c.h`    | 修改     | 函数签名更新               |
| `include/hls_fft_c2r.h`    | 修改     | 函数签名更新               |
| `src/hls_fft_simple.cpp`   | 重大修改 | FFT 核心重构               |
| `src/hls_fft_r2c.cpp`      | 重大修改 | 移除手动重组逻辑           |
| `src/hls_fft_c2r.cpp`      | 重大修改 | 移除手动重组逻辑           |
| `src/hls_top.cpp`          | 修改     | 接口适配                   |
| `testbench/fft_tb.cpp`     | 新增功能 | 官方格式支持、黄金结果对比 |

---

## 风险与缓解

| 风险                   | 影响           | 缓解措施                       |
| ---------------------- | -------------- | ------------------------------ |
| 定点数量化误差         | 精度下降       | 保留浮点接口，内部使用足够位宽 |
| R2C 逻辑简化后功能缺失 | 频域数据错误   | 保留原有测试，增加边界测试     |
| 缩放配置错误           | 溢出或精度损失 | 参考官方 scaling_schedule 计算 |
| 综合时序不达标         | 频率降低       | 优化关键路径，调整 pipeline    |

---

## 参考文档

- 官方实现：`../interface_stream/fft_top.h`
- 官方实现：`../interface_stream/fft_top.cpp`
- 官方测试：`../interface_stream/fft_tb.cpp`
- HLS FFT IP 文档：`hls_fft.h`

---

## 检查清单

- [ ] 所有源文件已备份
- [ ] git 分支已创建 (建议：`feature/fft-refactor`)
- [ ] 测试环境已准备
- [ ] 官方 stimulus 文件已复制