/**
 * @file hls_litho_system.h
 * @brief FPGA-Litho Complete System Integration Header
 * 
 * 系统集成架构 - 支持TCC和SOCS两种工作模式
 * 
 * 数据流架构:
 * 
 * TCC模式 (mode=1):
 *   Source → Pupil Calc → TCC Matrix → calcImage → Output
 *   
 * SOCS模式 (mode=2):
 *   Kernels + Scales → Kernel-Mask Multiply → Square Accumulate → Shift → Output
 * 
 * 时钟约束: 5ns (200MHz)
 * - TCC模块: II=1 @ 342MHz verified
 * - calcImage: II=4 @ 273MHz verified  
 * - SOCS模块: II=1 @ 290MHz verified
 * 
 * @author FPGA-Litho Team
 * @date 2026-04-03
 */

#ifndef HLS_LITHO_SYSTEM_H
#define HLS_LITHO_SYSTEM_H

#include <hls_stream.h>
#include <complex>
#include "hls_types.h"
#include "hls_tcc.h"
#include "hls_socs.h"
#include "hls_calc_image_integrated.h"

//=============================================================================
// System Configuration Constants
//=============================================================================

// 统一尺寸参数 (与各模块兼容)
constexpr int SYS_MAX_LX = 64;           // 最大频域X尺寸
constexpr int SYS_MAX_LY = 64;           // 最大频域Y尺寸  
constexpr int SYS_MAX_NX = 7;            // 最大TCC/SOCS半宽
constexpr int SYS_MAX_NY = 7;            // 最大TCC/SOCS半高
constexpr int SYS_MAX_KERNELS = 8;       // 最大SOCS核数量
constexpr int SYS_MAX_SRC_SIZE = 64;     // 最大光源尺寸

// 派生尺寸
constexpr int SYS_TCC_DIM = (2*SYS_MAX_NX+1) * (2*SYS_MAX_NY+1);  // TCC矩阵维度
constexpr int SYS_TCC_TOTAL = SYS_TCC_DIM * SYS_TCC_DIM;          // TCC矩阵总元素
constexpr int SYS_OUTPUT_SIZE = (4*SYS_MAX_NX+1) * (4*SYS_MAX_NY+1); // SOCS输出尺寸

//=============================================================================
// System Parameter Structure
//=============================================================================

/**
 * @brief Litho系统运行参数
 */
struct LithoSystemParams {
    // 光学参数
    float lambda;       // 波长 (nm)
    float NA;           // 数值孔径
    float defocus;      // 离焦量 (nm)
    
    // 尺寸参数
    int Lx;             // 频域X尺寸
    int Ly;             // 频域Y尺寸
    int Nx;             // TCC/SOCS半宽
    int Ny;             // TCC/SOCS半高
    
    // 计算参数
    int srcSize;        // 光源尺寸 (TCC模式)
    int nkernels;       // SOCS核数量 (SOCS模式)
    
    // 控制参数
    int mode;           // 工作模式: 1=TCC, 2=SOCS
};

//=============================================================================
// Top-Level Function Declarations
//=============================================================================

/**
 * @brief Litho系统顶层集成模块
 * 
 * 支持两种工作模式的完整光刻模拟系统
 * 
 * @param source      光源数据 (AXI-Master) [TCC模式使用]
 * @param mask_fft    掩模频谱 (AXI-Master) [所有模式使用]
 * @param tcc         TCC矩阵 (AXI-Master) [TCC/calcImage模式使用]
 * @param kernels     SOCS核数据 (AXI-Master) [SOCS模式使用]
 * @param scales      SOCS权重 (AXI-Master) [SOCS模式使用]
 * @param imgf        输出图像频谱 (AXI-Master)
 * @param img_out     输出空间域图像 (AXI-Master) [SOCS模式]
 * @param params      系统参数 (AXI-Lite)
 */
void hls_litho_system(
    // AXI-Master 数据接口
    cmpxFloat source[SYS_MAX_SRC_SIZE * SYS_MAX_SRC_SIZE],
    cmpxFloat mask_fft[SYS_MAX_LX * SYS_MAX_LY],
    cmpxFloat tcc[SYS_TCC_TOTAL],
    cmpxFloat kernels[SYS_MAX_KERNELS * SYS_TCC_DIM],
    float scales[SYS_MAX_KERNELS],
    cmpxFloat imgf[SYS_MAX_LX * SYS_MAX_LY],
    float img_out[SYS_OUTPUT_SIZE],
    
    // AXI-Lite 控制接口
    float lambda,
    float NA,
    float defocus,
    int Lx,
    int Ly,
    int Nx,
    int Ny,
    int srcSize,
    int nkernels,
    int mode
);

/**
 * @brief TCC模式完整流程
 * 
 * 包含: Pupil计算 → TCC累加 → calcImage
 * 
 * @param source      光源数据
 * @param mask_fft    掩模频谱
 * @param imgf        输出图像频谱
 * @param params      系统参数
 */
void hls_litho_tcc_mode(
    cmpxFloat source[SYS_MAX_SRC_SIZE * SYS_MAX_SRC_SIZE],
    cmpxFloat mask_fft[SYS_MAX_LX * SYS_MAX_LY],
    cmpxFloat imgf[SYS_MAX_LX * SYS_MAX_LY],
    LithoSystemParams &params
);

/**
 * @brief SOCS模式完整流程
 * 
 * 包含: Kernel-Mask乘法 → IFFT → 平方累加 → 移位
 * 
 * @param kernels     SOCS核数据
 * @param scales      SOCS权重
 * @param mask_fft    掩模频谱
 * @param img_out     输出空间域图像
 * @param params      系统参数
 */
void hls_litho_socs_mode(
    cmpxFloat kernels[SYS_MAX_KERNELS * SYS_TCC_DIM],
    float scales[SYS_MAX_KERNELS],
    cmpxFloat mask_fft[SYS_MAX_LX * SYS_MAX_LY],
    float img_out[SYS_OUTPUT_SIZE],
    LithoSystemParams &params
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief 参数初始化模块
 */
void init_system_params(
    LithoSystemParams &params,
    float lambda,
    float NA,
    float defocus,
    int Lx, int Ly,
    int Nx, int Ny,
    int srcSize,
    int nkernels,
    int mode
);

/**
 * @brief 复数平方幅度计算
 */
inline float squared_magnitude(const cmpxFloat& c) {
    float r = c.real();
    float i = c.imag();
    return r * r + i * i;
}

#endif // HLS_LITHO_SYSTEM_H