/*
 * FPGA-Litho HLS FFT R2C Header
 * FFT实数到复数变换头文件
 * 
 * 重构版本：移除手动频域重组函数声明
 */

#ifndef HLS_FFT_R2C_H
#define HLS_FFT_R2C_H

#include "../include/hls_types.h"
#include <hls_stream.h>

// ============================================================
// 辅助函数声明
// ============================================================

/**
 * @brief 实数浮点转定点复数 (虚部置0)
 */
void real_to_complex_fixed(
    hls::stream<realFloat> &real_in,
    hls::stream<cmpxFixedIn> &cmplx_out,
    int size
);

/**
 * @brief 定点复数转浮点复数
 */
void fixed_to_complex_float(
    hls::stream<cmpxFixedOut> &cmplx_in,
    hls::stream<cmpxFloat> &cmplx_out,
    int size
);

// ============================================================
// 顶层函数声明
// ============================================================

/**
 * @brief FFT R2C 完整流程 (简化版)
 * 
 * 流程:
 * 1. 实数浮点 -> 定点复数 (虚部置0)
 * 2. FFT核心处理 (natural_order输出)
 * 3. 定点复数 -> 浮点复数
 * 
 * 注意: 已移除手动频域重组，依赖FFT IP的natural_order
 */
void hls_fft_r2c(
    hls::stream<realFloat> &real_in,
    hls::stream<cmpxFloat> &cmplx_out,
    int sizeX,
    int sizeY
);

#endif // HLS_FFT_R2C_H
    hls::stream<realFloat> &data_in,
    hls::stream<cmpxFloat> &data_out,
    int sizeX,
    int sizeY
);

#endif // HLS_FFT_R2C_H