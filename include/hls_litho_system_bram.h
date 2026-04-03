/**
 * @file hls_litho_system_bram.h
 * @brief K-Litho BRAM Interface Header (无DDR板卡版本)
 * 
 * BRAM存储接口架构 - 支持TCC和SOCS两种工作模式
 * 
 * 存储方案:
 * - 本地BRAM存储: ~115KB (65块18Kb BRAM)
 * - AXI-Lite控制接口: 参数配置和数据访问
 * - TCC模式限制: Nx≤3 (BRAM容量限制)
 * - SOCS模式支持: 完整8核计算
 * 
 * 接口设计:
 * - 数据加载: load_xxx_data(idx, val) 单元素加载
 * - 批量加载: load_xxx_batch(data[N]) 批量加载
 * - 计算控制: start_litho_compute(mode, params)
 * - 结果读取: read_xxx_data(idx) 单元素读取
 * 
 * 地址映射: 参考 doc/BRAM_INTERFACE_MAPPING.md
 * 
 * @author K-Litho Team
 * @date 2026-04-03
 */

#ifndef HLS_LITHO_SYSTEM_BRAM_H
#define HLS_LITHO_SYSTEM_BRAM_H

#include <hls_stream.h>
#include <complex>
#include "hls_types.h"
#include "hls_tcc.h"
#include "hls_socs.h"
#include "hls_calc_image_integrated.h"

//=============================================================================
// BRAM System Configuration Constants
//=============================================================================

// BRAM存储尺寸限制 (与地址映射文档一致)
constexpr int BRAM_MAX_LX = 64;           // 最大频域X尺寸
constexpr int BRAM_MAX_LY = 64;           // 最大频域Y尺寸
constexpr int BRAM_MAX_NX_TCC = 3;        // TCC模式最大Nx (BRAM容量限制)
constexpr int BRAM_MAX_NX_SOCS = 15;      // SOCS模式最大Nx
constexpr int BRAM_MAX_NY = 15;           // 最大Ny
constexpr int BRAM_MAX_KERNELS = 8;       // 最大SOCS核数量
constexpr int BRAM_MAX_SRC_SIZE = 64;     // 最大光源尺寸

// BRAM存储数组尺寸
constexpr int BRAM_SOURCE_SIZE = BRAM_MAX_LX * BRAM_MAX_LY;           // 4096
constexpr int BRAM_MASK_SIZE = BRAM_MAX_LX * BRAM_MAX_LY;             // 4096
constexpr int BRAM_TCC_SIZE = (2*BRAM_MAX_NX_TCC+1) * (2*BRAM_MAX_NX_TCC+1);  // 49 (Nx=3)
constexpr int BRAM_KERNELS_SIZE = BRAM_MAX_KERNELS * 225;             // 1800
constexpr int BRAM_SCALES_SIZE = BRAM_MAX_KERNELS;                    // 8
constexpr int BRAM_IMGF_SIZE = BRAM_MAX_LX * BRAM_MAX_LY;             // 4096
constexpr int BRAM_IMG_OUT_SIZE = (4*BRAM_MAX_NX_SOCS+1) * (4*BRAM_MAX_NY+1); // 841

// BRAM块数估算 (用于资源验证)
// source: 32KB = 18块, mask: 32KB = 18块, tcc: 2KB = 1块
// kernels: 14KB = 8块, imgf: 32KB = 18块, img_out: 3KB = 2块
// 总计: 65块 + scales(寄存器) ≈ 65块

//=============================================================================
// BRAM Storage Arrays Declaration
//=============================================================================

// BRAM存储数组 (在实现文件中定义)
extern cmpxFloat source_bram[BRAM_SOURCE_SIZE];
extern cmpxFloat mask_bram[BRAM_MASK_SIZE];
extern cmpxFloat tcc_bram[BRAM_TCC_SIZE];
extern cmpxFloat kernels_bram[BRAM_KERNELS_SIZE];
extern float scales_bram[BRAM_SCALES_SIZE];
extern cmpxFloat imgf_bram[BRAM_IMGF_SIZE];
extern float img_out_bram[BRAM_IMG_OUT_SIZE];

// 状态寄存器
extern volatile int compute_status;  // 0=idle, 1=running, 2=done, 3=error

//=============================================================================
// BRAM Data Loading Interfaces (数据加载接口)
//=============================================================================

/**
 * @brief 加载光源数据 (单个复数)
 * @param idx 数组索引 [0, 4095]
 * @param val 复数值
 */
void load_source_data(int idx, cmpxFloat val);

/**
 * @brief 加载掩模频谱数据 (单个复数)
 * @param idx 数组索引 [0, 4095]
 * @param val 复数值
 */
void load_mask_data(int idx, cmpxFloat val);

/**
 * @brief 加载TCC矩阵数据 (单个复数)
 * @param idx 数组索引 [0, 48] (Nx=3时)
 * @param val 复数值
 */
void load_tcc_data(int idx, cmpxFloat val);

/**
 * @brief 加载SOCS核数据 (单个复数)
 * @param idx 数组索引 [0, 1799] (8核)
 * @param val 复数值
 */
