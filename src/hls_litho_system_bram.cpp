/**
 * @file hls_litho_system_bram.cpp
 * @brief K-Litho BRAM Interface Implementation (无DDR板卡版本)
 * 
 * 实现特性:
 * - 本地BRAM存储: ~115KB (65块18Kb BRAM)
 * - AXI-Lite控制接口: 所有函数接口
 * - 参数验证: 尺寸和边界检查
 * - 状态管理: idle/running/done/error
 * 
 * 设计参考: doc/BRAM_INTERFACE_MAPPING.md
 * 
 * @author K-Litho Team
 * @date 2026-04-03
 */

#include "../include/hls_litho_system_bram.h"
#include <hls_math.h>

//=============================================================================
// BRAM Storage Arrays Definition
//=============================================================================

// BRAM存储数组定义
cmpxFloat source_bram[BRAM_SOURCE_SIZE];
cmpxFloat mask_bram[BRAM_MASK_SIZE];
cmpxFloat tcc_bram[BRAM_TCC_SIZE];
cmpxFloat kernels_bram[BRAM_KERNELS_SIZE];
float scales_bram[BRAM_SCALES_SIZE];
cmpxFloat imgf_bram[BRAM_IMGF_SIZE];
float img_out_bram[BRAM_IMG_OUT_SIZE];

// 状态寄存器
volatile int compute_status = 0;  // 0=idle, 1=running, 2=done, 3=error

//=============================================================================
// Data Loading Functions Implementation
//=============================================================================

void load_source_data(int idx, cmpxFloat val) {
#pragma HLS INLINE
#pragma HLS INTERFACE s_axilite port=idx bundle=control
#pragma HLS INTERFACE s_axilite port=val bundle=control
    
    // 边界检查
    if (idx >= 0 && idx < BRAM_SOURCE_SIZE) {
        source_bram[idx] = val;
    } else {
        compute_status = 3;  // error
    }
}

void load_mask_data(int idx, cmpxFloat val) {
#pragma HLS INLINE
#pragma HLS INTERFACE s_axilite port=idx bundle=control
#pragma HLS INTERFACE s_axilite port=val bundle=control
    
    if (idx >= 0 && idx < BRAM_MASK_SIZE) {
        mask_bram[idx] = val;
    } else {
        compute_status = 3;
    }
}

void load_tcc_data(int idx, cmpxFloat val) {
#pragma HLS INLINE
#pragma HLS INTERFACE s_axilite port=idx bundle=control
#pragma HLS INTERFACE s_axilite port=val bundle=control
    
    if (idx >= 0 && idx < BRAM_TCC_SIZE) {
        tcc_bram[idx] = val;
    } else {
        compute_status = 3;
    }
}

void load_kernels_data(int idx, cmpxFloat val) {
#pragma HLS INLINE
#pragma HLS INTERFACE s_axilite port=idx bundle=control
#pragma HLS INTERFACE s_axilite port=val bundle=control
    
    if (idx >= 0 && idx < BRAM_KERNELS_SIZE) {
        kernels_bram[idx] = val;
    } else {
        compute_status = 3;
    }
}

void load_scales_data(int idx, float val) {
#pragma HLS INLINE
#pragma HLS INTERFACE s_axilite port=idx bundle=control
#pragma HLS INTERFACE s_axilite port=val bundle=control
    
    if (idx >= 0 && idx < BRAM_SCALES_SIZE) {
        scales_bram[idx] = val;
    } else {
        compute_status = 3;
    }
}

//=============================================================================
// Data Reading Functions Implementation
//=============================================================================

cmpxFloat read_imgf_data(int idx) {
#pragma HLS INLINE
#pragma HLS INTERFACE s_axilite port=idx bundle=control
#pragma HLS INTERFACE s_axilite port=return bundle=control
    
    if (idx >= 0 && idx < BRAM_IMGF_SIZE) {
        return imgf_bram[idx];
    } else {
        compute_status = 3;
        return cmpxFloat(0.0f, 0.0f);
    }
}

float read_img_out_data(int idx) {
#pragma HLS INLINE
#pragma HLS INTERFACE s_axilite port=idx bundle=control
#pragma HLS INTERFACE s_axilite port=return bundle=control
    
    if (idx >= 0 && idx < BRAM_IMG_OUT_SIZE) {
        return img_out_bram[idx];
    } else {
        compute_status = 3;
        return 0.0f;
    }
}

//=============================================================================
// Batch Loading Functions Implementation
//=============================================================================

