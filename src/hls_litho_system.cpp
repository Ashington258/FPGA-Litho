/**
 * @file hls_litho_system.cpp
 * @brief FPGA-Litho Complete System Integration Implementation
 * 
 * 实现完整的光刻模拟系统集成，支持TCC和SOCS两种工作模式
 * 
 * @author FPGA-Litho Team
 * @date 2026-04-03
 */

#include "../include/hls_litho_system.h"
#include "../include/hls_tcc.h"
#include "../include/hls_socs.h"
#include "../include/hls_calc_image_integrated.h"
#include "../include/hls_shift.h"
#include <hls_math.h>

//=============================================================================
// Parameter Initialization
//=============================================================================

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
) {
    params.lambda = lambda;
    params.NA = NA;
    params.defocus = defocus;
    params.Lx = Lx;
    params.Ly = Ly;
    params.Nx = Nx;
    params.Ny = Ny;
    params.srcSize = srcSize;
    params.nkernels = nkernels;
    params.mode = mode;
}

//=============================================================================
// TCC Mode Implementation
//=============================================================================

void hls_litho_tcc_mode(
    cmpxFloat source[SYS_MAX_SRC_SIZE * SYS_MAX_SRC_SIZE],
    cmpxFloat mask_fft[SYS_MAX_LX * SYS_MAX_LY],
    cmpxFloat imgf[SYS_MAX_LX * SYS_MAX_LY],
    LithoSystemParams &params
) {
#pragma HLS DATAFLOW
    
    //=========================================================================
    // Step 1: TCC矩阵计算 (使用验证的TCC模块)
    //=========================================================================
    
    // 本地TCC矩阵缓存
    cmpxFloat tcc_local[SYS_TCC_TOTAL];
#pragma HLS BIND_STORAGE variable=tcc_local type=RAM_2P impl=BRAM
#pragma HLS ARRAY_PARTITION variable=tcc_local cyclic factor=4 dim=1
    
    // 调用TCC计算模块 (简化版本，实际需要完整的hls_calc_tcc)
    // TODO: 集成完整的TCC计算模块
    // hls_calc_tcc(source, tcc_local, params.srcSize, params.Nx, params.Ny);
    
    // 当前简化实现：直接使用预计算的TCC（由Host提供）
    // 在完整系统中，TCC计算应在前置阶段完成
    
    //=========================================================================
    // Step 2: calcImage频域计算 (使用验证的200MHz模块)
    //=========================================================================
    
    // 调用验证的calcImage模块
    hls_calc_image_integrated(
        mask_fft,
        tcc_local,
        imgf,
        params.Lx,
        params.Ly,
        params.Nx,
        params.Ny
    );
}

//=============================================================================
// SOCS Mode Implementation  
//=============================================================================

