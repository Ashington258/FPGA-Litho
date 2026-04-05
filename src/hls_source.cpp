/*
 * FPGA-Litho HLS Source Generation Module
 * 光源生成模块
 * 
 * 替代原始函数: source.cpp:createAnnular(), createDipole(), createCrossQuadrupole()
 */

#include "../include/hls_types.h"
#include <hls_stream.h>
#include <cmath>

using namespace hls;

// ============================================================
// 光源类型定义
// ============================================================

// Annular光源参数 (圆环形)
struct AnnularParams {
    float innerRadius;  // 内半径 (归一化, 0-1)
    float outerRadius;  // 外半径 (归一化, 0-1)
};

// Dipole光源参数 (双极)
struct DipoleParams {
    float radius;       // 圆半径 (归一化, 0-1)
    float offset;       // 偏移距离 (归一化, 0-1)
    bool  onXAxis;      // 是否在X轴上
};

// CrossQuadrupole光源参数 (十字四极)
struct CrossQuadrupoleParams {
    float radius;       // 圆半径 (归一化, 0-1)
    float offset;       // 偏移距离 (归一化, 0-1)
};

// Point光源参数 (点光源)
struct PointParams {
    float x;            // X位置 (归一化, -1到1)
    float y;            // Y位置 (归一化, -1到1)
};

// ============================================================
// Annular光源生成 (圆环形)
// ============================================================

/**
 * @brief 生成Annular光源
 * 在内外半径之间的圆环区域填充1, 其余区域为0
 */
void hls_source_annular(
    hls::stream<realFloat> &source_out,
    int matrixSize,
    float innerRadius,
    float outerRadius
) {
#pragma HLS INTERFACE axis port=source_out
#pragma HLS INTERFACE s_axilite port=matrixSize,innerRadius,outerRadius
#pragma HLS INTERFACE s_axilite port=return
#pragma HLS PIPELINE II=1

    int centerX = (matrixSize - 1) / 2;
    int centerY = (matrixSize - 1) / 2;
    int innerRadiusSize = innerRadius * centerX;
    int outerRadiusSize = outerRadius * centerX;
    
    // 使用平方距离避免sqrt计算
    int innerR2 = innerRadiusSize * innerRadiusSize;
    int outerR2 = outerRadiusSize * outerRadiusSize;
    
    for (int y = 0; y < matrixSize; y++) {
#pragma HLS LOOP_TRIPCOUNT min=32 max=256 avg=128
        for (int x = 0; x < matrixSize; x++) {
#pragma HLS LOOP_TRIPCOUNT min=32 max=256 avg=128
#pragma HLS PIPELINE II=1
            
            // 计算平方距离
            int dx = x - centerX;
            int dy = y - centerY;
            int dist2 = dx * dx + dy * dy;
            
            // 判断是否在圆环内
            realFloat val;
            if (dist2 >= innerR2 && dist2 <= outerR2) {
                val = 1.0f;
            } else {
                val = 0.0f;
            }
            
            source_out.write(val);
        }
    }
}

// ============================================================
// Dipole光源生成 (双极)
// ============================================================

/**
 * @brief 生成Dipole光源
 * 两个对称分布的圆形区域
 */