void load_source_batch(cmpxFloat data[BRAM_SOURCE_SIZE]) {
#pragma HLS INTERFACE s_axilite port=data bundle=control
    
LOAD_SOURCE_BATCH_LOOP:
    for (int i = 0; i < BRAM_SOURCE_SIZE; i++) {
#pragma HLS PIPELINE II=1
        source_bram[i] = data[i];
    }
}

void load_mask_batch(cmpxFloat data[BRAM_MASK_SIZE]) {
#pragma HLS INTERFACE s_axilite port=data bundle=control
    
LOAD_MASK_BATCH_LOOP:
    for (int i = 0; i < BRAM_MASK_SIZE; i++) {
#pragma HLS PIPELINE II=1
        mask_bram[i] = data[i];
    }
}

void load_kernels_batch(cmpxFloat data[BRAM_KERNELS_SIZE]) {
#pragma HLS INTERFACE s_axilite port=data bundle=control
    
LOAD_KERNELS_BATCH_LOOP:
    for (int i = 0; i < BRAM_KERNELS_SIZE; i++) {
#pragma HLS PIPELINE II=1
        kernels_bram[i] = data[i];
    }
}

void load_scales_batch(float data[BRAM_SCALES_SIZE]) {
#pragma HLS INTERFACE s_axilite port=data bundle=control
    
LOAD_SCALES_BATCH_LOOP:
    for (int i = 0; i < BRAM_SCALES_SIZE; i++) {
#pragma HLS PIPELINE II=1
        scales_bram[i] = data[i];
    }
}

//=============================================================================
// Compute Control Functions Implementation
//=============================================================================

void start_litho_compute(
    int mode,
    int Lx, int Ly,
    int Nx, int Ny,
    int srcSize,
    int nkernels
) {
#pragma HLS INTERFACE s_axilite port=mode bundle=control
#pragma HLS INTERFACE s_axilite port=Lx bundle=control
#pragma HLS INTERFACE s_axilite port=Ly bundle=control
#pragma HLS INTERFACE s_axilite port=Nx bundle=control
#pragma HLS INTERFACE s_axilite port=Ny bundle=control
#pragma HLS INTERFACE s_axilite port=srcSize bundle=control
#pragma HLS INTERFACE s_axilite port=nkernels bundle=control
#pragma HLS INTERFACE s_axilite port=return bundle=control
    
    // 参数验证
    bool params_valid = true;
    
    // 模式检查
    if (mode != 1 && mode != 2) {
        params_valid = false;
    }
    
    // 尺寸检查
    if (Lx < 1 || Lx > BRAM_MAX_LX || Ly < 1 || Ly > BRAM_MAX_LY) {
        params_valid = false;
    }
    
    // TCC模式Nx限制检查
    if (mode == 1 && Nx > BRAM_MAX_NX_TCC) {
        params_valid = false;
    }
    
    // SOCS模式参数检查
    if (mode == 2 && nkernels > BRAM_MAX_KERNELS) {
        params_valid = false;
    }
    
    // 光源尺寸检查 (仅TCC模式需要)
    if (mode == 1 && (srcSize < 1 || srcSize > BRAM_MAX_SRC_SIZE)) {
        params_valid = false;
    }
    
    // SOCS模式下srcSize可以为0 (不需要光源数据)
    if (mode == 2 && srcSize < 0) {
        params_valid = false;
    }
    
    // 如果参数无效，设置错误状态并返回
    if (!params_valid) {
        compute_status = 3;  // error
        return;
    }
    
    // 设置运行状态
    compute_status = 1;  // running
    
    // 根据模式调用相应的计算模块
    if (mode == 1) {
        // TCC模式
        hls_litho_tcc_mode_bram(
            source_bram, mask_bram, tcc_bram, imgf_bram,
            Lx, Ly, Nx, Ny, srcSize
        );
    } else if (mode == 2) {
        // SOCS模式
        hls_litho_socs_mode_bram(
            kernels_bram, scales_bram, mask_bram, img_out_bram,
            Lx, Ly, Nx, Ny, nkernels
        );
    }
    
    // 设置完成状态
    compute_status = 2;  // done
}

int get_compute_status() {
#pragma HLS INTERFACE s_axilite port=return bundle=control
    return compute_status;
}

