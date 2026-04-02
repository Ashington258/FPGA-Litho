/*
 * K-Litho HLS Mask Generation Module
 * 掩模生成模块
 * 
 * 替代原始函数: mask.cpp:generateLineSpace(), createMask()
 */

#include "../include/hls_types.h"
#include <hls_stream.h>

using namespace hls;

// ============================================================
// 掩模类型定义
// ============================================================

// LineSpace掩模参数
struct LineSpaceParams {
    bool isHorizontal;   // 是否水平方向
    int lineWidth;       // 线宽 (像素)
    int spaceWidth;      // 空隙宽 (像素)
};

// ============================================================
// LineSpace掩模生成
// ============================================================

/**
 * @brief 生成LineSpace掩模
 * 周期性的线条-空隙图案
 */
void hls_mask_linespace(
    hls::stream<realFloat> &mask_out,
    int sizeX,
    int sizeY,
    bool isHorizontal,
    int lineWidth,
    int spaceWidth
) {
#pragma HLS INTERFACE axis port=mask_out
#pragma HLS INTERFACE s_axilite port=sizeX,sizeY,isHorizontal,lineWidth,spaceWidth
#pragma HLS INTERFACE s_axilite port=return
#pragma HLS PIPELINE II=1

    int period = lineWidth + spaceWidth;
    
    for (int y = 0; y < sizeY; y++) {
#pragma HLS LOOP_TRIPCOUNT min=32 max=256 avg=128
        for (int x = 0; x < sizeX; x++) {
#pragma HLS LOOP_TRIPCOUNT min=32 max=256 avg=128
#pragma HLS PIPELINE II=1
            
            realFloat val;
            
            if (isHorizontal) {
                // 水平线条: Y方向周期
                if ((y % period) < lineWidth) {
                    val = 1.0f;  // 线条区域
                } else {
                    val = 0.0f;  // 空隙区域
                }
            } else {
                // 垂直线条: X方向周期
                if ((x % period) < lineWidth) {
                    val = 1.0f;  // 线条区域
                } else {
                    val = 0.0f;  // 空隙区域
                }
            }
            
            mask_out.write(val);
        }
    }
}

// ============================================================
// 嵌入大尺寸掩模
// ============================================================

/**
 * @brief 将小掩模嵌入到大尺寸掩模中心
 * 
 * @param mask_in   输入小掩模
 * @param mask_out  输出大掩模 (嵌入后)
 * @param smallX    小掩模X尺寸
 * @param smallY    小掩模Y尺寸
 * @param largeX    大掩模X尺寸
 * @param largeY    大掩模Y尺寸
 * @param dose      剂量因子
 */
void hls_mask_embed(
    hls::stream<realFloat> &mask_in,
    hls::stream<realFloat> &mask_out,
    int smallX,
    int smallY,
    int largeX,
    int largeY,
    realFloat dose
) {
#pragma HLS INTERFACE axis port=mask_in
#pragma HLS INTERFACE axis port=mask_out
#pragma HLS INTERFACE s_axilite port=smallX,smallY,largeX,largeY,dose
#pragma HLS INTERFACE s_axilite port=return
#pragma HLS PIPELINE II=1

    int offsetY = (largeY - smallY) / 2;
    int offsetX = (largeX - smallX) / 2;
    
    // 读取小掩模到临时数组
    realFloat small_mask[smallX * smallY];
#pragma HLS ARRAY_PARTITION variable=small_mask cyclic factor=4
    
    for (int i = 0; i < smallX * smallY; i++) {
#pragma HLS LOOP_TRIPCOUNT min=256 max=1024 avg=512
#pragma HLS PIPELINE II=1
        small_mask[i] = mask_in.read();
    }
    
    // 生成大掩模
    for (int y = 0; y < largeY; y++) {
#pragma HLS LOOP_TRIPCOUNT min=64 max=512 avg=256
        for (int x = 0; x < largeX; x++) {
#pragma HLS LOOP_TRIPCOUNT min=64 max=512 avg=256
#pragma HLS PIPELINE II=1
            
            realFloat val;
            
            // 判断是否在小掩模范围内
            int smallYIdx = y - offsetY;
            int smallXIdx = x - offsetX;
            
            if (smallYIdx >= 0 && smallYIdx < smallY &&
                smallXIdx >= 0 && smallXIdx < smallX) {
                // 在范围内: 取小掩模值并乘以剂量
                val = small_mask[smallYIdx * smallX + smallXIdx] * dose;
            } else {
                // 超出范围: 填0
                val = 0.0f;
            }
            
            mask_out.write(val);
        }
    }
}

// ============================================================
// 简单矩形掩模生成
// ============================================================

/**
 * @brief 生成矩形掩模
 * 简单的矩形图案
 */
