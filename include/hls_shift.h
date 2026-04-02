/*
 * K-Litho HLS Shift Header
 * 2D循环移位头文件
 */

#ifndef HLS_SHIFT_H
#define HLS_SHIFT_H

#include <hls_stream.h>
#include <complex>

// 类型定义 (从 hls_types.h 引入)
typedef float realFloat;
typedef std::complex<float> cmpxFloat;

/**
 * @brief 实数数据2D循环移位
 */
void hls_shift_real(
    hls::stream<realFloat> &data_in,
    hls::stream<realFloat> &data_out,
    int sizeX,
    int sizeY,
    bool shift_x,
    bool shift_y
);

/**
 * @brief 复数数据2D循环移位
 */
void hls_shift_complex(
    hls::stream<cmpxFloat> &data_in,
    hls::stream<cmpxFloat> &data_out,
    int sizeX,
    int sizeY,
    bool shift_x,
    bool shift_y
);

/**
 * @brief 实数数据逆移位 (从角落移回中心)
 */
void hls_shift_inverse_real(
    hls::stream<realFloat> &data_in,
    hls::stream<realFloat> &data_out,
    int sizeX,
    int sizeY
);

/**
 * @brief 复数数据逆移位 (从角落移回中心)
 */
void hls_shift_inverse_complex(
    hls::stream<cmpxFloat> &data_in,
    hls::stream<cmpxFloat> &data_out,
    int sizeX,
    int sizeY
);

#endif // HLS_SHIFT_H