void reset_bram_storage() {
#pragma HLS INTERFACE s_axilite port=return bundle=control
    
    // 清空BRAM存储 (简化实现，实际可优化)
RESET_SOURCE_LOOP:
    for (int i = 0; i < BRAM_SOURCE_SIZE; i++) {
#pragma HLS PIPELINE II=1
        source_bram[i] = cmpxFloat(0.0f, 0.0f);
    }
    
RESET_MASK_LOOP:
    for (int i = 0; i < BRAM_MASK_SIZE; i++) {
#pragma HLS PIPELINE II=1
        mask_bram[i] = cmpxFloat(0.0f, 0.0f);
    }
    
RESET_TCC_LOOP:
    for (int i = 0; i < BRAM_TCC_SIZE; i++) {
#pragma HLS PIPELINE II=1
        tcc_bram[i] = cmpxFloat(0.0f, 0.0f);
    }
    
RESET_KERNELS_LOOP:
    for (int i = 0; i < BRAM_KERNELS_SIZE; i++) {
#pragma HLS PIPELINE II=1
        kernels_bram[i] = cmpxFloat(0.0f, 0.0f);
    }
    
RESET_SCALES_LOOP:
    for (int i = 0; i < BRAM_SCALES_SIZE; i++) {
#pragma HLS PIPELINE II=1
        scales_bram[i] = 0.0f;
    }
    
RESET_IMGF_LOOP:
    for (int i = 0; i < BRAM_IMGF_SIZE; i++) {
#pragma HLS PIPELINE II=1
        imgf_bram[i] = cmpxFloat(0.0f, 0.0f);
    }
    
RESET_IMG_OUT_LOOP:
    for (int i = 0; i < BRAM_IMG_OUT_SIZE; i++) {
#pragma HLS PIPELINE II=1
        img_out_bram[i] = 0.0f;
    }
    
    // 重置状态
    compute_status = 0;  // idle
}

//=============================================================================
// Internal Compute Functions Implementation
//=============================================================================

void hls_litho_tcc_mode_bram(
    cmpxFloat source[BRAM_SOURCE_SIZE],
    cmpxFloat mask_fft[BRAM_MASK_SIZE],
    cmpxFloat tcc[BRAM_TCC_SIZE],
    cmpxFloat imgf[BRAM_IMGF_SIZE],
    int Lx, int Ly,
    int Nx, int Ny,
    int srcSize
) {
    // 本地缓存用于calcImage计算
    cmpxFloat tcc_local[BRAM_TCC_SIZE];
    cmpxFloat imgf_local[BRAM_IMGF_SIZE];
    
#pragma HLS BIND_STORAGE variable=tcc_local type=RAM_2P impl=BRAM
#pragma HLS BIND_STORAGE variable=imgf_local type=RAM_2P impl=BRAM
    
    // 数组分区优化并行访问
#pragma HLS ARRAY_PARTITION variable=tcc_local cyclic factor=8
#pragma HLS ARRAY_PARTITION variable=imgf_local cyclic factor=8
    
    // TCC矩阵计算 (简化版本 - 使用预计算的TCC)
    // 在实际应用中，TCC应由CPU预计算并加载到tcc_bram
    // 这里我们从tcc_bram复制到tcc_local
COPY_TCC_LOOP:
    for (int i = 0; i < BRAM_TCC_SIZE; i++) {
#pragma HLS PIPELINE II=1
        tcc_local[i] = tcc[i];
    }
    
    // 调用calcImage模块计算频域输出
    // 注意: 这里需要将mask_fft和tcc_local转换为calcImage期望的格式
    // 由于calcImage需要特定的数组布局，这里使用简化的占位实现
    
    // 简化实现: 直接复制mask_fft到imgf (实际应调用calcImage)
COPY_MASK_TO_IMGF_LOOP:
    for (int i = 0; i < Lx * Ly && i < BRAM_IMGF_SIZE; i++) {
#pragma HLS PIPELINE II=1
        // 简化: imgf[i] = mask_fft[i] * tcc权重
        // 实际应调用: hls_calc_image_integrated(mask_fft, tcc_local, imgf_local, ...)
        imgf_local[i] = mask_fft[i] * tcc_local[0];  // 占位实现
    }
    
    // 复制结果到输出BRAM
COPY_IMGF_OUT_LOOP:
    for (int i = 0; i < BRAM_IMGF_SIZE; i++) {
#pragma HLS PIPELINE II=1
        imgf[i] = imgf_local[i];
    }
}

