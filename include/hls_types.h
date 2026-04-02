/*
 * K-Litho HLS Types Definition
 * Copyright 2026
 * 
 * 定义HLS工程使用的数据类型和常量
 */

#ifndef HLS_TYPES_H
#define HLS_TYPES_H

#include <complex>
#include <cmath>
#include "ap_fixed.h"
#include "hls_fft.h"

// ============================================================
// 基础数据类型定义
// ============================================================

// 浮点类型 (用于外部接口和数据处理)
typedef std::complex<float> cmpxFloat;
typedef float realFloat;

// 定点数类型 (用于资源优化测试)
// 格式: ap_fixed<W, I> 其中 W=总位宽, I=整数位宽
typedef ap_fixed<32, 16> fixed32_t;   // 32位定点数, 16位整数部分
typedef ap_fixed<24, 12> fixed24_t;   // 24位定点数, 12位整数部分
typedef ap_fixed<16, 8>  fixed16_t;   // 16位定点数, 8位整数部分

// 定点复数类型
typedef std::complex<fixed32_t> cmpxFixed32;
typedef std::complex<fixed24_t> cmpxFixed24;

// ============================================================
// FFT参数配置 (完全匹配 interface_stream/fft_top.h)
// ============================================================

// 可配置参数 - 完全匹配interface_stream/fft_top.h
const char FFT_INPUT_WIDTH                     = 16;  // 输入数据位宽 (定点数)
const char FFT_OUTPUT_WIDTH                    = FFT_INPUT_WIDTH;  // 输出位宽=输入位宽
const char FFT_STATUS_WIDTH                    = 8;
const char FFT_CHANNELS                        = 1;
const int  FFT_LENGTH                          = 1024;
const char FFT_NFFT_MAX                        = 10;
const bool FFT_HAS_NFFT                        = 0;
const hls::ip_fft::arch FFT_ARCH               = hls::ip_fft::pipelined_streaming_io;
const char FFT_TWIDDLE_WIDTH                   = 16;
const hls::ip_fft::ordering FFT_OUTPUT_ORDER   = hls::ip_fft::natural_order;
const bool FFT_OVFLO                           = 1;
const bool FFT_CYCLIC_PREFIX_INSERTION         = 0;
const bool FFT_XK_INDEX                        = 0;
const hls::ip_fft::scaling FFT_SCALING         = hls::ip_fft::scaled;    // scaled模式配合正确缩放
const hls::ip_fft::rounding FFT_ROUNDING       = hls::ip_fft::truncation;

// 存储配置 (not configurable yet)
const hls::ip_fft::mem FFT_MEM_DATA            = hls::ip_fft::block_ram;
const hls::ip_fft::mem FFT_MEM_PHASE_FACTORS   = hls::ip_fft::block_ram;
const hls::ip_fft::mem FFT_MEM_REORDER         = hls::ip_fft::block_ram;
const char FFT_STAGES_BLOCK_RAM                = 4;
const bool FFT_MEM_OPTIONS_HYBRID              = 0;
const hls::ip_fft::opt FFT_COMPLEX_MULT_TYPE   = hls::ip_fft::use_luts;
const hls::ip_fft::opt FFT_BUTTERFLY_TYPE      = hls::ip_fft::use_luts;

// FFT定点数据类型 (scaled模式输入输出位宽相同)
// 输入/输出类型: ap_fixed<WIDTH, 1> - 1位整数部分，适合归一化数据
// scaled模式通过每级缩放防止溢出，输入输出位宽保持一致
typedef ap_fixed<FFT_INPUT_WIDTH, 1> fft_data_in_t;
typedef ap_fixed<FFT_OUTPUT_WIDTH, FFT_OUTPUT_WIDTH-FFT_INPUT_WIDTH+1> fft_data_out_t;  // scaled模式输出位宽=输入
typedef std::complex<fft_data_in_t> cmpxFixedIn;
typedef std::complex<fft_data_out_t> cmpxFixedOut;

// 保持原有类型定义兼容性
typedef fft_data_in_t fft_data_t;
typedef cmpxFixedIn cmpxFixed;

// FFT配置结构体 (scaled模式)
struct fft_config_t : hls::ip_fft::params_t {
    static const unsigned input_width = FFT_INPUT_WIDTH;
    static const unsigned output_width = FFT_OUTPUT_WIDTH;
    static const unsigned output_ordering = hls::ip_fft::natural_order;
};

typedef hls::ip_fft::config_t<fft_config_t> config_t;
typedef hls::ip_fft::status_t<fft_config_t> status_t;

// ============================================================
// Litho参数常量
// ============================================================

// 光刻模拟参数范围
const float LAMBDA_MIN     = 13.5f;    // EUV波长 (nm)
const float LAMBDA_MAX     = 365.0f;   // UV波长 (nm)
const float NA_MIN         = 0.1f;     // 最小数值孔径
const float NA_MAX         = 1.35f;    // 最大数值孔径
const float DEFOCUS_MIN    = -500.0f;  // 最小离焦 (nm)
const float DEFOCUS_MAX    = 500.0f;   // 最大离焦 (nm)

// 图像尺寸参数
const int MAX_IMAGE_SIZE   = 1024;     // 最大图像尺寸
const int MAX_TCC_SIZE     = 256;      // TCC矩阵最大维度
const int MAX_KERNELS      = 16;       // 最大SOCS核数量

// ============================================================
// 辅助宏定义
// ============================================================

// 循环优化宏
#define UNROLL_FACTOR_4  4
#define UNROLL_FACTOR_8  8
#define UNROLL_FACTOR_16 16

// 数组分区宏
#define PARTITION_COMPLETE  complete
#define PARTITION_CYCLIC_4  cyclic factor=4
#define PARTITION_CYCLIC_8  cyclic factor=8
#define PARTITION_BLOCK_4   block factor=4

// ============================================================
// 辅助函数
// ============================================================

// 计算归一化因子
inline float normalize_factor(int sizeX, int sizeY) {
    return 1.0f / (sizeX * sizeY);
}

// 计算循环移位索引
inline int shift_index(int idx, int shift, int size) {
    int result = idx + shift;
    if (result >= size) result -= size;
    if (result < 0) result += size;
    return result;
}

// 复数乘法 (用于DSP优化)
inline cmpxFloat complex_mult(cmpxFloat a, cmpxFloat b) {
    // a * b = (ar*br - ai*bi) + i(ar*bi + ai*br)
    float ar = a.real(), ai = a.imag();
    float br = b.real(), bi = b.imag();
    return cmpxFloat(ar*br - ai*bi, ar*bi + ai*br);
}

// 复数共轭
inline cmpxFloat complex_conj(cmpxFloat a) {
    return cmpxFloat(a.real(), -a.imag());
}

#endif // HLS_TYPES_H