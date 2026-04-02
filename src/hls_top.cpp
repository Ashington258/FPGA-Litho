/*
 * K-Litho HLS Top Module
 * 光刻模拟顶层集成模块
 * 
 * 数据流架构:
 * Source Gen -> Mask Gen -> FFT R2C -> calcTCC/calcSOCS -> FFT C2R -> Output
 * 
 * 时钟约束: 5ns (200MHz)
 * - FFT模块: 支持200MHz
 * - calcImage: II=4 @ 200MHz (Fmax: 273MHz verified)
 * 
 * @author K-Litho Team
 * @date 2026-04-02 (Updated for 200MHz integration)
 */

#include "../include/hls_types.h"
#include "../include/hls_fft_r2c.h"
#include "../include/hls_fft_c2r.h"
#include "../include/hls_shift.h"
#include "../include/hls_calc_image_integrated.h"
#include <hls_stream.h>

using namespace hls;

// ============================================================
// FFT完整处理流程 (包含移位)
// ============================================================

/**
 * @brief 完整的FFT R2C流程
 * 包含输入移位 -> FFT -> 输出重组
 */
void fft_r2c_pipeline(
    hls::stream<realFloat> &data_in,
    hls::stream<cmpxFloat> &data_out,
    int sizeX,
    int sizeY
) {
#pragma HLS DATAFLOW

    // 内部流
    hls::stream<realFloat> shifted_in("shifted_in");
#pragma HLS STREAM depth=1024 variable=shifted_in

    // Step 1: 输入移位 (将数据中心移到角落)
    hls_shift_real(data_in, shifted_in, sizeX, sizeY, false, false);

    // Step 2: FFT R2C变换
    hls_fft_r2c(shifted_in, data_out, sizeX, sizeY);
}

/**
 * @brief 完整的IFFT C2R流程
 * 包含输入重组 -> IFFT -> 输出移位
 */
void fft_c2r_pipeline(
    hls::stream<cmpxFloat> &data_in,
    hls::stream<realFloat> &data_out,
    int sizeX,
    int sizeY
) {
#pragma HLS DATAFLOW

    // 内部流
    hls::stream<realFloat> ifft_out("ifft_out");
#pragma HLS STREAM depth=1024 variable=ifft_out

    // Step 1: IFFT C2R变换
    hls_fft_c2r(data_in, ifft_out, sizeX, sizeY);

    // Step 2: 输出移位 (将数据从角落移回中心)
    hls_shift_inverse_real(ifft_out, data_out, sizeX, sizeY);
}

// ============================================================
// 复数乘累加模块 (用于calcImage/calcSOCS)
// ============================================================

/**
 * @brief 复数向量乘累加
 * 
 * @param a       输入向量a
 * @param b       输入向量b
 * @param result  累加结果
 * @param size    向量长度
 */
void complex_mac_accumulate(
    hls::stream<cmpxFloat> &a,
    hls::stream<cmpxFloat> &b,
    cmpxFloat *result,
    int size
) {
#pragma HLS INTERFACE axis port=a
#pragma HLS INTERFACE axis port=b
#pragma HLS PIPELINE II=1

    cmpxFloat acc(0.0f, 0.0f);
    
    for (int i = 0; i < size; i++) {
#pragma HLS LOOP_TRIPCOUNT min=256 max=1024 avg=512
        cmpxFloat val_a = a.read();
        cmpxFloat val_b = b.read();
        acc += complex_mult(val_a, complex_conj(val_b));
    }
    
    *result = acc;
}

// ============================================================
// 简化的光学图像计算 (集成200MHz版本)
// ============================================================

/**
 * @brief 简化版光学图像频域计算 - 集成版本
 * 
 * 使用经过验证的200MHz calcImage kernel
 * II=4 @ 200MHz (Fmax: 273MHz)
 * 
 * @param mask_fft  掩模FFT
 * @param tcc       TCC矩阵 (简化处理)
 * @param imgf_out  输出图像频域
 * @param sizeX     X方向尺寸
 * @param sizeY     Y方向尺寸
 */
void calc_image_simple(
    hls::stream<cmpxFloat> &mask_fft,
    hls::stream<cmpxFloat> &tcc,
    hls::stream<cmpxFloat> &imgf_out,
    int sizeX,
    int sizeY
) {
#pragma HLS INTERFACE axis port=mask_fft
#pragma HLS INTERFACE axis port=tcc
#pragma HLS INTERFACE axis port=imgf_out
#pragma HLS PIPELINE II=1

    // 使用占位实现: 直接将mask_fft传递到输出
    // 完整calcImage集成需要AXI-Master接口，见hls_calc_image_integrated
    for (int i = 0; i < sizeX * sizeY; i++) {
#pragma HLS LOOP_TRIPCOUNT min=1024 max=1024 avg=1024
        cmpxFloat mask_val = mask_fft.read();
        imgf_out.write(mask_val);
    }
}

/**
 * @brief calcImage集成版本 (AXI-Master接口)
 * 
 * 需要外部调用者提供AXI-Master内存接口
 * 内部调用hls_calc_image_integrated
 * 
 * @param msk       掩模频谱数组
 * @param tcc       TCC矩阵数组
 * @param imgf      输出图像频谱数组
 * @param Lx        X方向尺寸
 * @param Ly        Y方向尺寸
 * @param Nx        TCC半宽
 * @param Ny        TCC半高
 */
