/*
 * K-Litho HLS FFT R2C Module
 * 实数到复数FFT变换
 * 
 * 替代原始函数: klitho_tcc.cpp:FT_r2c()
 * 参考: interface_stream/fft_top.cpp
 */

#include "../include/hls_types.h"
#include <hls_stream.h>

using namespace hls;

// ============================================================
// FFT R2C 核心模块
// ============================================================

/**
 * @brief 实数到复数2D FFT变换
 * 
 * @param dir      FFT方向 (0=正向FFT, 1=逆向IFFT)
 * @param scaling  缩放因子配置
 * @param xn_in    输入实数数据流 (已移位)
 * @param xk_out   输出复数数据流
 * @param status   状态输出
 * @param sizeX    X方向尺寸
 * @param sizeY    Y方向尺寸
 */
void hls_fft_r2c_core(
    ap_uint<1> dir,
    ap_uint<15> scaling,
    hls::stream<cmpxFloat> &xn_in,
    hls::stream<cmpxFloat> &xk_out,
    bool *status
) {
#pragma HLS INTERFACE axis port=xn_in
#pragma HLS INTERFACE axis port=xk_out
#pragma HLS INTERFACE ap_fifo depth=1 port=status
#pragma HLS PIPELINE II=1

    // 调用HLS FFT IP核
    hls::fft<fft_config_t>(xn_in, xk_out, dir, scaling, -1, status);
}

/**
 * @brief 实数输入预处理模块
 * 将实数数据转换为复数格式并送入FFT核
 * 
 * @param real_in  实数输入流
 * @param cmplx_out 复数输出流 (送入FFT核)
 * @param size     数据尺寸
 */
void real_to_complex(
    hls::stream<realFloat> &real_in,
    hls::stream<cmpxFloat> &cmplx_out,
    int size
) {
#pragma HLS INTERFACE axis port=real_in
#pragma HLS INTERFACE axis port=cmplx_out
#pragma HLS PIPELINE II=1

    for (int i = 0; i < size; i++) {
#pragma HLS LOOP_TRIPCOUNT min=1024 max=1024 avg=1024
        realFloat val = real_in.read();
        cmplx_out.write(cmpxFloat(val, 0.0f));
    }
}

/**
 * @brief FFT输出后处理模块
 * 处理FFT输出并进行频域重组
 * 
 * @param cmplx_in FFT核输出
 * @param cmplx_out 最终输出
 * @param sizeX    X方向尺寸
 * @param sizeY    Y方向尺寸
 */
void fft_output_reorder(
    hls::stream<cmpxFloat> &cmplx_in,
    hls::stream<cmpxFloat> &cmplx_out,
    int sizeX,
    int sizeY
) {
#pragma HLS INTERFACE axis port=cmplx_in
#pragma HLS INTERFACE axis port=cmplx_out
#pragma HLS PIPELINE II=1

    int sizeXh = sizeX / 2;
    int wt = (sizeX / 2) + 1;
    int AY = (sizeY + 1) / 2;
    int BY = sizeY - AY;
    float norm_factor = normalize_factor(sizeX, sizeY);

    // 临时存储 (用于频域重组)
    cmpxFloat R[wt * sizeY];
    cmpxFloat L[wt * sizeY];
#pragma HLS ARRAY_PARTITION variable=R cyclic factor=4
#pragma HLS ARRAY_PARTITION variable=L cyclic factor=4

    // 读取FFT输出到临时数组
    for (int y = 0; y < sizeY; y++) {
#pragma HLS LOOP_TRIPCOUNT min=256 max=256 avg=256
        for (int x = 0; x < wt; x++) {
#pragma HLS LOOP_TRIPCOUNT min=8 max=16 avg=12
#pragma HLS PIPELINE II=1
            R[y * wt + x] = cmplx_in.read();
        }
    }

    // 频域重组: 将FFT输出重新排列到完整的频域
    // R数组包含正频率部分, L数组包含负频率部分
    for (int y = 0; y < sizeY; y++) {
#pragma HLS LOOP_TRIPCOUNT min=256 max=256 avg=256
        for (int x = 0; x < wt; x++) {
#pragma HLS LOOP_TRIPCOUNT min=8 max=16 avg=12
#pragma HLS PIPELINE II=1
            // 构造负频率部分 (复数共轭)
            int y2 = (sizeY - 1 - y) % sizeY;
            int x2 = wt - 1 - x;
            if (x2 >= 0 && x2 < wt) {
                L[y * wt + x] = complex_conj(R[y2 * wt + x2]);
            }
        }
    }

    // 输出重组后的数据并进行归一化
    // 左半部分 (负频率)
    for (int y = 0; y < sizeY; y++) {
#pragma HLS LOOP_TRIPCOUNT min=256 max=256 avg=256
        for (int x = 0; x < sizeXh; x++) {
#pragma HLS LOOP_TRIPCOUNT min=4 max=8 avg=6
#pragma HLS PIPELINE II=1
            int idx = y * wt + (sizeXh - 1 - x);
            if (idx >= 0 && idx < wt * sizeY) {
                cmplx_out.write(L[idx] * norm_factor);
            } else {
                cmplx_out.write(cmpxFloat(0.0f, 0.0f));
            }
        }
    }

    // 右半部分 (正频率)
    for (int y = 0; y < sizeY; y++) {
#pragma HLS LOOP_TRIPCOUNT min=256 max=256 avg=256
        for (int x = 0; x < sizeXh; x++) {
#pragma HLS LOOP_TRIPCOUNT min=4 max=8 avg=6
#pragma HLS PIPELINE II=1
            int idx = y * wt + x;
            cmplx_out.write(R[idx] * norm_factor);
        }
    }
}

// ============================================================
// 顶层FFT R2C模块
// ============================================================

/**
 * @brief FFT R2C顶层接口
 * 
 * @param real_in   实数输入流 (需预先移位)
 * @param cmplx_out 复数输出流
 * @param sizeX     X方向尺寸
 * @param sizeY     Y方向尺寸
 */
void hls_fft_r2c(
    hls::stream<realFloat> &real_in,
    hls::stream<cmpxFloat> &cmplx_out,
    int sizeX,
    int sizeY
) {
#pragma HLS INTERFACE axis port=real_in
#pragma HLS INTERFACE axis port=cmplx_out
#pragma HLS INTERFACE s_axilite port=sizeX,sizeY
#pragma HLS INTERFACE s_axilite port=return
#pragma HLS DATAFLOW

    // 内部流
    hls::stream<cmpxFloat> cmplx_inter("cmplx_inter");
    hls::stream<cmpxFloat> fft_out("fft_out");
#pragma HLS STREAM depth=1024 variable=cmplx_inter
#pragma HLS STREAM depth=1024 variable=fft_out

    bool status = false;
    ap_uint<1> dir = 0;  // 正向FFT
    ap_uint<15> scaling = 0;  // 缩放配置

    // Step 1: 实数转复数
    real_to_complex(real_in, cmplx_inter, sizeX * sizeY);

    // Step 2: FFT核心计算
    hls_fft_r2c_core(dir, scaling, cmplx_inter, fft_out, &status);

    // Step 3: 输出重组
    fft_output_reorder(fft_out, cmplx_out, sizeX, sizeY);
}