void hls_litho_socs_mode(
    cmpxFloat kernels[SYS_MAX_KERNELS * SYS_TCC_DIM],
    float scales[SYS_MAX_KERNELS],
    cmpxFloat mask_fft[SYS_MAX_LX * SYS_MAX_LY],
    float img_out[SYS_OUTPUT_SIZE],
    LithoSystemParams &params
) {
#pragma HLS DATAFLOW
    
    //=========================================================================
    // Step 1: 数据预取到本地缓存
    //=========================================================================
    
    // Kernel本地缓存 (分区以支持并行访问)
    cmpxFloat kernel_local[SYS_MAX_KERNELS * SYS_TCC_DIM];
#pragma HLS BIND_STORAGE variable=kernel_local type=RAM_2P impl=BRAM
#pragma HLS ARRAY_PARTITION variable=kernel_local cyclic factor=4 dim=1
    
    // Mask本地缓存
    cmpxFloat mask_local[SYS_MAX_LX * SYS_MAX_LY];
#pragma HLS BIND_STORAGE variable=mask_local type=RAM_2P impl=BRAM
#pragma HLS ARRAY_PARTITION variable=mask_local cyclic factor=4 dim=1
    
    // Scales本地缓存
    float scales_local[SYS_MAX_KERNELS];
#pragma HLS BIND_STORAGE variable=scales_local type=RAM_1P impl=BRAM
    
    // 输出图像缓存
    float img_accum[SYS_OUTPUT_SIZE];
#pragma HLS BIND_STORAGE variable=img_accum type=RAM_2P impl=BRAM
#pragma HLS ARRAY_PARTITION variable=img_accum cyclic factor=4 dim=1
    
    //=========================================================================
    // Step 2: 预取数据
    //=========================================================================
    
    int kernel_total = params.nkernels * SYS_TCC_DIM;
    int mask_total = params.Lx * params.Ly;
    int output_total = (4*params.Nx+1) * (4*params.Ny+1);
    
    // 预取Kernel数据
    for (int i = 0; i < kernel_total; i++) {
#pragma HLS LOOP_TRIPCOUNT min=392 max=392 avg=392
#pragma HLS PIPELINE II=1
        kernel_local[i] = kernels[i];
    }
    
    // 预取Mask数据
    for (int i = 0; i < mask_total; i++) {
#pragma HLS LOOP_TRIPCOUNT min=1024 max=4096 avg=1024
#pragma HLS PIPELINE II=1
        mask_local[i] = mask_fft[i];
    }
    
    // 预取Scales数据
    for (int k = 0; k < params.nkernels; k++) {
#pragma HLS LOOP_TRIPCOUNT min=1 max=8 avg=4
#pragma HLS PIPELINE II=1
        scales_local[k] = scales[k];
    }
    
    // 初始化输出缓存
    for (int i = 0; i < output_total; i++) {
#pragma HLS LOOP_TRIPCOUNT min=225 max=225 avg=225
#pragma HLS PIPELINE II=1
        img_accum[i] = 0.0f;
    }
    
    //=========================================================================
    // Step 3: 多核SOCS计算 (调用验证的SOCS核心模块)
    //=========================================================================
    
    // 调用SOCS核心计算模块
    // 注意：简化实现，完整版本需要集成IFFT
    hls_calc_socs(
        kernel_local,
        scales_local,
        mask_local,
        img_accum,
        params.nkernels,
        params.Lx, params.Ly,
        params.Nx, params.Ny
    );
    
    //=========================================================================
    // Step 4: 输出结果回写
    //=========================================================================
    
    for (int i = 0; i < output_total; i++) {
#pragma HLS LOOP_TRIPCOUNT min=225 max=225 avg=225
#pragma HLS PIPELINE II=1
        img_out[i] = img_accum[i];
    }
}