void hls_source_dipole(
    hls::stream<realFloat> &source_out,
    int matrixSize,
    float radius,
    float offset,
    bool onXAxis
) {
#pragma HLS INTERFACE axis port=source_out
#pragma HLS INTERFACE s_axilite port=matrixSize,radius,offset,onXAxis
#pragma HLS INTERFACE s_axilite port=return
#pragma HLS PIPELINE II=1

    int centerX = matrixSize / 2;
    int centerY = matrixSize / 2;
    int radiusSize = radius * (matrixSize - 1) / 2;
    int offsetSize = offset * (matrixSize - 1) / 2;
    
    int radius2 = radiusSize * radiusSize;
    
    for (int y = 0; y < matrixSize; y++) {
#pragma HLS LOOP_TRIPCOUNT min=32 max=256 avg=128
        for (int x = 0; x < matrixSize; x++) {
#pragma HLS LOOP_TRIPCOUNT min=32 max=256 avg=128
#pragma HLS PIPELINE II=1
            
            // 计算两个圆的中心
            int c1x, c1y, c2x, c2y;
            if (onXAxis) {
                // 在X轴上对称分布
                c1x = centerX + offsetSize;
                c1y = centerY;
                c2x = centerX - offsetSize;
                c2y = centerY;
            } else {
                // 在Y轴上对称分布
                c1x = centerX;
                c1y = centerY + offsetSize;
                c2x = centerX;
                c2y = centerY - offsetSize;
            }
            
            // 计算到两个圆心的平方距离
            int dx1 = x - c1x;
            int dy1 = y - c1y;
            int dist2_1 = dx1 * dx1 + dy1 * dy1;
            
            int dx2 = x - c2x;
            int dy2 = y - c2y;
            int dist2_2 = dx2 * dx2 + dy2 * dy2;
            
            // 判断是否在任一圆内
            realFloat val;
            if (dist2_1 <= radius2 || dist2_2 <= radius2) {
                val = 1.0f;
            } else {
                val = 0.0f;
            }
            
            source_out.write(val);
        }
    }
}

// ============================================================
// CrossQuadrupole光源生成 (十字四极)
// ============================================================

/**
 * @brief 生成CrossQuadrupole光源
 * 四个对称分布的圆形区域
 */
void hls_source_cross_quadrupole(
    hls::stream<realFloat> &source_out,
    int matrixSize,
    float radius,
    float offset
) {
#pragma HLS INTERFACE axis port=source_out
#pragma HLS INTERFACE s_axilite port=matrixSize,radius,offset
#pragma HLS INTERFACE s_axilite port=return
#pragma HLS PIPELINE II=1

    int centerX = (matrixSize - 1) / 2;
    int centerY = (matrixSize - 1) / 2;
    int radiusSize = radius * centerX;
    int offsetSize = offset * centerX;
    
    int radius2 = radiusSize * radiusSize;
    
    for (int y = 0; y < matrixSize; y++) {
#pragma HLS LOOP_TRIPCOUNT min=32 max=256 avg=128
        for (int x = 0; x < matrixSize; x++) {
#pragma HLS LOOP_TRIPCOUNT min=32 max=256 avg=128
#pragma HLS PIPELINE II=1
            
            // 四个圆的中心位置
            int cx_top    = centerX;
            int cy_top    = centerY + offsetSize;
            int cx_bottom = centerX;
            int cy_bottom = centerY - offsetSize;
            int cx_right  = centerX + offsetSize;
            int cy_right  = centerY;
            int cx_left   = centerX - offsetSize;
            int cy_left   = centerY;
            
            // 计算平方距离
            int dist2_top    = (x - cx_top)    * (x - cx_top)    + (y - cy_top)    * (y - cy_top);
            int dist2_bottom = (x - cx_bottom) * (x - cx_bottom) + (y - cy_bottom) * (y - cy_bottom);
            int dist2_right  = (x - cx_right)  * (x - cx_right)  + (y - cy_right)  * (y - cy_right);
            int dist2_left   = (x - cx_left)   * (x - cx_left)   + (y - cy_left)   * (y - cy_left);
            
            // 判断是否在任一圆内
            realFloat val;
            if (dist2_top <= radius2 || dist2_bottom <= radius2 ||
                dist2_right <= radius2 || dist2_left <= radius2) {
                val = 1.0f;
            } else {
                val = 0.0f;
            }
            
            source_out.write(val);
        }
    }
}

// ============================================================
// Point光源生成 (点光源)
// ============================================================

/**
 * @brief 生成Point光源
 * 单点光源
 */
