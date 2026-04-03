/**
 * @file hls_socs.cpp
 * @brief SOCS光学图像计算模块实现
 * 
 * 实现:
 * - 多核循环处理 (nk个SOCS核)
 * - Kernel-Mask复数乘法 (DSP映射)
 * - 平方累加计算
 * - 循环移位输出
 */

#include "../include/hls_socs.h"
#include "../include/hls_types.h"
#include <hls_math.h>

/**
 * @brief 数组接口的循环移位 (简化版本)
 * 将频域中心移到图像中心
 */
void hls_shift_array_real(
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

/**
 * @brief SOCS核心计算流程
 * 
 * 实现多核循环计算:
 * 1. Kernel-Mask复数乘法
 * 2. 平方幅度累加
 * 3. 循环移位输出
 */
void hls_calc_socs_core(
    complex_float kernels[SOCS_MAX_KERNELS][SOCS_KERNEL_SIZE],
    float scales[SOCS_MAX_KERNELS],
    complex_float mask_fft[SOCS_MAX_LX * SOCS_MAX_LY],
    float image_out[SOCS_MAX_LX * SOCS_MAX_LY],
    int nk,
    int Lx, int Ly,
    int Nx, int Ny
) {
    // 计算尺寸参数
    int sizeX = 4 * Nx + 1;
    int sizeY = 4 * Ny + 1;
    int difX = sizeX - (2 * Nx + 1);
    int difY = sizeY - (2 * Ny + 1);
    int kernelSize = (2 * Nx + 1) * (2 * Ny + 1);
    int Lxh = Lx / 2;
    int Lyh = Ly / 2;
    
    // 本地工作数组
    complex_float product[(4 * SOCS_MAX_NX + 1) * (4 * SOCS_MAX_NY + 1)];
    float img_accum[(4 * SOCS_MAX_NX + 1) * (4 * SOCS_MAX_NY + 1)];
    float img_shifted[(4 * SOCS_MAX_NX + 1) * (4 * SOCS_MAX_NY + 1)];
    
#pragma HLS ARRAY_PARTITION variable=kernels cyclic factor=4 dim=2
#pragma HLS ARRAY_PARTITION variable=scales cyclic factor=4
#pragma HLS ARRAY_PARTITION variable=mask_fft cyclic factor=4
#pragma HLS ARRAY_PARTITION variable=product cyclic factor=4
#pragma HLS ARRAY_PARTITION variable=img_accum cyclic factor=4
    
    // 初始化累加器
    for (int i = 0; i < sizeX * sizeY; i++) {
#pragma HLS PIPELINE II=1
        img_accum[i] = 0.0f;
        product[i] = complex_float(0.0f, 0.0f);
    }
    
    // 多核循环计算
    for (int k = 0; k < nk; k++) {
        // 步骤1: Kernel-Mask复数乘法并填充到product数组
        for (int y = -Ny; y <= Ny; y++) {
            for (int x = -Nx; x <= Nx; x++) {
#pragma HLS PIPELINE II=1
                
                // Kernel索引
                int krn_idx = (y + Ny) * (2 * Nx + 1) + (x + Nx);
                
                // Mask索引 (取中央区域)
                int msk_idx = (y + Lyh) * Lx + (x + Lxh);
                
                // Product位置
                int prod_idx = (difY + y + Ny) * sizeX + difX + x + Nx;
                
                // 复数乘法: krn * msk (DSP映射)
                complex_float krn = kernels[k][krn_idx];
                complex_float msk = mask_fft[msk_idx];
                
                float real_prod = krn.real() * msk.real() - krn.imag() * msk.imag();
                float imag_prod = krn.real() * msk.imag() + krn.imag() * msk.real();
                
                product[prod_idx] = complex_float(real_prod, imag_prod);
            }
        }
        
        // 步骤2: 平方幅度累加
        // 简化版本: 直接使用product数组计算平方幅度
        float scale = scales[k];
        
        for (int i = 0; i < sizeX * sizeY; i++) {
#pragma HLS PIPELINE II=1
            
            float real_val = product[i].real();
            float imag_val = product[i].imag();
            float mag_sq = real_val * real_val + imag_val * imag_val;
            
            img_accum[i] += scale * mag_sq;
        }
    }
    
    // 步骤3: 循环移位 (将频域中心移到图像中心)
    hls_shift_array_real(img_accum, img_shifted, sizeX, sizeY, true, true);
    
    // 步骤4: 输出截取中央区域
    // 简化处理: 直接复制到输出
    int outSize = Lx * Ly;
    
    for (int i = 0; i < outSize; i++) {
#pragma HLS PIPELINE II=1
        // 简化: 如果输出尺寸大于计算尺寸,填充零
        if (i < sizeX * sizeY) {
            image_out[i] = img_shifted[i];
        } else {
            image_out[i] = 0.0f;
        }
    }
}

/**
 * @brief SOCS顶层接口 (AXI-Master)
 */
void hls_calc_socs(
    complex_float *gmem_krn,
    float *gmem_scl,
    complex_float *gmem_msk,
    float *gmem_img,
    int nk,
    int Lx, int Ly,
    int Nx, int Ny
) {
    // AXI接口配置
#pragma HLS INTERFACE m_axi port=gmem_krn depth=8192 offset=slave bundle=krn_mem
#pragma HLS INTERFACE m_axi port=gmem_scl depth=32 offset=slave bundle=scl_mem
#pragma HLS INTERFACE m_axi port=gmem_msk depth=4096 offset=slave bundle=msk_mem
#pragma HLS INTERFACE m_axi port=gmem_img depth=4096 offset=slave bundle=img_mem
#pragma HLS INTERFACE s_axilite port=nk bundle=ctrl
#pragma HLS INTERFACE s_axilite port=Lx bundle=ctrl
#pragma HLS INTERFACE s_axilite port=Ly bundle=ctrl
#pragma HLS INTERFACE s_axilite port=Nx bundle=ctrl
#pragma HLS INTERFACE s_axilite port=Ny bundle=ctrl
#pragma HLS INTERFACE s_axilite port=return bundle=ctrl
    
    // 参数范围限制
    int nk_safe = (nk > SOCS_MAX_KERNELS) ? SOCS_MAX_KERNELS : nk;
    int Lx_safe = (Lx > SOCS_MAX_LX) ? SOCS_MAX_LX : Lx;
    int Ly_safe = (Ly > SOCS_MAX_LY) ? SOCS_MAX_LY : Ly;
    int Nx_safe = (Nx > SOCS_MAX_NX) ? SOCS_MAX_NX : Nx;
    int Ny_safe = (Ny > SOCS_MAX_NY) ? SOCS_MAX_NY : Ny;
    
    int kernelSize = (2 * Nx_safe + 1) * (2 * Ny_safe + 1);
    
    // 本地缓存声明
    complex_float loc_krn[SOCS_MAX_KERNELS][SOCS_KERNEL_SIZE];
    float loc_scl[SOCS_MAX_KERNELS];
    complex_float loc_msk[SOCS_MAX_LX * SOCS_MAX_LY];
    float loc_img[SOCS_MAX_LX * SOCS_MAX_LY];
    
#pragma HLS ARRAY_PARTITION variable=loc_krn cyclic factor=4 dim=2
#pragma HLS ARRAY_PARTITION variable=loc_scl cyclic factor=4
#pragma HLS ARRAY_PARTITION variable=loc_msk cyclic factor=4
#pragma HLS ARRAY_PARTITION variable=loc_img cyclic factor=4
    
    // 预取数据
    // SOCS核数据
    for (int k = 0; k < nk_safe; k++) {
        for (int i = 0; i < kernelSize; i++) {
#pragma HLS PIPELINE II=1
            loc_krn[k][i] = gmem_krn[k * kernelSize + i];
        }
    }
    
    // 权重系数
    for (int k = 0; k < nk_safe; k++) {
#pragma HLS PIPELINE II=1
        loc_scl[k] = gmem_scl[k];
    }
    
    // 掩模数据
    int mskSize = Lx_safe * Ly_safe;
    for (int i = 0; i < mskSize; i++) {
#pragma HLS PIPELINE II=1
        loc_msk[i] = gmem_msk[i];
    }
    
    // 核心计算
    hls_calc_socs_core(loc_krn, loc_scl, loc_msk, loc_img, 
                       nk_safe, Lx_safe, Ly_safe, Nx_safe, Ny_safe);
    
    // 回写结果
    for (int i = 0; i < Lx_safe * Ly_safe; i++) {
#pragma HLS PIPELINE II=1
        gmem_img[i] = loc_img[i];
    }
}