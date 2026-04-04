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
            
            // SOCS mode simplified compute
            cmpxFloat mask_local[BRAM_MASK_SIZE];
            cmpxFloat product_acc[BRAM_IMGF_SIZE];
            float img_acc[BRAM_IMG_OUT_SIZE];
            
#pragma HLS BIND_STORAGE variable=mask_local type=RAM_2P impl=BRAM
#pragma HLS BIND_STORAGE variable=product_acc type=RAM_2P impl=BRAM
#pragma HLS BIND_STORAGE variable=img_acc type=RAM_2P impl=BRAM
            
            // Initialize accumulators
INIT_SOCS_ACC_LOOP:
            for (int i = 0; i < BRAM_IMGF_SIZE; i++) {
#pragma HLS PIPELINE II=1
                product_acc[i] = cmpxFloat(0.0f, 0.0f);
            }
            
INIT_IMG_SOCS_LOOP:
            for (int i = 0; i < BRAM_IMG_OUT_SIZE; i++) {
#pragma HLS PIPELINE II=1
                img_acc[i] = 0.0f;
            }
            
            // Copy mask to local
COPY_MASK_SOCS_LOOP:
            for (int i = 0; i < Lx * Ly; i++) {
#pragma HLS PIPELINE II=1
                mask_local[i] = mask_bram[i];
            }
            
            // Kernel accumulation (simplified)
KERNEL_SOCS_ACC_LOOP:
            for (int k = 0; k < nkernels; k++) {
                int kernel_start = k * 225;
                
KERNEL_MASK_SOCS_LOOP:
                for (int i = 0; i < Lx * Ly; i++) {
#pragma HLS PIPELINE II=1
                    int kernel_idx = i % 225;
                    cmpxFloat kernel_val = kernels_bram[kernel_start + kernel_idx];
                    
                    float real_prod = mask_local[i].real() * kernel_val.real() 
                                    - mask_local[i].imag() * kernel_val.imag();
                    float imag_prod = mask_local[i].real() * kernel_val.imag() 
                                    + mask_local[i].imag() * kernel_val.real();
                    
                    float scale = scales_bram[k];
                    product_acc[i] += cmpxFloat(real_prod * scale, imag_prod * scale);
                }
            }
            
            // Square magnitude calculation
SQUARE_MAG_SOCS_LOOP:
            for (int i = 0; i < Lx * Ly; i++) {
#pragma HLS PIPELINE II=1
                float mag_sq = product_acc[i].real() * product_acc[i].real()
                             + product_acc[i].imag() * product_acc[i].imag();
                img_acc[i] = mag_sq;
            }
            
            // Copy result to img_out_bram
COPY_IMG_OUT_SOCS_LOOP:
            for (int i = 0; i < Lx * Ly; i++) {
#pragma HLS PIPELINE II=1
                img_out_bram[i] = img_acc[i];
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