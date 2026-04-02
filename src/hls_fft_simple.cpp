/*
 * K-Litho HLS Simplified FFT Module
 * 简化版FFT模块 - 使用定点数 (hls::fft原生支持)
 * 
 * 重要修复：参考Vitis官方 interface_stream 实现
 * - 使用 config1 配置结构体
 * - 固定缩放策略 SCALING_FFT = 0x1555
 * - FFT IP核原生支持定点数 (ap_fixed)
 */

#include "../include/hls_types.h"
#include <hls_stream.h>

// ============================================================
// FFT核心函数 (与官方interface_stream完全一致)
// 使用定点数 + scaled缩放模式
// ============================================================

void fft_top(
    ap_uint<1> dir,
    ap_uint<15> scaling,
    hls::stream<cmpxFixedIn> &xn,
    hls::stream<cmpxFixedOut> &xk,
    bool* status
) {
#pragma HLS interface ap_fifo depth=1 port=status
#pragma HLS interface ap_fifo depth=1024 port=xn,xk
#pragma HLS stream variable=xn
#pragma HLS stream variable=xk
#pragma HLS dataflow

    // 使用 config1 配置结构体调用 FFT IP核
    // scaling: 每两位控制一级FFT缩放
    // nfft=-1: 使用默认 FFT_NFFT_MAX (10, 即1024点)
    hls::fft<config1>(xn, xk, dir, scaling, -1, status);
}

// ============================================================
// 顶层FFT测试模块 (浮点接口，内部定点处理)
// ============================================================

void hls_top_simple(
    hls::stream<realFloat> &data_in,
    hls::stream<realFloat> &data_out,
    int sizeX,
    int sizeY
) {
#pragma HLS INTERFACE axis port=data_in
#pragma HLS INTERFACE axis port=data_out
#pragma HLS INTERFACE s_axilite port=sizeX
#pragma HLS INTERFACE s_axilite port=sizeY
#pragma HLS INTERFACE s_axilite port=return
#pragma HLS DATAFLOW

    int total_size = sizeX * sizeY;
    
    // 内部定点数据流
    hls::stream<cmpxFixed> fft_in("fft_in");
    hls::stream<cmpxFixed> fft_out("fft_out");
    hls::stream<cmpxFixed> ifft_out("ifft_out");
#pragma HLS STREAM depth=1024 variable=fft_in
#pragma HLS STREAM depth=1024 variable=fft_out
#pragma HLS STREAM depth=1024 variable=ifft_out

    // Stage 1: 浮点转定点 (实部为输入值，虚部为0)
    // ap_fixed<32,1> 范围: [-1, ~1) 适合归一化数据
    for (int i = 0; i < total_size; i++) {
#pragma HLS LOOP_TRIPCOUNT min=1024 max=1024 avg=1024
        realFloat val = data_in.read();
        fft_in.write(cmpxFixed(fft_data_t(val), fft_data_t(0)));
    }

    // Stage 2: 正向FFT (scaled模式, 固定缩放策略)
    bool status_fft;
    fft_top(0, SCALING_FFT, fft_in, fft_out, &status_fft);

    // Stage 3: 逆向IFFT (scaled模式, 固定缩放策略)
    bool status_ifft;
    fft_top(1, SCALING_IFFT, fft_out, ifft_out, &status_ifft);

    // Stage 4: 定点转浮点输出
    // scaled模式: FFT每级缩放1bit (总缩放1024), IFFT每级缩放1bit (总缩放1024)
    // FFT->IFFT: 幅度自动恢复 (1024 × 1024 = 完整恢复)
    // 注意: 使用固定缩放策略时，输出幅度与输入一致，无需额外归一化
    for (int i = 0; i < total_size; i++) {
#pragma HLS LOOP_TRIPCOUNT min=1024 max=1024 avg=1024
        cmpxFixedOut val = ifft_out.read();
        data_out.write(val.real().to_float());
    }
}