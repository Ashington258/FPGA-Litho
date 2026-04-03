#ifndef HLS_SOCS_H
#define HLS_SOCS_H

/**
 * @file hls_socs.h
 * @brief SOCS (Sum of Coherent Sources) 光学图像计算模块
 * 
 * 使用SOCS核快速计算光学图像，基于部分相干光源分解理论
 * 
 * 算法流程:
 * 1. 多核循环: nk个SOCS核
 * 2. Kernel-Mask复数乘法
 * 3. IFFT变换到空间域
 * 4. 平方累加: scales[k] * (real² + imag²)
 * 5. 循环移位 + 傅里叶插值
 */

#include <hls_stream.h>
#include <ap_fixed.h>
#include <complex>
#include "hls_types.h"

// SOCS配置常量
constexpr int SOCS_MAX_KERNELS = 8;      // 最大SOCS核数量
constexpr int SOCS_MAX_NX = 7;           // 最大Nx参数
constexpr int SOCS_MAX_NY = 7;           // 最大Ny参数
constexpr int SOCS_MAX_LX = 64;          // 最大Lx参数
constexpr int SOCS_MAX_LY = 64;          // 最大Ly参数

// 计算派生尺寸
constexpr int SOCS_KERNEL_SIZE = (2 * SOCS_MAX_NX + 1) * (2 * SOCS_MAX_NY + 1);  // 单核大小
constexpr int SOCS_OUTPUT_SIZE = (4 * SOCS_MAX_NX + 1) * (4 * SOCS_MAX_NY + 1);  // IFFT输出大小

/**
 * @brief Kernel-Mask复数乘法模块
 * 
 * 计算SOCS核与掩模频域数据的复数乘积
 * 
 * @param kernel  单个SOCS核数据 (2Nx+1) × (2Ny+1)
 * @param mask    掩模频域数据 (Lx × Ly)
 * @param product 乘积结果 (sizeX × sizeY)
 * @param Lx, Ly  掩模尺寸
 * @param Nx, Ny  核尺寸参数
 */
void hls_kernel_mask_multiply(
    hls::stream<complex_float> &kernel,
    hls::stream<complex_float> &mask,
    hls::stream<complex_float> &product,
    int Lx, int Ly,
    int Nx, int Ny
);

/**
 * @brief 平方累加模块
 * 
 * 计算 scales[k] * (real² + imag²) 并累加到输出图像
 * 
 * @param input   IFFT输出复数数据流
 * @param scale   当前核的权重系数
 * @param accum   累加结果数组 (平方幅度)
 * @param sizeX, sizeY 数据尺寸
 */
void hls_square_accumulate(
    hls::stream<complex_float> &input,
    float scale,
    float accum[],
    int sizeX,
    int sizeY
);

/**
 * @brief SOCS图像计算核心模块
 * 
 * 使用多个SOCS核计算光学图像
 * 
 * @param kernels    所有SOCS核数据 (nk个核)
 * @param scales     各核的权重系数
 * @param mask_fft   掩模频域数据
 * @param image_out  输出光学图像
 * @param nk         核数量
 * @param Lx, Ly     输出图像尺寸
 * @param Nx, Ny     核尺寸参数
 */
void hls_calc_socs_core(
    complex_float kernels[SOCS_MAX_KERNELS][SOCS_KERNEL_SIZE],
    float scales[SOCS_MAX_KERNELS],
    complex_float mask_fft[SOCS_MAX_LX * SOCS_MAX_LY],
    float image_out[SOCS_MAX_LX * SOCS_MAX_LY],
    int nk,
    int Lx, int Ly,
    int Nx, int Ny
);

/**
 * @brief SOCS顶层接口 (AXI-Master版本)
 * 
 * AXI-Master接口用于大数据传输
 * 
 * @param gmem_krn   SOCS核数据内存端口
 * @param gmem_scl   权重系数内存端口
 * @param gmem_msk   掩模频域数据内存端口
 * @param gmem_img   输出图像内存端口
 * @param nk         核数量
 * @param Lx, Ly     输出图像尺寸
 * @param Nx, Ny     核尺寸参数
 */
void hls_calc_socs(
    complex_float *gmem_krn,    // AXI-Master: kernels[nk * kernelSize]
    float *gmem_scl,            // AXI-Master: scales[nk]
    complex_float *gmem_msk,    // AXI-Master: mask_fft[Lx * Ly]
    float *gmem_img,            // AXI-Master: image_out[Lx * Ly]
    int nk,
    int Lx, int Ly,
    int Nx, int Ny
);

#endif // HLS_SOCS_H