void hls_mask_rectangle(
    hls::stream<realFloat> &mask_out,
    int sizeX,
    int sizeY,
    int rectWidth,
    int rectHeight,
    int centerX,
    int centerY
) {
#pragma HLS INTERFACE axis port=mask_out
#pragma HLS INTERFACE s_axilite port=sizeX,sizeY,rectWidth,rectHeight,centerX,centerY
#pragma HLS INTERFACE s_axilite port=return
#pragma HLS PIPELINE II=1

    int startX = centerX - rectWidth / 2;
    int startY = centerY - rectHeight / 2;
    int endX = startX + rectWidth;
    int endY = startY + rectHeight;
    
    for (int y = 0; y < sizeY; y++) {
#pragma HLS LOOP_TRIPCOUNT min=32 max=256 avg=128
        for (int x = 0; x < sizeX; x++) {
#pragma HLS LOOP_TRIPCOUNT min=32 max=256 avg=128
#pragma HLS PIPELINE II=1
            
            realFloat val;
            
            if (x >= startX && x < endX && y >= startY && y < endY) {
                val = 1.0f;  // 矩形内部
            } else {
                val = 0.0f;  // 矩形外部
            }
            
            mask_out.write(val);
        }
    }
}

// ============================================================
// 交叉线条掩模生成
// ============================================================

/**
 * @brief 生成交叉线条掩模 (十字形)
 */
void hls_mask_cross(
    hls::stream<realFloat> &mask_out,
    int sizeX,
    int sizeY,
    int lineWidth,
    bool isHorizontal,
    bool isVertical
) {
#pragma HLS INTERFACE axis port=mask_out
#pragma HLS INTERFACE s_axilite port=sizeX,sizeY,lineWidth,isHorizontal,isVertical
#pragma HLS INTERFACE s_axilite port=return
#pragma HLS PIPELINE II=1

    int centerX = sizeX / 2;
    int centerY = sizeY / 2;
    int halfWidth = lineWidth / 2;
    
    for (int y = 0; y < sizeY; y++) {
#pragma HLS LOOP_TRIPCOUNT min=32 max=256 avg=128
        for (int x = 0; x < sizeX; x++) {
#pragma HLS LOOP_TRIPCOUNT min=32 max=256 avg=128
#pragma HLS PIPELINE II=1
            
            realFloat val = 0.0f;
            
            // 检查是否在水平线范围内
            if (isHorizontal && abs(y - centerY) < halfWidth) {
                val = 1.0f;
            }
            
            // 检查是否在垂直线范围内
            if (isVertical && abs(x - centerX) < halfWidth) {
                val = 1.0f;
            }
            
            mask_out.write(val);
        }
    }
}

// ============================================================
// 顶层掩模生成模块
// ============================================================

/**
 * @brief 掩模生成顶层接口
 * 
 * @param mask_out 掩模输出
 * @param sizeX    X方向尺寸
 * @param sizeY    Y方向尺寸
 * @param maskType 掩模类型 (0=LineSpace, 1=Rectangle, 2=Cross)
 * @param param1   参数1 (lineWidth/rectWidth/lineWidth)
 * @param param2   参数2 (spaceWidth/rectHeight/isHorizontal)
 * @param param3   参数3 (isHorizontal/centerX/isVertical)
 */
void hls_mask_gen(
    hls::stream<realFloat> &mask_out,
    int sizeX,
    int sizeY,
    int maskType,
    int param1,
    int param2,
    int param3
) {
#pragma HLS INTERFACE axis port=mask_out
#pragma HLS INTERFACE s_axilite port=sizeX,sizeY,maskType,param1,param2,param3
#pragma HLS INTERFACE s_axilite port=return
#pragma HLS PIPELINE II=1
    
    switch (maskType) {
        case 0:  // LineSpace
            hls_mask_linespace(mask_out, sizeX, sizeY, (param3 != 0), param1, param2);
            break;
        case 1:  // Rectangle
            hls_mask_rectangle(mask_out, sizeX, sizeY, param1, param2, sizeX/2, sizeY/2);
            break;
        case 2:  // Cross
            hls_mask_cross(mask_out, sizeX, sizeY, param1, (param2 != 0), (param3 != 0));
            break;
        default:
            hls_mask_linespace(mask_out, sizeX, sizeY, false, param1, param2);
            break;
    }
}

// ============================================================
// 带嵌入的掩模生成
// ============================================================

/**
 * @brief 带嵌入功能的掩模生成
 * 在大尺寸掩模中心嵌入生成的图案
 */
void hls_mask_gen_embedded(
    hls::stream<realFloat> &mask_out,
    int smallX,
    int smallY,
    int largeX,
    int largeY,
    int maskType,
    int param1,
    int param2,
    int param3,
    realFloat dose
) {
#pragma HLS INTERFACE axis port=mask_out
#pragma HLS INTERFACE s_axilite port=smallX,smallY,largeX,largeY,maskType,param1,param2,param3,dose
#pragma HLS INTERFACE s_axilite port=return
#pragma HLS DATAFLOW

    // 中间流
    hls::stream<realFloat> mask_small("mask_small");
#pragma HLS STREAM depth=512 variable=mask_small
    
    // Step 1: 生成小掩模
    hls_mask_gen(mask_small, smallX, smallY, maskType, param1, param2, param3);
    
    // Step 2: 嵌入到大尺寸并应用剂量
    hls_mask_embed(mask_small, mask_out, smallX, smallY, largeX, largeY, dose);
}