void hls_source_point(
    hls::stream<realFloat> &source_out,
    int matrixSize,
    float px,
    float py
) {
#pragma HLS INTERFACE axis port=source_out
#pragma HLS INTERFACE s_axilite port=matrixSize,px,py
#pragma HLS INTERFACE s_axilite port=return
#pragma HLS PIPELINE II=1

    // 计算点位置对应的像素索引
    int ptXId, ptYId;
    
    // 坐标转换: 归一化坐标(-1到1) -> 矩阵索引(0到matrixSize-1)
    ptXId = (int)(px * matrixSize / 2.0f + matrixSize / 2.0f);
    ptYId = (int)(-py * matrixSize / 2.0f + matrixSize / 2.0f);  // Y轴反向
    
    // 处理边界情况
    if (ptXId < 0) ptXId = 0;
    if (ptXId >= matrixSize) ptXId = matrixSize - 1;
    if (ptYId < 0) ptYId = 0;
    if (ptYId >= matrixSize) ptYId = matrixSize - 1;
    
    for (int y = 0; y < matrixSize; y++) {
#pragma HLS LOOP_TRIPCOUNT min=32 max=256 avg=128
        for (int x = 0; x < matrixSize; x++) {
#pragma HLS LOOP_TRIPCOUNT min=32 max=256 avg=128
#pragma HLS PIPELINE II=1
            
            realFloat val;
            if (x == ptXId && y == ptYId) {
                val = 1.0f;
            } else {
                val = 0.0f;
            }
            
            source_out.write(val);
        }
    }
}

// ============================================================
// 光源归一化模块
// ============================================================

/**
 * @brief 光源归一化
 * 使所有元素之和为1
 */
void hls_source_normalize(
    hls::stream<realFloat> &source_in,
    hls::stream<realFloat> &source_out,
    int size
) {
#pragma HLS INTERFACE axis port=source_in
#pragma HLS INTERFACE axis port=source_out
#pragma HLS INTERFACE s_axilite port=size
#pragma HLS INTERFACE s_axilite port=return

    // Step 1: 计算总和
    realFloat sum = 0.0f;
    realFloat temp[MAX_IMAGE_SIZE * MAX_IMAGE_SIZE];
#pragma HLS ARRAY_PARTITION variable=temp cyclic factor=4
    
    for (int i = 0; i < size; i++) {
#pragma HLS LOOP_TRIPCOUNT min=1024 max=1024 avg=1024
#pragma HLS PIPELINE II=1
        temp[i] = source_in.read();
        sum += temp[i];
    }
    
    // Step 2: 归一化输出
    realFloat norm_factor = (sum > 0.0f) ? (1.0f / sum) : 1.0f;
    
    for (int i = 0; i < size; i++) {
#pragma HLS LOOP_TRIPCOUNT min=1024 max=1024 avg=1024
#pragma HLS PIPELINE II=1
        source_out.write(temp[i] * norm_factor);
    }
}

// ============================================================
// 顶层光源生成模块
// ============================================================

/**
 * @brief 光源生成顶层接口
 * 
 * @param source_out 归一化光源输出
 * @param matrixSize 光源矩阵尺寸
 * @param srcType    光源类型 (0=Annular, 1=Dipole, 2=CrossQuadrupole, 3=Point)
 * @param param1     参数1 (innerRadius/radius/radius/px)
 * @param param2     参数2 (outerRadius/offset/offset/py)
 * @param param3     参数3 (onXAxis, 仅Dipole使用)
 */
void hls_source_gen(
    hls::stream<realFloat> &source_out,
    int matrixSize,
    int srcType,
    float param1,
    float param2,
    int param3
) {
#pragma HLS INTERFACE axis port=source_out
#pragma HLS INTERFACE s_axilite port=matrixSize,srcType,param1,param2,param3
#pragma HLS INTERFACE s_axilite port=return
#pragma HLS DATAFLOW

    // 中间流
    hls::stream<realFloat> source_raw("source_raw");
#pragma HLS STREAM depth=1024 variable=source_raw
    
    int totalSize = matrixSize * matrixSize;
    
    // 根据类型生成光源
    switch (srcType) {
        case 0:  // Annular
            hls_source_annular(source_raw, matrixSize, param1, param2);
            break;
        case 1:  // Dipole
            hls_source_dipole(source_raw, matrixSize, param1, param2, (param3 != 0));
            break;
        case 2:  // CrossQuadrupole
            hls_source_cross_quadrupole(source_raw, matrixSize, param1, param2);
            break;
        case 3:  // Point
            hls_source_point(source_raw, matrixSize, param1, param2);
            break;
        default:
            // 默认: Annular
            hls_source_annular(source_raw, matrixSize, param1, param2);
            break;
    }
    
    // 归一化
    hls_source_normalize(source_raw, source_out, totalSize);
}