void calc_image_integrated_wrapper(
    cmpxFloat msk[CI_MAX_LX * CI_MAX_LY],
    cmpxFloat tcc[CI_TCC_TOTAL],
    cmpxFloat imgf[CI_MAX_LX * CI_MAX_LY],
    int Lx,
    int Ly,
    int Nx,
    int Ny
) {
#pragma HLS DATAFLOW
    
    // 调用验证的200MHz calcImage kernel
    hls_calc_image_integrated(msk, tcc, imgf, Lx, Ly, Nx, Ny);
}

// ============================================================
// 顶层模块
// ============================================================

/**
 * @brief K-Litho 顶层集成模块
 * 
 * @param source_in   光源数据输入 (可选, 也可使用内部生成)
 * @param mask_in     掩模数据输入
 * @param image_out   输出光学图像
 * @param lambda      波长 (nm)
 * @param NA          数值孔径
 * @param defocus     离焦量 (nm)
 * @param sizeX       X方向尺寸
 * @param sizeY       Y方向尺寸
 * @param mode        运行模式 (0=FFT测试, 1=TCC模式, 2=SOCS模式)
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
) {
#pragma HLS INTERFACE axis port=source_in
#pragma HLS INTERFACE axis port=mask_in
#pragma HLS INTERFACE axis port=image_out
#pragma HLS INTERFACE s_axilite port=lambda,NA,defocus,sizeX,sizeY,mode
#pragma HLS INTERFACE s_axilite port=return
#pragma HLS DATAFLOW

    // ============================================================
    // 模式0: FFT测试模式
    // 直接测试FFT/IFFT流程
    // ============================================================
    
    if (mode == 0) {
        // FFT测试流程: Mask -> FFT -> IFFT -> Output
        hls::stream<cmpxFloat> mask_fft("mask_fft");
#pragma HLS STREAM depth=1024 variable=mask_fft

        // FFT R2C
        fft_r2c_pipeline(mask_in, mask_fft, sizeX, sizeY);
        
        // IFFT C2R
        fft_c2r_pipeline(mask_fft, image_out, sizeX, sizeY);
    }
    
    // ============================================================
    // 模式1: TCC模式 (占位)
    // 完整实现需要 calcTCC + calcImage
    // ============================================================
    
    else if (mode == 1) {
        // TODO: 完整TCC计算流程
        hls::stream<cmpxFloat> mask_fft("mask_fft");
        hls::stream<cmpxFloat> imgf_fft("imgf_fft");
#pragma HLS STREAM depth=1024 variable=mask_fft
#pragma HLS STREAM depth=1024 variable=imgf_fft

        // FFT R2C
        fft_r2c_pipeline(mask_in, mask_fft, sizeX, sizeY);
        
        // 简化图像计算 (完整实现见Phase 3)
        hls::stream<cmpxFloat> tcc_dummy("tcc_dummy");
#pragma HLS STREAM depth=256 variable=tcc_dummy
        calc_image_simple(mask_fft, tcc_dummy, imgf_fft, sizeX, sizeY);
        
        // IFFT C2R
        fft_c2r_pipeline(imgf_fft, image_out, sizeX, sizeY);
    }
    
    // ============================================================
    // 模式2: SOCS模式 (占位)
    // 完整实现需要 calcSOCS
    // ============================================================
    
    else if (mode == 2) {
        // TODO: 完整SOCS计算流程
        hls::stream<cmpxFloat> mask_fft("mask_fft");
        hls::stream<cmpxFloat> imgf_fft("imgf_fft");
#pragma HLS STREAM depth=1024 variable=mask_fft
#pragma HLS STREAM depth=1024 variable=imgf_fft

        // FFT R2C
        fft_r2c_pipeline(mask_in, mask_fft, sizeX, sizeY);
        
        // 简化处理 (完整实现见Phase 3)
        hls::stream<cmpxFloat> kernels_dummy("kernels_dummy");
#pragma HLS STREAM depth=256 variable=kernels_dummy
        calc_image_simple(mask_fft, kernels_dummy, imgf_fft, sizeX, sizeY);
        
        // IFFT C2R
        fft_c2r_pipeline(imgf_fft, image_out, sizeX, sizeY);
    }
}

// ============================================================
// 简化顶层接口 (仅FFT测试)
// ============================================================

/**
 * @brief 简化顶层接口 - 仅FFT/IFFT流程
 * 用于快速验证FFT模块
 */
void hls_top_simple(
    hls::stream<realFloat> &data_in,
    hls::stream<realFloat> &data_out,
    int sizeX,
    int sizeY
) {
#pragma HLS INTERFACE axis port=data_in
#pragma HLS INTERFACE axis port=data_out
#pragma HLS INTERFACE s_axilite port=sizeX,sizeY
#pragma HLS INTERFACE s_axilite port=return
#pragma HLS DATAFLOW

    hls::stream<cmpxFloat> data_fft("data_fft");
#pragma HLS STREAM depth=1024 variable=data_fft

    // FFT R2C
    fft_r2c_pipeline(data_in, data_fft, sizeX, sizeY);
    
    // IFFT C2R
    fft_c2r_pipeline(data_fft, data_out, sizeX, sizeY);
}