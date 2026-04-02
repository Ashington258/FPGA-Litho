/*
 * K-Litho HLS FFT C2R Module
 * 复数到实数IFFT变换
 * 
 * 替代原始函数: klitho_tcc.cpp:FT_c2r()
 */

#include "../include/hls_types.h"
#include <hls_stream.h>

using namespace hls;

// ============================================================
// FFT C2R 核心模块
// ============================================================

/**
 * @brief FFT C2R预处理模块
 * 将复数频域数据重组为FFT核所需格式
 * 
 * @param cmplx_in 复数输入流 (频域数据)
 * @param cmplx_out 重组后的数据流 (送入IFFT核)
 * @param sizeX    X方向尺寸
 * @param sizeY    Y方向尺寸
 */
void fft_input_reorder(
    hls::stream<cmpxFloat> &cmplx_in,
    hls::stream<cmpxFloat> &cmplx_out,
    int sizeX,
    int sizeY
) {
#pragma HLS INTERFACE axis port=cmplx_in
#pragma HLS INTERFACE axis port=cmplx_out
#pragma HLS PIPELINE II=1

    int wt = (sizeX / 2) + 1;
    int xh = (sizeX + 1) / 2;
    int yh = (sizeY + 1) / 2;

    // 临时存储
    cmpxFloat freq_data[sizeX * sizeY];
#pragma HLS ARRAY_PARTITION variable=freq_data cyclic factor=4

    // 读取输入频域数据
    for (int i = 0; i < sizeX * sizeY; i++) {
#pragma HLS LOOP_TRIPCOUNT min=1024 max=1024 avg=1024
#pragma HLS PIPELINE II=1
        freq_data[i] = cmplx_in.read();
    }

    // 重组: 将频域数据转换为FFT所需的r2c格式
    // FFT r2c输出只包含正频率, 输入时需要对应处理
    for (int y = 0; y < sizeY; y++) {
#pragma HLS LOOP_TRIPCOUNT min=256 max=256 avg=256
        int sy = shift_index(y, yh, sizeY);
        for (int x = xh - 1; x < sizeX; x++) {
#pragma HLS LOOP_TRIPCOUNT min=4 max=8 avg=6
#pragma HLS PIPELINE II=1
            int sx = shift_index(x, xh, sizeX);
            if (sx < wt) {
                cmplx_out.write(freq_data[sy * sizeX + sx]);
            }
        }
    }

    // 处理偶数尺寸的特殊情况
    if (sizeX % 2 == 0) {
        // 填充Nyquist频率
        for (int y = 0; y < sizeY; y++) {
#pragma HLS LOOP_TRIPCOUNT min=256 max=256 avg=256
#pragma HLS PIPELINE II=1
            int y2 = (sizeY - 1 - y) % sizeY;
            cmpxFloat nyquist = complex_conj(freq_data[y2 * sizeX]);
            cmplx_out.write(nyquist);
        }
    }
}

/**
 * @brief IFFT核心计算
 */
void hls_fft_c2r_core(
    ap_uint<1> dir,
    ap_uint<15> scaling,
    hls::stream<cmpxFloat> &xn_in,
    hls::stream<cmpxFloat> &xk_out,
    bool *status
) {
#pragma HLS INTERFACE axis port=xn_in
#pragma HLS INTERFACE axis port=xk_out
#pragma HLS INTERFACE ap_fifo depth=1 port=status
#pragma HLS PIPELINE II=1

    // 调用HLS FFT IP核 (逆向)
    hls::fft<fft_config_t>(xn_in, xk_out, dir, scaling, -1, status);
}

/**
 * @brief IFFT输出后处理
 * 从复数结果提取实数部分
 * 
 * @param cmplx_in IFFT核输出
 * @param real_out 实数输出流
 * @param size     数据尺寸
 */
void complex_to_real(
    hls::stream<cmpxFloat> &cmplx_in,
    hls::stream<realFloat> &real_out,
    int size
) {
#pragma HLS INTERFACE axis port=cmplx_in
#pragma HLS INTERFACE axis port=real_out
#pragma HLS PIPELINE II=1

    for (int i = 0; i < size; i++) {
#pragma HLS LOOP_TRIPCOUNT min=1024 max=1024 avg=1024
#pragma HLS PIPELINE II=1
        cmpxFloat val = cmplx_in.read();
        // IFFT输出应接近实数, 取实部
        real_out.write(val.real());
    }
}

// ============================================================
// 顶层FFT C2R模块
// ============================================================

/**
 * @brief FFT C2R顶层接口 (IFFT)
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
#pragma HLS INTERFACE s_axilite port=sizeX,sizeY
#pragma HLS INTERFACE s_axilite port=return
#pragma HLS DATAFLOW

    // 内部流
    hls::stream<cmpxFloat> reordered_in("reordered_in");
    hls::stream<cmpxFloat> ifft_out("ifft_out");
#pragma HLS STREAM depth=1024 variable=reordered_in
#pragma HLS STREAM depth=1024 variable=ifft_out

    bool status = false;
    ap_uint<1> dir = 1;  // 逆向IFFT
    ap_uint<15> scaling = 0;  // 缩放配置

    // Step 1: 输入重组
    fft_input_reorder(cmplx_in, reordered_in, sizeX, sizeY);

    // Step 2: IFFT核心计算
    hls_fft_c2r_core(dir, scaling, reordered_in, ifft_out, &status);

    // Step 3: 复数转实数
    complex_to_real(ifft_out, real_out, sizeX * sizeY);
}