void load_kernels_data(int idx, cmpxFloat val);

/**
 * @brief 加载SOCS权重数据 (单个浮点数)
 * @param idx 数组索引 [0, 7]
 * @param val 浮点数值
 */
void load_scales_data(int idx, float val);

//=============================================================================
// BRAM Data Reading Interfaces (数据读取接口)
//=============================================================================

/**
 * @brief 读取频域输出数据 (单个复数)
 * @param idx 数组索引 [0, 4095]
 * @return 复数值
 */
cmpxFloat read_imgf_data(int idx);

/**
 * @brief 读取空间域输出数据 (单个浮点数)
 * @param idx 数组索引 [0, 840]
 * @return 浮点数值
 */
float read_img_out_data(int idx);

//=============================================================================
// Batch Loading Interfaces (批量加载接口 - 可选)
//=============================================================================

/**
 * @brief 批量加载光源数据
 * @param data 复数数组指针 (size=BRAM_SOURCE_SIZE)
 */
void load_source_batch(cmpxFloat data[BRAM_SOURCE_SIZE]);

/**
 * @brief 批量加载掩模频谱数据
 * @param data 复数数组指针 (size=BRAM_MASK_SIZE)
 */
void load_mask_batch(cmpxFloat data[BRAM_MASK_SIZE]);

/**
 * @brief 批量加载SOCS核数据
 * @param data 复数数组指针 (size=BRAM_KERNELS_SIZE)
 */
void load_kernels_batch(cmpxFloat data[BRAM_KERNELS_SIZE]);

/**
 * @brief 批量加载SOCS权重数据
 * @param data 浮点数组指针 (size=BRAM_SCALES_SIZE)
 */
void load_scales_batch(float data[BRAM_SCALES_SIZE]);

//=============================================================================
// Compute Control Interfaces (计算控制接口)
//=============================================================================

/**
 * @brief 启动Litho计算
 * @param mode 工作模式: 1=TCC, 2=SOCS
 * @param Lx 频域X尺寸 [1, 64]
 * @param Ly 频域Y尺寸 [1, 64]
 * @param Nx TCC/SOCS半宽 [1, 3] for TCC, [1, 15] for SOCS
 * @param Ny TCC/SOCS半高 [1, 15]
 * @param srcSize 光源尺寸 [32, 64] (TCC模式)
 * @param nkernels SOCS核数量 [1, 8] (SOCS模式)
 */
void start_litho_compute(
    int mode,
    int Lx, int Ly,
    int Nx, int Ny,
    int srcSize,
    int nkernels
);

/**
 * @brief 获取计算状态
 * @return 状态码: 0=idle, 1=running, 2=done, 3=error
 */
int get_compute_status();

/**
 * @brief 重置BRAM存储
 */
void reset_bram_storage();

//=============================================================================
// Internal Compute Functions (内部计算函数)
//=============================================================================

/**
 * @brief TCC模式计算 (BRAM版本)
 * @internal 内部函数，由start_litho_compute调用
 */
void hls_litho_tcc_mode_bram(
    cmpxFloat source[BRAM_SOURCE_SIZE],
    cmpxFloat mask_fft[BRAM_MASK_SIZE],
    cmpxFloat tcc[BRAM_TCC_SIZE],
    cmpxFloat imgf[BRAM_IMGF_SIZE],
    int Lx, int Ly,
    int Nx, int Ny,
    int srcSize
);

/**
 * @brief SOCS模式计算 (BRAM版本)
 * @internal 内部函数，由start_litho_compute调用
 */
void hls_litho_socs_mode_bram(
    cmpxFloat kernels[BRAM_KERNELS_SIZE],
    float scales[BRAM_SCALES_SIZE],
    cmpxFloat mask_fft[BRAM_MASK_SIZE],
    float img_out[BRAM_IMG_OUT_SIZE],
    int Lx, int Ly,
    int Nx, int Ny,
    int nkernels
);

//=============================================================================
// Top-Level Function (顶层接口)
//=============================================================================

/**
 * @brief Litho系统顶层函数 (BRAM版本)
 * 
 * 功能: 根据mode参数执行TCC或SOCS模式计算
 * 
 * 接口设计:
 * - 所有数据通过BRAM存储数组传递
 * - 控制参数通过AXI-Lite接口传入
 * - 计算结果存储在imgf_bram或img_out_bram
 * 
 * 使用流程:
 * 1. 调用load_xxx_data()加载数据
 * 2. 调用start_litho_compute()启动计算
 * 3. 轮询get_compute_status()等待完成
 * 4. 调用read_xxx_data()读取结果
 * 
 * @param mode 工作模式: 1=TCC, 2=SOCS
 * @param Lx 频域X尺寸
 * @param Ly 频域Y尺寸
 * @param Nx TCC/SOCS半宽
 * @param Ny TCC/SOCS半高
 * @param srcSize 光源尺寸
 * @param nkernels SOCS核数量
 */
void hls_litho_system_bram(
    int mode,
    int Lx, int Ly,
    int Nx, int Ny,
    int srcSize,
    int nkernels
);

#endif // HLS_LITHO_SYSTEM_BRAM_H