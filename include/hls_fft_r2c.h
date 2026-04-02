/*
 * K-Litho HLS FFT R2C Header
 * FFT实数到复数变换头文件
 */

#ifndef HLS_FFT_R2C_H
#define HLS_FFT_R2C_H

#include <hls_stream.h>
#include <ap_fixed.h>
#include <complex>

// 类型定义 (从 hls_types.h 引入)
typedef float realFloat;
typedef std::complex<float> cmpxFloat;

/**
 * @brief FFT R2C 核心变换
 */
void hls_fft_r2c_core(
    ap_uint<1> direction,
    ap_uint<15> length,
    hls::stream<cmpxFloat> &xn_in,
    hls::stream<cmpxFloat> &xk_out,
    bool *ovflo
);

/**
 * @brief 实数转复数流
 */
void real_to_complex(
    hls::stream<realFloat> &real_in,
    hls::stream<cmpxFloat> &cmplx_out,
    int total_size
);

/**
 * @brief FFT输出重排序
 */
void fft_output_reorder(
    hls::stream<cmpxFloat> &fft_out,
    hls::stream<cmpxFloat> &reordered_out,
    int sizeX,
    int sizeY
);

/**
 * @brief FFT R2C 完整流程
 */
void hls_fft_r2c(
    hls::stream<realFloat> &data_in,
    hls::stream<cmpxFloat> &data_out,
    int sizeX,
    int sizeY
);

#endif // HLS_FFT_R2C_H