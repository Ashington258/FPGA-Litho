/*
 * FPGA-Litho HLS Shift Module
 * 数据循环移位模块
 * 
 * 替代原始函数: klitho_tcc.cpp:myShift()
 */

#include "../include/hls_types.h"
#include <hls_stream.h>

using namespace hls;

// ============================================================
// 2D循环移位模块
// ============================================================

/**
 * @brief 2D数据循环移位
 * 将数据中心移到角落或反向操作
 * 
 * @param in       输入数据流
 * @param out      输出数据流
 * @param sizeX    X方向尺寸
 * @param sizeY    Y方向尺寸
 * @param shiftTypeX X方向移位类型 (true: sizeX/2, false: (sizeX+1)/2)
 * @param shiftTypeY Y方向移位类型
 */
template<typename T>
void hls_shift_2d(
    hls::stream<T> &in,
    hls::stream<T> &out,
    int sizeX,
    int sizeY,
    bool shiftTypeX,
    bool shiftTypeY
) {
#pragma HLS INTERFACE axis port=in
#pragma HLS INTERFACE axis port=out
#pragma HLS INTERFACE s_axilite port=sizeX,sizeY,shiftTypeX,shiftTypeY
#pragma HLS INTERFACE s_axilite port=return

    // 计算移位量
    int xh = shiftTypeX ? (sizeX / 2) : ((sizeX + 1) / 2);
    int yh = shiftTypeY ? (sizeY / 2) : ((sizeY + 1) / 2);

    // 读取输入数据到临时数组
    T data_in[MAX_IMAGE_SIZE * MAX_IMAGE_SIZE];
#pragma HLS ARRAY_PARTITION variable=data_in cyclic factor=4 dim=1

    // Step 1: 读取输入
    for (int y = 0; y < sizeY; y++) {
#pragma HLS LOOP_TRIPCOUNT min=256 max=512 avg=256
        for (int x = 0; x < sizeX; x++) {
#pragma HLS LOOP_TRIPCOUNT min=256 max=512 avg=256
#pragma HLS PIPELINE II=1
            data_in[y * sizeX + x] = in.read();
        }
    }

    // Step 2: 循环移位
    // 使用硬件友好的索引计算
    T data_out[MAX_IMAGE_SIZE * MAX_IMAGE_SIZE];
#pragma HLS ARRAY_PARTITION variable=data_out cyclic factor=4 dim=1

    for (int y = 0; y < sizeY; y++) {
#pragma HLS LOOP_TRIPCOUNT min=256 max=512 avg=256
        for (int x = 0; x < sizeX; x++) {
#pragma HLS LOOP_TRIPCOUNT min=256 max=512 avg=256
#pragma HLS PIPELINE II=1
            
            // 计算移位后的目标索引
            int sy, sx;
            
            // Y方向移位
            int y_temp = y + yh;
            if (y_temp >= sizeY) {
                sy = y_temp - sizeY;
            } else {
                sy = y_temp;
            }
            
            // X方向移位
            int x_temp = x + xh;
            if (x_temp >= sizeX) {
                sx = x_temp - sizeX;
            } else {
                sx = x_temp;
            }
            
            // 写入目标位置
            data_out[sy * sizeX + sx] = data_in[y * sizeX + x];
        }
    }

    // Step 3: 输出数据
    for (int y = 0; y < sizeY; y++) {
#pragma HLS LOOP_TRIPCOUNT min=256 max=512 avg=256
        for (int x = 0; x < sizeX; x++) {
#pragma HLS LOOP_TRIPCOUNT min=256 max=512 avg=256
#pragma HLS PIPELINE II=1
            out.write(data_out[y * sizeX + x]);
        }
    }
}

// ============================================================
// 具体类型的实例化
// ============================================================

/**
 * @brief 实数数据循环移位
 */
void hls_shift_real(
    hls::stream<realFloat> &in,
    hls::stream<realFloat> &out,
    int sizeX,
    int sizeY,
    bool shiftTypeX,
    bool shiftTypeY
) {
#pragma HLS INTERFACE axis port=in
#pragma HLS INTERFACE axis port=out
#pragma HLS INTERFACE s_axilite port=sizeX,sizeY,shiftTypeX,shiftTypeY
#pragma HLS INTERFACE s_axilite port=return

    // 直接调用模板函数
    hls_shift_2d<realFloat>(in, out, sizeX, sizeY, shiftTypeX, shiftTypeY);
}

/**
 * @brief 复数数据循环移位
 */
void hls_shift_complex(
    hls::stream<cmpxFloat> &in,
    hls::stream<cmpxFloat> &out,
    int sizeX,
    int sizeY,
    bool shiftTypeX,
    bool shiftTypeY
) {
#pragma HLS INTERFACE axis port=in
#pragma HLS INTERFACE axis port=out
#pragma HLS INTERFACE s_axilite port=sizeX,sizeY,shiftTypeX,shiftTypeY
#pragma HLS INTERFACE s_axilite port=return

    // 直接调用模板函数
    hls_shift_2d<cmpxFloat>(in, out, sizeX, sizeY, shiftTypeX, shiftTypeY);
}

// ============================================================
// 反向移位模块 (用于IFFT输出后处理)
// ============================================================

/**
 * @brief 反向循环移位
 * 将数据从角落移回中心
 * 
 * @param in       输入数据流
 * @param out      输出数据流
 * @param sizeX    X方向尺寸
 * @param sizeY    Y方向尺寸
 */
void hls_shift_inverse_real(
    hls::stream<realFloat> &in,
    hls::stream<realFloat> &out,
    int sizeX,
    int sizeY
) {
#pragma HLS INTERFACE axis port=in
#pragma HLS INTERFACE axis port=out
#pragma HLS INTERFACE s_axilite port=sizeX,sizeY
#pragma HLS INTERFACE s_axilite port=return

    // 反向移位使用相反的参数
    // 如果正向移位使用 (sizeX/2, sizeY/2)
    // 反向移位使用 ((sizeX+1)/2, (sizeY+1)/2)
    bool shiftTypeX = false;  // 使用 (sizeX+1)/2
    bool shiftTypeY = false;  // 使用 (sizeY+1)/2
    
    hls_shift_2d<realFloat>(in, out, sizeX, sizeY, shiftTypeX, shiftTypeY);
}

void hls_shift_inverse_complex(
    hls::stream<cmpxFloat> &in,
    hls::stream<cmpxFloat> &out,
    int sizeX,
    int sizeY
) {
#pragma HLS INTERFACE axis port=in
#pragma HLS INTERFACE axis port=out
#pragma HLS INTERFACE s_axilite port=sizeX,sizeY
#pragma HLS INTERFACE s_axilite port=return

    bool shiftTypeX = false;
    bool shiftTypeY = false;
    
    hls_shift_2d<cmpxFloat>(in, out, sizeX, sizeY, shiftTypeX, shiftTypeY);
}