void hls_litho_socs_mode_bram(
    cmpxFloat kernels[BRAM_KERNELS_SIZE],
    float scales[BRAM_SCALES_SIZE],
    cmpxFloat mask_fft[BRAM_MASK_SIZE],
    float img_out[BRAM_IMG_OUT_SIZE],
    int Lx, int Ly,
    int Nx, int Ny,
    int nkernels
) {
    // 本地缓存
    cmpxFloat mask_local[BRAM_MASK_SIZE];
    cmpxFloat product_acc[BRAM_IMGF_SIZE];  // 累加器
    float img_acc[BRAM_IMG_OUT_SIZE];
    
#pragma HLS BIND_STORAGE variable=mask_local type=RAM_2P impl=BRAM
#pragma HLS BIND_STORAGE variable=product_acc type=RAM_2P impl=BRAM
#pragma HLS BIND_STORAGE variable=img_acc type=RAM_2P impl=BRAM
    
    // 数组分区
#pragma HLS ARRAY_PARTITION variable=mask_local cyclic factor=4
#pragma HLS ARRAY_PARTITION variable=product_acc cyclic factor=4
    
    // 初始化累加器
INIT_ACC_LOOP:
    for (int i = 0; i < BRAM_IMGF_SIZE; i++) {
#pragma HLS PIPELINE II=1
        product_acc[i] = cmpxFloat(0.0f, 0.0f);
    }
    
INIT_IMG_ACC_LOOP:
    for (int i = 0; i < BRAM_IMG_OUT_SIZE; i++) {
#pragma HLS PIPELINE II=1
        img_acc[i] = 0.0f;
    }
    
    // 复制mask到本地缓存
COPY_MASK_LOCAL_LOOP:
    for (int i = 0; i < Lx * Ly && i < BRAM_MASK_SIZE; i++) {
#pragma HLS PIPELINE II=1
        mask_local[i] = mask_fft[i];
    }
    
    // SOCS核累加计算
KERNEL_ACC_LOOP:
    for (int k = 0; k < nkernels; k++) {
        // 当前核的起始索引
        int kernel_start = k * 225;
        
        // Kernel-Mask乘法累加 (简化版本)
KERNEL_MASK_MUL_LOOP:
        for (int i = 0; i < Lx * Ly && i < BRAM_IMGF_SIZE; i++) {
#pragma HLS PIPELINE II=1
            
            // 获取kernel值 (假设kernel尺寸为15x15=225)
            int kernel_idx = i % 225;
            cmpxFloat kernel_val = kernels[kernel_start + kernel_idx];
            
            // 复数乘法
            float real_prod = mask_local[i].real() * kernel_val.real() 
                            - mask_local[i].imag() * kernel_val.imag();
            float imag_prod = mask_local[i].real() * kernel_val.imag() 
                            + mask_local[i].imag() * kernel_val.real();
            
            // 加权累加
            float scale = scales[k];
            product_acc[i] += cmpxFloat(real_prod * scale, imag_prod * scale);
        }
    }
    
    // 平方幅度计算 (简化版本 - 未实现完整的IFFT)
SQUARE_MAG_LOOP:
    for (int i = 0; i < Lx * Ly && i < BRAM_IMG_OUT_SIZE; i++) {
#pragma HLS PIPELINE II=1
        
        // 计算平方幅度
        float mag_sq = product_acc[i].real() * product_acc[i].real()
                     + product_acc[i].imag() * product_acc[i].imag();
        
        // 简化: 直接输出 (实际应有IFFT和循环移位)
        img_acc[i] = mag_sq;
    }
    
    // 复制结果到输出BRAM
COPY_IMG_OUT_LOOP:
    for (int i = 0; i < BRAM_IMG_OUT_SIZE; i++) {
#pragma HLS PIPELINE II=1
        img_out[i] = img_acc[i];
    }
}

//=============================================================================
// Top-Level Function Implementation
//=============================================================================

void hls_litho_system_bram(
    int mode,
    int Lx, int Ly,
    int Nx, int Ny,
    int srcSize,
    int nkernels
) {
#pragma HLS INTERFACE s_axilite port=mode bundle=control
#pragma HLS INTERFACE s_axilite port=Lx bundle=control
#pragma HLS INTERFACE s_axilite port=Ly bundle=control
#pragma HLS INTERFACE s_axilite port=Nx bundle=control
#pragma HLS INTERFACE s_axilite port=Ny bundle=control
#pragma HLS INTERFACE s_axilite port=srcSize bundle=control
#pragma HLS INTERFACE s_axilite port=nkernels bundle=control
#pragma HLS INTERFACE s_axilite port=return bundle=control

    // BRAM存储绑定 (强制使用BRAM实现，避免使用URAM)
#pragma HLS BIND_STORAGE variable=source_bram type=RAM_2P impl=BRAM
#pragma HLS BIND_STORAGE variable=mask_bram type=RAM_2P impl=BRAM
#pragma HLS BIND_STORAGE variable=tcc_bram type=RAM_2P impl=BRAM
#pragma HLS BIND_STORAGE variable=kernels_bram type=RAM_2P impl=BRAM
#pragma HLS BIND_STORAGE variable=imgf_bram type=RAM_2P impl=BRAM
#pragma HLS BIND_STORAGE variable=img_out_bram type=RAM_2P impl=BRAM
    
    // 调用计算控制函数
    start_litho_compute(mode, Lx, Ly, Nx, Ny, srcSize, nkernels);
}