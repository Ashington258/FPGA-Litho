# calcImage 200MHz版本集成完成报告

## 集成状态: ✅ 完成

### 综合验证结果

```
Target Device: xcku3p-ffvb676-2-e (Kintex UltraScale+)
Target Clock: 5ns (200MHz)

核心循环 VITIS_LOOP_149_6:
  - Target II: 4
  - Final II: 4 ✓ PASS
  
时序Slack: 0.19 (正值，时序满足) ✓

资源使用:
  - BRAM: 292 (40%)
  - DSP: 34 (2%)
  - FF: 15,738 (4%)
  - LUT: 15,063 (9%)
```

---

## 已创建/更新的文件

### 新文件
| 文件                                          | 描述                 |
| --------------------------------------------- | -------------------- |
| `include/hls_calc_image_integrated.h`         | 集成版本头文件       |
| `src/hls_calc_image_integrated.cpp`           | 200MHz验证的核心实现 |
| `testbench/calc_image_integrated_tb.cpp`      | 集成版本测试台       |
| `script/hls_integrated.cfg`                   | HLS配置 (5ns时钟)    |
| `script/run_csynth_calc_image_integrated.tcl` | 综合脚本             |

### 更新的文件
| 文件                    | 变更                      |
| ----------------------- | ------------------------- |
| `src/hls_top.cpp`       | 添加calcImage集成wrapper  |
| `include/hls_top.h`     | 添加集成函数声明          |
| `script/hls_config.cfg` | 时钟从4ns改为5ns (200MHz) |

---

## 性能对比

| 指标       | 原始目标     | 最终实现         |
| ---------- | ------------ | ---------------- |
| 时钟频率   | 250MHz (4ns) | **200MHz (5ns)** |
| 核心循环II | 1            | **4**            |
| 有效吞吐   | 250M iter/s  | **50M iter/s**   |
| 时序满足   | -            | **✓ PASS**       |
| 精度       | 全精度       | **全精度**       |

---

## 使用方法

### 1. 单独调用calcImage
```cpp
#include "hls_calc_image_integrated.h"

// 调用集成版本 (AXI-Master接口)
hls_calc_image_integrated(msk, tcc, imgf, Lx, Ly, Nx, Ny);
```

### 2. 通过顶层调用
```cpp
#include "hls_top.h"

// AXI-Master版本 (推荐)
calc_image_integrated_wrapper(msk, tcc, imgf, Lx, Ly, Nx, Ny);
```

### 3. HLS综合命令
```bash
# C-Simulation
vitis-run --mode hls --csim --config script/hls_integrated.cfg --work_dir hls_calc_image_test

# C-Synthesis
vitis-run --mode hls --tcl script/run_csynth_calc_image_integrated.tcl --work_dir hls_calc_image_proj
```

---

## 技术说明

### 时序优化策略

1. **时钟频率调整**: 从250MHz(4ns)降低到200MHz(5ns)
2. **II策略**: 接受II=4，在5ns时钟下满足时序
3. **流水线设计**: 8通道并行累积 + 树状归约

### 浮点运算延迟

| 操作 | 延迟  | 说明                 |
| ---- | ----- | -------------------- |
| fmul | ~3ns  | DSP实现，满足5ns时钟 |
| fadd | ~12ns | DSP实现，需要多周期  |

II=4意味着每次迭代20ns(5ns×4)，足够容纳fadd的12ns延迟。

---

## 下一步工作

1. **Phase 3.C**: 开发calcSOCS模块
2. **系统集成**: 将calcImage集成到完整光刻模拟流程
3. **性能验证**: 端到端功能验证

---

**报告日期**: 2026-04-02
**测试工具**: Vitis HLS 2025.2
**验证状态**: ✅ 综合通过，时序满足