/**
 * @file hls_litho_system_bram.h
 * @brief K-Litho BRAM Single-Function Interface (Phase 6C Refactor)
 * 
 * 单函数架构 - 所有BRAM操作通过operation参数控制
 * 
 * Operation编码:
 * 0=load_source, 1=load_mask, 2=load_tcc, 3=load_kernels, 4=load_scales,
 * 5=compute_tcc, 6=compute_socs, 7=read_imgf, 8=read_img_out, 9=reset
 * 
 * BRAM存储:
 * - source_bram: 4096 x 64bit = 16 BRAM_18K
 * - mask_bram: 4096 x 64bit = 16 BRAM_18K
 * - tcc_bram: 49 x 64bit = 1 BRAM_18K
 * - kernels_bram: 1800 x 64bit = 8 BRAM_18K
 * - scales_bram: 8 x 32bit = 0 BRAM (registers)
 * - imgf_bram: 4096 x 64bit = 16 BRAM_18K
 * - img_out_bram: 4096 x 32bit = 8 BRAM_18K
 * - Total: ~57 BRAM_18K blocks
 * 
 * @author K-Litho Team
 * @date 2026-04-04
 */

#ifndef HLS_LITHO_SYSTEM_BRAM_H
#define HLS_LITHO_SYSTEM_BRAM_H

#include <complex>
#include "hls_types.h"

//=============================================================================
// BRAM System Configuration Constants
//=============================================================================

// BRAM存储尺寸限制
// 注意: 这些值影响寄存器位宽，设置过小会导致寄存器被优化截断
constexpr int BRAM_MAX_LX = 64;           // 最大频域X尺寸
constexpr int BRAM_MAX_LY = 64;           // 最大频域Y尺寸
constexpr int BRAM_MAX_NX_TCC = 15;       // TCC模式最大Nx (增大以支持更大范围)
constexpr int BRAM_MAX_NX_SOCS = 15;      // SOCS模式最大Nx
constexpr int BRAM_MAX_NY = 15;           // 最大Ny
constexpr int BRAM_MAX_KERNELS = 8;       // 最大SOCS核数量
constexpr int BRAM_MAX_SRC_SIZE = 256;    // 最大光源尺寸 (增大)

// BRAM存储数组尺寸
constexpr int BRAM_SOURCE_SIZE = BRAM_MAX_LX * BRAM_MAX_LY;           // 4096
constexpr int BRAM_MASK_SIZE = BRAM_MAX_LX * BRAM_MAX_LY;             // 4096
constexpr int BRAM_TCC_SIZE = (2*BRAM_MAX_NX_TCC+1) * (2*BRAM_MAX_NX_TCC+1);  // 49
constexpr int BRAM_KERNELS_SIZE = BRAM_MAX_KERNELS * 225;             // 1800 (8 x 15x15)
constexpr int BRAM_SCALES_SIZE = BRAM_MAX_KERNELS;                    // 8
constexpr int BRAM_IMGF_SIZE = BRAM_MAX_LX * BRAM_MAX_LY;             // 4096
constexpr int BRAM_IMG_OUT_SIZE = BRAM_MAX_LX * BRAM_MAX_LY;          // 4096

//=============================================================================
// Operation Codes
//=============================================================================

// 数据加载操作 (0-4)
constexpr int OP_LOAD_SOURCE    = 0;      // 加载光源数据
constexpr int OP_LOAD_MASK      = 1;      // 加载mask数据
constexpr int OP_LOAD_TCC       = 2;      // 加载TCC矩阵
constexpr int OP_LOAD_KERNELS   = 3;      // 加载SOCS kernels
constexpr int OP_LOAD_SCALES    = 4;      // 加载SOCS scales

// 计算操作 (5-6)
constexpr int OP_COMPUTE_TCC    = 5;      // TCC模式计算
constexpr int OP_COMPUTE_SOCS   = 6;      // SOCS模式计算

// 数据读取操作 (7-8)
constexpr int OP_READ_IMGF      = 7;      // 读取imgf结果
constexpr int OP_READ_IMG_OUT   = 8;      // 读取img_out结果

// 系统操作 (9)
constexpr int OP_RESET          = 9;      // 重置所有BRAM存储

//=============================================================================
// Top-Level Function Declaration
//=============================================================================

/**
 * @brief BRAM系统顶层函数 - 单函数架构
 * 
 * @param operation 操作码 (0-9)
 * @param idx 数组索引 (用于load/read操作)
 * @param val 数据值 (用于load操作，complex float)
 * @param mode 计算模式 (1=TCC, 2=SOCS，仅用于compute操作)
 * @param Lx 频域X尺寸
 * @param Ly 频域Y尺寸
 * @param Nx TCC/SOCS参数
 * @param Ny TCC/SOCS参数
 * @param srcSize 光源尺寸 (仅TCC模式)
 * @param nkernels SOCS核数量 (仅SOCS模式)
 * @return cmpxFloat 读取的数据值或状态指示
 * 
 * 返回值含义:
 * - load操作: 返回无关值
 * - compute操作: 1.0表示成功，-1.0表示参数错误
 * - read操作: 返回指定索引的数据
 * - reset操作: 1.0表示成功
 */
cmpxFloat hls_litho_system_bram(
    int operation,
    int idx,
    cmpxFloat val,
    int mode,
    int Lx, int Ly,
    int Nx, int Ny,
    int srcSize,
    int nkernels
);

#endif // HLS_LITHO_SYSTEM_BRAM_H