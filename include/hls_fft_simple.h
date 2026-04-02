/*
 * K-Litho HLS Simplified FFT Header
 * 参考Vitis官方 interface_stream/fft_top.h 实现
 */

#ifndef HLS_FFT_SIMPLE_H
#define HLS_FFT_SIMPLE_H

#include "../include/hls_types.h"
#include <hls_stream.h>

// ============================================================
// FFT核心函数 (与官方interface_stream完全一致)
// ============================================================

/**
 * FFT顶层函数 - 简化接口
 * @param dir        方向: 0=正向FFT, 1=逆向IFFT
 * @param scaling    缩放因子: 每两位控制一级FFT缩放
 *                   推荐使用SCALING_FFT (0x1555) 防止溢出
 * @param xn         输入流 (定点复数)
 * @param xk         输出流 (定点复数)
 * @param status     状态输出 (溢出标志)
 */
void fft_top(
    ap_uint<1> dir,
    ap_uint<15> scaling,
    hls::stream<cmpxFixedIn> &xn,
    hls::stream<cmpxFixedOut> &xk,
    bool* status
);

// ============================================================
// 简化版FFT接口 (浮点输入/输出，内部定点处理)
// ============================================================

/**
 * 正向FFT (浮点接口)
 * 实数输入 -> 复数输出
 */
void hls_fft_forward_simple(
    hls::stream<realFloat> &real_in,
    hls::stream<cmpxFloat> &cmplx_out,
    int total_size
);

/**
 * 逆向IFFT (浮点接口)
 * 复数输入 -> 实数输出
 */
void hls_fft_inverse_simple(
    hls::stream<cmpxFloat> &cmplx_in,
    hls::stream<realFloat> &real_out,
    int total_size
);

/**
 * FFT完整测试流程 (浮点接口)
 * 输入 -> FFT -> IFFT -> 输出
 */
void hls_top_simple(
    hls::stream<realFloat> &data_in,
    hls::stream<realFloat> &data_out,
    int sizeX,
    int sizeY
);

#endif // HLS_FFT_SIMPLE_H