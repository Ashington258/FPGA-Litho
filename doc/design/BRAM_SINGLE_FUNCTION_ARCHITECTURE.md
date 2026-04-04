# BRAM Single-Function Architecture Design

## 问题分析

### 当前问题
1. **BIND_STORAGE pragma限制**: 只能在函数作用域内使用，不能在全局作用域使用
2. **全局数组无法正确绑定**: HLS优化时全局数组可能被优化掉
3. **多函数架构复杂**: 数据加载、计算、读取分离导致BRAM无法正确绑定

### HLS BRAM绑定要求
- BIND_STORAGE必须在函数内使用
- 数组必须在函数内部声明或通过参数传递
- 静态局部数组可以持久化数据但无法跨函数访问

## 新架构方案

### 单函数架构 (Recommended)

```cpp
void hls_litho_system_bram(
    // 控制参数
    int operation,    // 0=load_source, 1=load_mask, 2=load_tcc, 
                      // 3=load_kernels, 4=load_scales,
                      // 5=compute_tcc, 6=compute_socs,
                      // 7=read_imgf, 8=read_img_out, 9=reset
    
    // 数据参数
    int idx,          // 索引 (用于load/read操作)
    cmpxFloat val,    // 值 (用于load操作)
    
    // 计算参数
    int mode,         // 1=TCC, 2=SOCS
    int Lx, int Ly,   // 频域尺寸
    int Nx, int Ny,   // TCC/SOCS参数
    int srcSize,      // 光源尺寸
    int nkernels      // SOCS核数量
) {
    // 静态局部BRAM数组 (持久化数据)
    static cmpxFloat source_bram[BRAM_SOURCE_SIZE];
    static cmpxFloat mask_bram[BRAM_MASK_SIZE];
    ...
    
    // BIND_STORAGE pragma (函数作用域)
#pragma HLS BIND_STORAGE variable=source_bram type=RAM_2P impl=BRAM
#pragma HLS BIND_STORAGE variable=mask_bram type=RAM_2P impl=BRAM
    ...
    
    // 根据operation执行相应操作
    switch(operation) {
        case 0: source_bram[idx] = val; break;
        case 5: hls_litho_tcc_compute(...); break;
        ...
    }
}
```

### 优势
1. **BRAM正确绑定**: 所有数组在函数内声明，BIND_STORAGE有效
2. **数据持久化**: 静态局部数组在调用间保持数据
3. **HLS优化友好**: 单函数架构便于HLS分析和优化
4. **AXI-Lite接口**: 所有参数通过AXI-Lite传递，无需复杂接口

### 操作编码
| Operation | 描述 | 参数使用 |
|-----------|------|---------|
| 0 | load_source | idx, val |
| 1 | load_mask | idx, val |
| 2 | load_tcc | idx, val |
| 3 | load_kernels | idx, val |
| 4 | load_scales | idx, val |
| 5 | compute_tcc | mode, Lx, Ly, Nx, Ny, srcSize |
| 6 | compute_socs | mode, Lx, Ly, Nx, Ny, nkernels |
| 7 | read_imgf | idx (返回val) |
| 8 | read_img_out | idx (返回val) |
| 9 | reset | 无参数 |

## 预期BRAM使用

### 资源估算
| 数组 | 尺寸 | BRAM_18K块数 |
|------|------|-------------|
| source_bram | 4096 × 64bit | 16 |
| mask_bram | 4096 × 64bit | 16 |
| tcc_bram | 49 × 64bit | 1 |
| kernels_bram | 1800 × 64bit | 8 |
| scales_bram | 8 × 32bit | 0 (寄存器) |
| imgf_bram | 4096 × 64bit | 16 |
| img_out_bram | 4096 × 32bit | 8 |
| **总计** | | **~57块** |

xcku3p可用BRAM: 540块，利用率: ~11%

## 实现计划

### Phase 6C: 单函数架构重构
1. 重写 `hls_litho_system_bram.cpp` 为单函数架构
2. 实现operation switch逻辑
3. 保留简化计算实现
4. 添加BIND_STORAGE和ARRAY_PARTITION pragma
5. 运行C仿真和HLS综合验证BRAM资源

### Phase 6D: 验证与优化
1. C仿真验证所有操作
2. HLS综合检查BRAM使用（目标: ≥57块）
3. 检查Fmax（目标: ≥200MHz）
4. 生成报告和文档

## 参考文档
- HLS UG1448: BIND_STORAGE pragma使用规则
- doc/BRAM_INTERFACE_MAPPING.md: 地址映射设计