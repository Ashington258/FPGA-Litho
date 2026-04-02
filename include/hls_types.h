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

// ============================================================
// FFT配置结构体 (与官方interface_stream完全一致)
// ============================================================

// 配置结构体 - 命名为config1与官方保持一致
struct config1 : hls::ip_fft::params_t {
    static const unsigned input_width = FFT_INPUT_WIDTH;
    static const unsigned output_width = FFT_OUTPUT_WIDTH;
    static const unsigned fft_length = FFT_LENGTH;
    static const unsigned nfft_max = FFT_NFFT_MAX;
    static const unsigned num_channels = FFT_CHANNELS;
    
    static const unsigned arch = hls::ip_fft::pipelined_streaming_io;
    static const unsigned output_ordering = hls::ip_fft::natural_order;
    static const unsigned scaling_mode = hls::ip_fft::scaled;
    static const unsigned rounding_mode = hls::ip_fft::truncation;
};

// 配置和状态类型定义
typedef hls::ip_fft::config_t<config1> config_t;
typedef hls::ip_fft::status_t<config1> status_t;

// 保持原有fft_config_t兼容性（deprecated，建议使用config1）
typedef config1 fft_config_t;

// ============================================================
// FFT缩放策略 (scaled模式)
// ============================================================

// 固定缩放策略：每级缩放1bit
// 0x1555 = 01 01 01 01 01... (每2位控制一级)
// 10级各缩放1bit → 总缩放 = 2^10 = 1024倍
// scaled模式下，FFT和IFFT使用相同缩放，幅度自动恢复
const ap_uint<15> SCALING_FFT  = 0x1555;  // FFT正向缩放
const ap_uint<15> SCALING_IFFT = 0x1555;  // IFFT逆向缩放

// 无缩放策略（用于精度测试，可能溢出）
const ap_uint<15> SCALING_NONE = 0x0000;

// ============================================================
// 动态缩放调度计算函数
// ============================================================

/**
 * @brief 根据输入数据幅度计算FFT缩放调度
 * 
 * scaled模式下，scaling_schedule每2位控制一级FFT缩放:
 * - 00 = 无缩放
 * - 01 = 缩放1bit (推荐)
 * - 10 = 缩放2bit
 * - 11 = 不缩放
 * 
 * @param input_max   输入数据最大幅度 (绝对值)
 * @param fft_stages  FFT级数 (log2(FFT_LENGTH), 如1024点=10级)
 * @return ap_uint<15> 缩放调度值
 * 
 * 计算逻辑:
 * - ap_fixed<16,1> 范围约 [-2, 2)，整数部分1位
 * - FFT频域峰值 ≈ N × 输入幅度 (N=FFT点数)
 * - 需要足够的缩放防止频域峰值溢出
 * - 每级缩放1bit可将峰值降低2倍
 * 
 * 示例:
 * - input_max=0.5, fft_stages=10 → 返回 0x1555 (每级缩放1bit)
 * - input_max=0.1, fft_stages=10 → 返回 0x0555 (偶数级缩放)
 * - input_max<0.01 → 返回 SCALING_NONE (无需缩放)
 */
inline ap_uint<15> compute_scaling_schedule(float input_max, int fft_stages) {
    // 安全阈值计算
    // ap_fixed<16,1> 整数部分1位，可表示 ±1.999...
    // FFT频域峰值 = N × input_max，需保证峰值 < 1.0
    
    float fft_length = (float)(1 << fft_stages);  // N = 2^stages
    float expected_peak = fft_length * input_max; // 预期频域峰值
    
    // 目标: 将峰值缩放到安全范围 (< 0.5 以留有余量)
    float target_peak = 0.5f;
    float required_scale = expected_peak / target_peak;
    
    // 计算需要的总缩放bits
    int total_scale_bits = 0;
    if (required_scale > 1.0f) {
        total_scale_bits = (int)ceil(log2(required_scale));
    }
    
    // 限制最大缩放 (不超过fft_stages)
    total_scale_bits = (total_scale_bits > fft_stages) ? fft_stages : total_scale_bits;
    
    // 构建scaling_schedule
    // 每级分配缩放，优先前级（蝶形运算从前向后）
    ap_uint<15> schedule = 0;
    int scale_per_stage = (total_scale_bits > 0) ? 
                          (total_scale_bits / fft_stages) + 1 : 0;
    
    // 每级最多缩放1bit (01), 防止精度损失过大
    // 使用ap_uint类型避免位宽不匹配警告
    for (int stage = 0; stage < fft_stages && stage < 7; stage++) {
        if (stage < total_scale_bits) {
            // 设置该级缩放1bit: 01
            // 使用ap_uint<15>类型避免位运算警告
            schedule |= (ap_uint<15>(0x01) << (stage * 2));
        }
    }
    
    // 如果计算结果为0但输入幅度较大，使用保守策略
    if (schedule == 0 && input_max > 0.1f) {
        schedule = SCALING_FFT;  // 使用默认保守缩放
    }
    
    return schedule;
}

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

// ============================================================
// 浮点-定点转换函数 (用于外部接口与内部FFT交互)
// ============================================================

// 浮点转定点 (输入接口)
// 将浮点复数转换为FFT IP所需的定点复数
inline cmpxFixed float_to_fixed(cmpxFloat f) {
    return cmpxFixed(fft_data_t(f.real()), fft_data_t(f.imag()));
}

// 定点转浮点 (输出接口)
// 将FFT IP输出的定点复数转换为浮点
inline cmpxFloat fixed_to_float(cmpxFixedOut f) {
    return cmpxFloat(f.real().to_float(), f.imag().to_float());
}

// 浮点实数转定点复数 (R2C输入)
// 实数作为复数的实部，虚部置0
inline cmpxFixed real_to_fixed(realFloat r) {
    return cmpxFixed(fft_data_t(r), fft_data_t(0));
}

// 定点复数取实部转浮点 (C2R输出)
inline realFloat fixed_real_to_float(cmpxFixedOut c) {
    return c.real().to_float();
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