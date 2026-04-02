/*
 * K-Litho HLS FFT R2C Module
 * 实数到复数FFT变换
 * 
 * 重构版本：移除手动频域重组，使用natural_order输出
 * 参考: interface_stream/fft_top.cpp
 */

#include "../include/hls_types.h"
#include "../include/hls_fft_simple.h"
#include <hls_stream.h>

using namespace hls;

// ============================================================
// 辅助函数：实数转定点复数
// ============================================================

/**
 * @brief 实数输入预处理模块
 * 将实数数据转换为定点复数格式 (虚部置0)
 * 
 * @param real_in  实数浮点输入流
 * @param cmplx_out 定点复数输出流 (送入FFT核)
 * @param size     数据尺寸
 */
void real_to_complex_fixed(
    hls::stream<realFloat> &real_in,
    hls::stream<cmpxFixedIn> &cmplx_out,
    int size
) {
#pragma HLS PIPELINE II=1

    for (int i = 0; i < size; i++) {
#pragma HLS LOOP_TRIPCOUNT min=1024 max=1024 avg=1024
        realFloat val = real_in.read();
        // 实部为输入值，虚部为0
        cmplx_out.write(cmpxFixedIn(fft_data_t(val), fft_data_t(0)));
    }
}

// ============================================================
// 辅助函数：定点复数转浮点复数
// ============================================================

/**
 * @brief FFT输出后处理
 * 将定点复数转换为浮点复数
 * 
 * @param cmplx_in FFT核定点输出
 * @param cmplx_out 浮点复数输出
 * @param size     数据尺寸
 */
void fixed_to_complex_float(
    hls::stream<cmpxFixedOut> &cmplx_in,
    hls::stream<cmpxFloat> &cmplx_out,
    int size
) {
#pragma HLS PIPELINE II=1

    for (int i = 0; i < size; i++) {
#pragma HLS LOOP_TRIPCOUNT min=1024 max=1024 avg=1024
        cmpxFixedOut val = cmplx_in.read();
        cmplx_out.write(fixed_to_float(val));
    }
}

// ============================================================
// 顶层FFT R2C模块 (简化版)
// ============================================================

/**
 * @brief FFT R2C顶层接口
 * 
 * 流程简化:
 * 1. 实数浮点 -> 定点复数 (虚部置0)
 * 2. FFT核心处理 (natural_order输出)
 * 3. 定点复数 -> 浮点复数
 * 
 * 注意: 移除了手动频域重组，依赖FFT IP的natural_order输出
 * R2C的FFT输出天然具有共轭对称性
 * 
 * @param real_in   实数输入流 (需预先移位)
 * @param cmplx_out 复数输出流
 * @param sizeX     X方向尺寸
 * @param sizeY     Y方向尺寸
 */
void hls_fft_r2c(
    hls::stream<realFloat> &real_in,
    hls::stream<cmpxFloat> &cmplx_out,
    int sizeX,
    int sizeY
) {
#pragma HLS INTERFACE axis port=real_in
#pragma HLS INTERFACE axis port=cmplx_out
#pragma HLS INTERFACE s_axilite port=sizeX
#pragma HLS INTERFACE s_axilite port=sizeY
#pragma HLS INTERFACE s_axilite port=return
#pragma HLS DATAFLOW

    int total_size = sizeX * sizeY;
    
    // 内部流 (使用定点类型)
    hls::stream<cmpxFixedIn> fft_in("fft_in");
    hls::stream<cmpxFixedOut> fft_out("fft_out");
#pragma HLS STREAM depth=1024 variable=fft_in
#pragma HLS STREAM depth=1024 variable=fft_out

    bool status = false;

    // Step 1: 实数浮点转定点复数 (虚部置0)
    real_to_complex_fixed(real_in, fft_in, total_size);

    // Step 2: FFT核心计算 (使用新的fft_top + 固定缩放)
    // natural_order输出，无需手动重组
    fft_top(0, SCALING_FFT, fft_in, fft_out, &status);

    // Step 3: 定点复数转浮点复数
    fixed_to_complex_float(fft_out, cmplx_out, total_size);
}