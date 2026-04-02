/*
 * K-Litho HLS Simplified FFT Module
 * 简化版FFT模块 - 使用定点数 (hls::fft原生支持)
 * 
 * 重要修复：使用定点数而非浮点数，参考interface_stream实现
 * FFT IP核原生支持定点数 (ap_fixed)，浮点支持有限
 */

#include "../include/hls_types.h"
#include <hls_stream.h>

// ============================================================
// FFT IP核封装 (使用定点数 + scaled缩放模式)
// scaled模式: scaling_schedule参数控制每级缩放
// scaling_schedule=0: 无缩放 (可能溢出，但用于测试精度)
// ============================================================

void fft_ip_core(
    ap_uint<1> dir,
    ap_uint<15> scaling_schedule,  // 缩放调度参数 (scaled模式)
    hls::stream<cmpxFixed> &xn,
    hls::stream<cmpxFixed> &xk,
    bool* status
) {
#pragma HLS interface ap_fifo depth=1 port=status
#pragma HLS interface ap_fifo depth=1024 port=xn,xk
#pragma HLS stream variable=xn
#pragma HLS stream variable=xk

    // scaled模式调用FFT IP核
    // scaling_schedule: 每两位控制一级FFT的缩放 (0=无缩放, 1=缩放1bit, 2=缩放2bit, 3=不缩放)
    // nfft=-1: 使用默认FFT_NFFT_MAX (10, 即1024点)
    hls::fft<fft_config_t>(xn, xk, dir, scaling_schedule, -1, status);
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

    // Stage 2: 正向FFT (scaled模式, 无缩放测试)
    bool status_fft;
    fft_ip_core(0, 0, fft_in, fft_out, &status_fft);  // scaling_schedule=0

    // Stage 3: 逆向IFFT (scaled模式, 无缩放测试)
    bool status_ifft;
    fft_ip_core(1, 0, fft_out, ifft_out, &status_ifft);  // scaling_schedule=0

    // Stage 4: 定点转浮点并正确缩放
    // scaled模式无缩放时: FFT->IFFT输出 = 输入 × N
    // 需要除以N恢复正确幅度
    float scale = 1.0f / (float)total_size;
    for (int i = 0; i < total_size; i++) {
#pragma HLS LOOP_TRIPCOUNT min=1024 max=1024 avg=1024
        cmpxFixed val = ifft_out.read();
        data_out.write((float)val.real() * scale);
    }
}