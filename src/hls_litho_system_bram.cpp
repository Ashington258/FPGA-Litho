/**
 * @file hls_litho_system_bram.cpp
 * @brief K-Litho BRAM Single-Function Architecture (Phase 6C Refactor)
 * 
 * 单函数架构 - 所有BRAM操作通过operation参数控制
 * 
 * Operation编码:
 * 0=load_source, 1=load_mask, 2=load_tcc, 3=load_kernels, 4=load_scales,
 * 5=compute_tcc, 6=compute_socs, 7=read_imgf, 8=read_img_out, 9=reset
 * 
 * BRAM存储数组（静态局部变量）:
 * - 在函数内声明，HLS可正确绑定BIND_STORAGE pragma
 * - 静态变量保证数据在调用间持久化
 * - 预期资源: ~57 BRAM_18K blocks
 * 
 * @author K-Litho Team
 * @date 2026-04-04
 */

#include "../include/hls_litho_system_bram.h"
#include <hls_math.h>

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief 循环移位函数 - 将频域中心移到图像中心
 * @param in 输入数组
 * @param out 输出数组
 * @param sizeX X方向尺寸
 * @param sizeY Y方向尺寸
 * @param shiftTypeX X方向移位类型（true=sizeX/2, false=(sizeX+1)/2）
 * @param shiftTypeY Y方向移位类型
 */
void hls_shift_array_real_bram(
    float in[],
    float out[],
    int sizeX,
    int sizeY,
    bool shiftTypeX,
    bool shiftTypeY
) {
    int xh = shiftTypeX ? (sizeX / 2) : ((sizeX + 1) / 2);
    int yh = shiftTypeY ? (sizeY / 2) : ((sizeY + 1) / 2);
    
    for (int y = 0; y < sizeY; y++) {
        for (int x = 0; x < sizeX; x++) {
#pragma HLS PIPELINE II=1
            int sy = (y + yh) % sizeY;
            int sx = (x + xh) % sizeX;
            out[sy * sizeX + sx] = in[y * sizeX + x];
        }
    }
}

//=============================================================================
// Top-Level Single-Function Implementation
//=============================================================================

