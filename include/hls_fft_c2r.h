/*
 * FPGA-Litho HLS FFT C2R Header
 * FFT复数到实数变换头文件
 * 
 * 重构版本：移除手动输入重组函数声明
 */

#ifndef HLS_FFT_C2R_H
#define HLS_FFT_C2R_H

#include "../include/hls_types.h"
#include <hls_stream.h>

// ============================================================
// 辅助函数声明
// ============================================================

/**
 * @brief 浮点复数转定点复数
 */
void complex_float_to_fixed(
    hls::stream<cmpxFloat> &cmplx_in,
    hls::stream<cmpxFixedIn> &cmplx_out,
    int size
);

/**
 * @brief 定点复数取实部转浮点
 */
void complex_fixed_to_real_float(
    hls::stream<cmpxFixedOut> &cmplx_in,
    hls::stream<realFloat> &real_out,
    int size
);

// ============================================================
// 顶层函数声明
// ============================================================

/**
 * @brief FFT C2R 完整流程 (简化版)
 * 
 * 流程:
 * 1. 浮点复数 -> 定点复数
 * 2. IFFT核心处理 (natural_order输入)
 * 3. 定点复数取实部 -> 浮点实数
 * 
 * 注意: 已移除手动输入重组，直接使用natural_order
 * scaled模式下FFT+IFFT使用相同缩放，幅度自动恢复
 */
void hls_fft_c2r(
    hls::stream<cmpxFloat> &cmplx_in,
    hls::stream<realFloat> &real_out,
    int sizeX,
    int sizeY
);

#endif // HLS_FFT_C2R_H
    hls::stream<cmpxFloat> &data_in,
    hls::stream<realFloat> &data_out,
    int sizeX,
    int sizeY
);

#endif // HLS_FFT_C2R_H