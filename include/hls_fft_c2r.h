/*
 * K-Litho HLS FFT C2R Header
 * FFT复数到实数变换头文件
 */

#ifndef HLS_FFT_C2R_H
#define HLS_FFT_C2R_H

#include <hls_stream.h>
#include <ap_fixed.h>
#include <complex>

// 类型定义 (从 hls_types.h 引入)
typedef float realFloat;
typedef std::complex<float> cmpxFloat;

/**
 * @brief FFT输入重排序
 */
void fft_input_reorder(
    hls::stream<cmpxFloat> &data_in,
    hls::stream<cmpxFloat> &reordered_out,
    int sizeX,
    int sizeY
);

/**
 * @brief FFT C2R 核心变换
 */
void hls_fft_c2r_core(
    ap_uint<1> direction,
    ap_uint<15> length,
    hls::stream<cmpxFloat> &xn_in,
    hls::stream<cmpxFloat> &xk_out,
    bool *ovflo
);

/**
 * @brief 复数转实数流
 */
void complex_to_real(
    hls::stream<cmpxFloat> &cmplx_in,
    hls::stream<realFloat> &real_out,
    int total_size
);

/**
 * @brief FFT C2R 完整流程
 */
void hls_fft_c2r(
    hls::stream<cmpxFloat> &data_in,
    hls::stream<realFloat> &data_out,
    int sizeX,
    int sizeY
);

#endif // HLS_FFT_C2R_H