cmpxFloat hls_litho_system_bram(
    // Control parameters
    int operation,    // Operation code (0-9)
    
    // Data parameters (for load/read operations)
    int idx,          // Array index
    cmpxFloat val,    // Value to load (complex float)
    
    // Compute parameters (for compute operations)
    int mode,         // Compute mode (unused in single-function)
    int Lx, int Ly,   // Frequency domain size
    int Nx, int Ny,   // TCC/SOCS parameters
    int srcSize,      // Source size
    int nkernels      // Number of SOCS kernels
) {
#pragma HLS INTERFACE s_axilite port=operation bundle=control
#pragma HLS INTERFACE s_axilite port=idx bundle=control
#pragma HLS INTERFACE s_axilite port=val bundle=control
#pragma HLS INTERFACE s_axilite port=mode bundle=control
#pragma HLS INTERFACE s_axilite port=Lx bundle=control
#pragma HLS INTERFACE s_axilite port=Ly bundle=control
#pragma HLS INTERFACE s_axilite port=Nx bundle=control
#pragma HLS INTERFACE s_axilite port=Ny bundle=control
#pragma HLS INTERFACE s_axilite port=srcSize bundle=control
#pragma HLS INTERFACE s_axilite port=nkernels bundle=control
#pragma HLS INTERFACE s_axilite port=return bundle=control

    // Static local BRAM arrays (persistent across calls)
    static cmpxFloat source_bram[BRAM_SOURCE_SIZE];
    static cmpxFloat mask_bram[BRAM_MASK_SIZE];
    static cmpxFloat tcc_bram[BRAM_TCC_SIZE];
    static cmpxFloat kernels_bram[BRAM_KERNELS_SIZE];
    static float scales_bram[BRAM_SCALES_SIZE];
    static cmpxFloat imgf_bram[BRAM_IMGF_SIZE];
    static float img_out_bram[BRAM_IMG_OUT_SIZE];
    
    // BRAM storage binding (force BRAM implementation)
#pragma HLS BIND_STORAGE variable=source_bram type=RAM_2P impl=BRAM
#pragma HLS BIND_STORAGE variable=mask_bram type=RAM_2P impl=BRAM
#pragma HLS BIND_STORAGE variable=tcc_bram type=RAM_2P impl=BRAM
#pragma HLS BIND_STORAGE variable=kernels_bram type=RAM_2P impl=BRAM
#pragma HLS BIND_STORAGE variable=scales_bram type=RAM_2P impl=BRAM
#pragma HLS BIND_STORAGE variable=imgf_bram type=RAM_2P impl=BRAM
#pragma HLS BIND_STORAGE variable=img_out_bram type=RAM_2P impl=BRAM
    
    // Array partitioning for parallel access
#pragma HLS ARRAY_PARTITION variable=source_bram cyclic factor=4
#pragma HLS ARRAY_PARTITION variable=mask_bram cyclic factor=4
#pragma HLS ARRAY_PARTITION variable=tcc_bram cyclic factor=4
#pragma HLS ARRAY_PARTITION variable=kernels_bram cyclic factor=8
#pragma HLS ARRAY_PARTITION variable=imgf_bram cyclic factor=4
    
    // Result variable for read operations
    cmpxFloat result = cmpxFloat(0.0f, 0.0f);
    
    // Execute operation based on operation code
    switch(operation) {
        
        //=============================================================
        // Load Operations (0-4)
        //=============================================================
        case OP_LOAD_SOURCE: {
            if (idx >= 0 && idx < BRAM_SOURCE_SIZE) {
                source_bram[idx] = val;
            }
            break;
        }
        
        case OP_LOAD_MASK: {
            if (idx >= 0 && idx < BRAM_MASK_SIZE) {
                mask_bram[idx] = val;
            }
            break;
        }
        
        case OP_LOAD_TCC: {
            if (idx >= 0 && idx < BRAM_TCC_SIZE) {
                tcc_bram[idx] = val;
            }
            break;
        }
        
        case OP_LOAD_KERNELS: {
            if (idx >= 0 && idx < BRAM_KERNELS_SIZE) {
                kernels_bram[idx] = val;
            }
            break;
        }
        
        case OP_LOAD_SCALES: {
            if (idx >= 0 && idx < BRAM_SCALES_SIZE) {
                scales_bram[idx] = val.real();  // Use real part for float
            }
            break;
        }
        
        //=============================================================
        // Compute Operations (5-6)
        //=============================================================
        case OP_COMPUTE_TCC: {
            // Parameter validation
            if (Nx > BRAM_MAX_NX_TCC || Lx > BRAM_MAX_LX || Ly > BRAM_MAX_LY || Lx <= 0 || Ly <= 0) {
                result = cmpxFloat(-1.0f, 0.0f);  // Error indicator
                break;
            }
            
            // TCC mode simplified compute
            // Local caches for calculation
            cmpxFloat mask_local[BRAM_MASK_SIZE];
            cmpxFloat imgf_local[BRAM_IMGF_SIZE];
            
#pragma HLS BIND_STORAGE variable=mask_local type=RAM_2P impl=BRAM
#pragma HLS BIND_STORAGE variable=imgf_local type=RAM_2P impl=BRAM
            
            // Copy mask to local cache
COPY_MASK_TCC_LOOP:
            for (int i = 0; i < Lx * Ly; i++) {
#pragma HLS PIPELINE II=1
                mask_local[i] = mask_bram[i];
            }
            
            // Simplified TCC convolution (placeholder for real calcImage)
CALC_TCC_LOOP:
            for (int i = 0; i < Lx * Ly; i++) {
#pragma HLS PIPELINE II=1
                cmpxFloat tcc_weight = (BRAM_TCC_SIZE > 0) ? tcc_bram[0] : cmpxFloat(1.0f, 0.0f);
                imgf_local[i] = cmpxFloat(
                    mask_local[i].real() * tcc_weight.real() - mask_local[i].imag() * tcc_weight.imag(),
                    mask_local[i].real() * tcc_weight.imag() + mask_local[i].imag() * tcc_weight.real()
                );
            }
            
            // Copy result to imgf_bram
COPY_IMGF_TCC_LOOP:
            for (int i = 0; i < Lx * Ly; i++) {
#pragma HLS PIPELINE II=1
                imgf_bram[i] = imgf_local[i];
            }
            
            result = cmpxFloat(1.0f, 0.0f);  // Success indicator
            break;
        }
        
        case OP_COMPUTE_SOCS: {
            // Parameter validation
            if (nkernels > BRAM_MAX_KERNELS || Lx > BRAM_MAX_LX || Ly > BRAM_MAX_LY || nkernels <= 0 || Lx <= 0 || Ly <= 0) {
                result = cmpxFloat(-1.0f, 0.0f);  // Error indicator
                break;
            }
            
            // SOCS mode full algorithm (移植自hls_socs.cpp)
            // 计算尺寸参数
            int sizeX = 4 * Nx + 1;
            int sizeY = 4 * Ny + 1;
            int difX = sizeX - (2 * Nx + 1);
            int difY = sizeY - (2 * Ny + 1);
            int kernelSize = (2 * Nx + 1) * (2 * Ny + 1);
            int Lxh = Lx / 2;
            int Lyh = Ly / 2;
            
            // 本地工作数组
            cmpxFloat mask_local[BRAM_MASK_SIZE];
            cmpxFloat product[(4 * BRAM_MAX_NX_SOCS + 1) * (4 * BRAM_MAX_NY + 1)];
            float img_accum[(4 * BRAM_MAX_NX_SOCS + 1) * (4 * BRAM_MAX_NY + 1)];
            float img_shifted[(4 * BRAM_MAX_NX_SOCS + 1) * (4 * BRAM_MAX_NY + 1)];
            
#pragma HLS BIND_STORAGE variable=mask_local type=RAM_2P impl=BRAM
#pragma HLS BIND_STORAGE variable=product type=RAM_2P impl=BRAM
#pragma HLS BIND_STORAGE variable=img_accum type=RAM_2P impl=BRAM
#pragma HLS BIND_STORAGE variable=img_shifted type=RAM_2P impl=BRAM
            
#pragma HLS ARRAY_PARTITION variable=mask_local cyclic factor=4
#pragma HLS ARRAY_PARTITION variable=product cyclic factor=4
#pragma HLS ARRAY_PARTITION variable=img_accum cyclic factor=4
            
            // 步骤1: 复制mask到本地缓存
COPY_MASK_SOCS_LOOP:
            for (int i = 0; i < Lx * Ly; i++) {
#pragma HLS PIPELINE II=1
                mask_local[i] = mask_bram[i];
            }
            
            // 步骤2: 初始化product和累加器
INIT_PRODUCT_SOCS_LOOP:
            for (int i = 0; i < sizeX * sizeY; i++) {
#pragma HLS PIPELINE II=1
                product[i] = cmpxFloat(0.0f, 0.0f);
                img_accum[i] = 0.0f;
            }
            
            // 步骤3: 多核循环计算
            for (int k = 0; k < nkernels; k++) {
                // Kernel-Mask复数乘法并填充到product数组
                for (int y = -Ny; y <= Ny; y++) {
                    for (int x = -Nx; x <= Nx; x++) {
#pragma HLS PIPELINE II=1
                        
                        // Kernel索引 (正确映射)
                        int krn_idx = (y + Ny) * (2 * Nx + 1) + (x + Nx);
                        
                        // Mask索引 (取中央区域)
                        int msk_idx = (y + Lyh) * Lx + (x + Lxh);
                        
                        // Product位置 (填充到扩展区域)
                        int prod_idx = (difY + y + Ny) * sizeX + difX + x + Nx;
                        
                        // 复数乘法: kernel[k][krn_idx] * mask[msk_idx] (DSP映射)
                        cmpxFloat krn = kernels_bram[k * kernelSize + krn_idx];
                        cmpxFloat msk = mask_local[msk_idx];
                        
                        float real_prod = krn.real() * msk.real() - krn.imag() * msk.imag();
                        float imag_prod = krn.real() * msk.imag() + krn.imag() * msk.real();
                        
                        product[prod_idx] = cmpxFloat(real_prod, imag_prod);
                    }
                }
                
                // 平方幅度累加
                float scale = scales_bram[k];
                
ACCUM_MAG_SOCS_LOOP:
                for (int i = 0; i < sizeX * sizeY; i++) {
#pragma HLS PIPELINE II=1
                    float real_val = product[i].real();
                    float imag_val = product[i].imag();
                    float mag_sq = real_val * real_val + imag_val * imag_val;
                    
                    img_accum[i] += scale * mag_sq;
                }
            }
            
            // 步骤4: 循环移位 (将频域中心移到图像中心)
            hls_shift_array_real_bram(img_accum, img_shifted, sizeX, sizeY, true, true);
            
            // 步骤5: 输出截取中央区域
COPY_IMG_OUT_SOCS_LOOP:
            for (int i = 0; i < Lx * Ly; i++) {
#pragma HLS PIPELINE II=1
                // 如果输出尺寸大于计算尺寸，填充零
                if (i < sizeX * sizeY) {
                    img_out_bram[i] = img_shifted[i];
                } else {
                    img_out_bram[i] = 0.0f;
                }
            }
            
            result = cmpxFloat(1.0f, 0.0f);  // Success indicator
            break;
        }
        
        //=============================================================
        // Read Operations (7-8)
        //=============================================================
        case OP_READ_IMGF: {
            if (idx >= 0 && idx < BRAM_IMGF_SIZE) {
                result = imgf_bram[idx];
            }
            break;
        }
        
        case OP_READ_IMG_OUT: {
            if (idx >= 0 && idx < BRAM_IMG_OUT_SIZE) {
                result = cmpxFloat(img_out_bram[idx], 0.0f);  // Return as complex
            }
            break;
        }
        
        //=============================================================
        // Reset Operation (9)
        //=============================================================
        case OP_RESET: {
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
            result = cmpxFloat(1.0f, 0.0f);  // Success indicator
            break;
        }
        
        default: {
            result = cmpxFloat(-1.0f, 0.0f);  // Invalid operation
            break;
        }
    }
    
    return result;
}