/*
 * FPGA-Litho HLS FFT C2R Module
 * 复数到实数IFFT变换
 * 
 * 重构版本：移除手动输入重组，简化流程
 * 参考: interface_stream/fft_top.cpp
 */

#include "../include/hls_types.h"
#include "../include/hls_fft_simple.h"
#include <hls_stream.h>

using namespace hls;

// ============================================================
// 辅助函数：浮点复数转定点复数
// ============================================================

/**
 * @brief 复数输入预处理
 * 将浮点复数转换为定点复数
 * 
 * @param cmplx_in 浮点复数输入流
 * @param cmplx_out 定点复数输出流 (送入IFFT核)
 * @param size     数据尺寸
 */
void complex_float_to_fixed(
    hls::stream<cmpxFloat> &cmplx_in,
    hls::stream<cmpxFixedIn> &cmplx_out,
    int size
) {
#pragma HLS PIPELINE II=1

    for (int i = 0; i < size; i++) {
#pragma HLS LOOP_TRIPCOUNT min=1024 max=1024 avg=1024
        cmpxFloat val = cmplx_in.read();
        cmplx_out.write(float_to_fixed(val));
    }
}

// ============================================================
// 辅助函数：定点复数取实部转浮点
// ============================================================

/**
 * @brief IFFT输出后处理
 * 从定点复数结果提取实部并转为浮点
 * 
 * @param cmplx_in IFFT核定点输出
 * @param real_out 实数浮点输出流
 * @param size     数据尺寸
 */
void complex_fixed_to_real_float(
    hls::stream<cmpxFixedOut> &cmplx_in,
    hls::stream<realFloat> &real_out,
    int size
) {
#pragma HLS PIPELINE II=1

    for (int i = 0; i < size; i++) {
#pragma HLS LOOP_TRIPCOUNT min=1024 max=1024 avg=1024
        cmpxFixedOut val = cmplx_in.read();
        // IFFT输出应接近实数，取实部
        real_out.write(val.real().to_float());
    }
}

// ============================================================
// 顶层FFT C2R模块 (简化版)
// ============================================================

/**
 * @brief FFT C2R顶层接口 (IFFT)
 * 
 * 流程简化:
 * 1. 浮点复数 -> 定点复数
 * 2. IFFT核心处理 (natural_order输入)
 * 3. 定点复数取实部 -> 浮点实数
 * 
 * 注意: 移除了手动输入重组，直接使用natural_order输入
 * scaled模式下FFT+IFFT使用相同缩放，幅度自动恢复
 * 
 * @param cmplx_in 复数输入流 (频域数据)
 * @param real_out 实数输出流 (需后续移位)
 * @param sizeX    X方向尺寸
 * @param sizeY    Y方向尺寸
 */
void hls_fft_c2r(
    hls::stream<cmpxFloat> &cmplx_in,
    hls::stream<realFloat> &real_out,
    int sizeX,
    int sizeY
) {
#pragma HLS INTERFACE axis port=cmplx_in
#pragma HLS INTERFACE axis port=real_out
#pragma HLS INTERFACE s_axilite port=sizeX
#pragma HLS INTERFACE s_axilite port=sizeY
#pragma HLS INTERFACE s_axilite port=return
#pragma HLS DATAFLOW

    int total_size = sizeX * sizeY;
    
    // 内部流 (使用定点类型)
    hls::stream<cmpxFixedIn> ifft_in("ifft_in");
    hls::stream<cmpxFixedOut> ifft_out("ifft_out");
#pragma HLS STREAM depth=1024 variable=ifft_in
#pragma HLS STREAM depth=1024 variable=ifft_out

    bool status = false;

    // Step 1: 浮点复数转定点复数
    complex_float_to_fixed(cmplx_in, ifft_in, total_size);

    // Step 2: IFFT核心计算 (使用新的fft_top + 固定缩放)
    // dir=1 表示逆向IFFT
    fft_top(1, SCALING_IFFT, ifft_in, ifft_out, &status);

    // Step 3: 定点复数取实部转浮点
    complex_fixed_to_real_float(ifft_out, real_out, total_size);
}