//=============================================================================
// Top-Level System Integration
//=============================================================================

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
) {
    //=========================================================================
    // AXI接口定义
    //=========================================================================
    
    // AXI-Master 接口 (内存映射)
#pragma HLS INTERFACE m_axi port=source depth=SYS_MAX_SRC_SIZE*SYS_MAX_SRC_SIZE \
    bundle=gmem0 max_read_burst_length=256
#pragma HLS INTERFACE m_axi port=mask_fft depth=SYS_MAX_LX*SYS_MAX_LY \
    bundle=gmem1 max_read_burst_length=256
#pragma HLS INTERFACE m_axi port=tcc depth=SYS_TCC_TOTAL \
    bundle=gmem2 max_read_burst_length=256
#pragma HLS INTERFACE m_axi port=kernels depth=SYS_MAX_KERNELS*SYS_TCC_DIM \
    bundle=gmem3 max_read_burst_length=256
#pragma HLS INTERFACE m_axi port=scales depth=SYS_MAX_KERNELS \
    bundle=gmem4 max_read_burst_length=64
#pragma HLS INTERFACE m_axi port=imgf depth=SYS_MAX_LX*SYS_MAX_LY \
    bundle=gmem5 max_write_burst_length=256
#pragma HLS INTERFACE m_axi port=img_out depth=SYS_OUTPUT_SIZE \
    bundle=gmem6 max_write_burst_length=256
    
    // AXI-Lite 控制接口
#pragma HLS INTERFACE s_axilite port=lambda bundle=control
#pragma HLS INTERFACE s_axilite port=NA bundle=control
#pragma HLS INTERFACE s_axilite port=defocus bundle=control
#pragma HLS INTERFACE s_axilite port=Lx bundle=control
#pragma HLS INTERFACE s_axilite port=Ly bundle=control
#pragma HLS INTERFACE s_axilite port=Nx bundle=control
#pragma HLS INTERFACE s_axilite port=Ny bundle=control
#pragma HLS INTERFACE s_axilite port=srcSize bundle=control
#pragma HLS INTERFACE s_axilite port=nkernels bundle=control
#pragma HLS INTERFACE s_axilite port=mode bundle=control
#pragma HLS INTERFACE s_axilite port=return bundle=control
    
    //=========================================================================
    // 参数初始化
    //=========================================================================
    
    LithoSystemParams params;
    init_system_params(params, lambda, NA, defocus, Lx, Ly, Nx, Ny, srcSize, nkernels, mode);
    
    //=========================================================================
    // 工作模式选择
    //=========================================================================
    
    // 本地TCC缓存 (TCC模式使用)
    cmpxFloat tcc_local[SYS_TCC_TOTAL];
#pragma HLS BIND_STORAGE variable=tcc_local type=RAM_2P impl=BRAM
#pragma HLS ARRAY_PARTITION variable=tcc_local cyclic factor=4 dim=1
    
    if (mode == 1) {
        //===============================================================
        // TCC模式: source → TCC → calcImage → imgf
        //===============================================================
        
        // 预取TCC数据 (假设Host已预计算)
        for (int i = 0; i < SYS_TCC_TOTAL; i++) {
#pragma HLS LOOP_TRIPCOUNT min=2401 max=2401 avg=2401
#pragma HLS PIPELINE II=1
            tcc_local[i] = tcc[i];
        }
        
        // 调用calcImage模块计算频域图像
        hls_calc_image_integrated(
            mask_fft,
            tcc_local,
            imgf,
            Lx, Ly, Nx, Ny
        );
        
    } else if (mode == 2) {
        //===============================================================
        // SOCS模式: kernels + mask → SOCS核心 → img_out
        //===============================================================
        
        // 调用SOCS模式处理
        hls_litho_socs_mode(
            kernels,
            scales,
            mask_fft,
            img_out,
            params
        );
        
    } else {
        //===============================================================
        // 默认/测试模式: 直接传递mask_fft到imgf
        //===============================================================
        
        int total_size = Lx * Ly;
        for (int i = 0; i < total_size; i++) {
#pragma HLS LOOP_TRIPCOUNT min=1024 max=4096 avg=1024
#pragma HLS PIPELINE II=1
            imgf[i] = mask_fft[i];
        }
    }
}

//=============================================================================
// SOCS核心调用 (直接使用现有hls_socs模块)
//=============================================================================

// 外部调用接口 - 使用现有hls_socs.cpp中已验证的版本
extern void hls_calc_socs(
    cmpxFloat *gmem_krn,    // AXI-Master: kernels[nk * kernelSize]
    float *gmem_scl,        // AXI-Master: scales[nk]
    cmpxFloat *gmem_msk,    // AXI-Master: mask_fft[Lx * Ly]
    float *gmem_img,        // AXI-Master: image_out[Lx * Ly]
    int nk,
    int Lx, int Ly,
    int Nx, int Ny
);

// SOCS模式包装器 - 转换数组指针格式
inline void hls_calc_socs_wrapper(
    cmpxFloat kernels[SYS_MAX_KERNELS * SYS_TCC_DIM],
    float scales[SYS_MAX_KERNELS],
    cmpxFloat mask[SYS_MAX_LX * SYS_MAX_LY],
    float img_out[SYS_OUTPUT_SIZE],
    int nkernels,
    int Lx, int Ly,
    int Nx, int Ny
) {
    // 直接调用外部SOCS函数
    hls_calc_socs(kernels, scales, mask, img_out, nkernels, Lx, Ly, Nx, Ny);
}