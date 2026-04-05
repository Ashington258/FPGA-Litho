/*
 * FPGA-Litho HLS Top Module Header
 * 光刻模拟顶层集成模块头文件
 * 
 * 时钟约束: 5ns (200MHz)
 * - calcImage: II=4 @ 200MHz (Fmax: 273MHz verified)
 * 
 * @author FPGA-Litho Team
 * @date 2026-04-02 (Updated for 200MHz integration)
 */

#ifndef HLS_TOP_H
#define HLS_TOP_H

#include <hls_stream.h>
#include <complex>
#include "hls_calc_image_integrated.h"

// 类型定义 (从 hls_types.h 引入)
typedef float realFloat;
typedef std::complex<float> cmpxFloat;

/**
 * @brief FFT R2C完整流程 (包含移位)
 */
void fft_r2c_pipeline(
    hls::stream<realFloat> &data_in,
    hls::stream<cmpxFloat> &data_out,
    int sizeX,
    int sizeY
);

/**
 * @brief IFFT C2R完整流程 (包含移位)
 */
void fft_c2r_pipeline(
    hls::stream<cmpxFloat> &data_in,
    hls::stream<realFloat> &data_out,
    int sizeX,
    int sizeY
);

/**
 * @brief 复数乘累加
 */
void complex_mac_accumulate(
    hls::stream<cmpxFloat> &a,
    hls::stream<cmpxFloat> &b,
    cmpxFloat *result,
    int size
);

/**
 * @brief 简化版光学图像频域计算 (AXI-Stream版本)
 */
void calc_image_simple(
    hls::stream<cmpxFloat> &mask_fft,
    hls::stream<cmpxFloat> &tcc,
    hls::stream<cmpxFloat> &imgf_out,
    int sizeX,
    int sizeY
);

/**
 * @brief calcImage集成版本 (AXI-Master接口)
 * 使用验证的200MHz calcImage kernel
 */
void calc_image_integrated_wrapper(
    cmpxFloat msk[CI_MAX_LX * CI_MAX_LY],
    cmpxFloat tcc[CI_TCC_TOTAL],
    cmpxFloat imgf[CI_MAX_LX * CI_MAX_LY],
    int Lx,
    int Ly,
    int Nx,
    int Ny
);

/**
 * @brief FPGA-Litho 顶层集成模块
 * 
 * @param source_in   光源数据输入
 * @param mask_in     掩模数据输入
 * @param image_out   输出光学图像
 * @param lambda      波长 (nm)
 * @param NA          数值孔径
 * @param defocus     离焦量 (nm)
 * @param sizeX       X方向尺寸
 * @param sizeY       Y方向尺寸
 * @param mode        运行模式 (0=FFT测试, 1=TCC, 2=SOCS)
 */
void hls_top(
    hls::stream<realFloat> &source_in,
    hls::stream<realFloat> &mask_in,
    hls::stream<realFloat> &image_out,
    float lambda,
    float NA,
    float defocus,
    int sizeX,
    int sizeY,
    int mode
);

/**
 * @brief 简化顶层接口 - 仅FFT/IFFT流程
 */
void hls_top_simple(
    hls::stream<realFloat> &data_in,
    hls::stream<realFloat> &data_out,
    int sizeX,
    int sizeY
);

#endif // HLS_TOP_H