/*
 * K-Litho HLS TCC Calculation Module Header
 * TCC矩阵计算HLS模块头文件
 * 
 * 实现功能:
 * - Pupil函数查找表计算
 * - TCC矩阵累加
 * - 上三角矩阵计算优化
 */

#ifndef HLS_TCC_H
#define HLS_TCC_H

#include <hls_stream.h>
#include <complex>
#include "hls_types.h"
#include "hls_math.h"

// ============================================================
// TCC计算参数定义
// ============================================================

// 最大参数范围 (根据典型 litho 参数设置)
const int MAX_SRC_SIZE = 64;       // 光源最大尺寸 (srcSize) - 调整为64避免内存爆炸
const int MAX_TCC_DIM = 7;         // TCC矩阵最大维度 (典型值 2*Nx+1)
const int MAX_KERNEL_NUM = 20;     // SOCS核最大数量
const int MAX_PUPIL_ENTRIES = MAX_SRC_SIZE * MAX_SRC_SIZE * MAX_TCC_DIM * MAX_TCC_DIM; // Pupil查找表条目数

// ============================================================
// 查找表参数
// ============================================================

// 三角函数查找表精度配置
const int TRIG_TABLE_SIZE = 256;   // sin/cos查找表大小 (0-2π范围)
const int SQRT_TABLE_SIZE = 256;   // sqrt查找表大小 (0-1范围)

// ============================================================
// 数据结构定义
// ============================================================

/**
 * @brief 光刻参数结构体
 */
struct LithoParams {
    float lambda;      // 波长 (nm)
    float NA;          // 数值孔径
    float defocus;     // 离焦量 (nm)
    int Lx;            // X方向频域范围
    int Ly;            // Y方向频域范围
    int Nx;            // X方向TCC索引范围
    int Ny;            // Y方向TCC索引范围
    int srcSize;       // 光源尺寸
    int tccSize;       // TCC矩阵尺寸 = (2*Nx+1)*(2*Ny+1)
};

/**
 * @brief Pupil函数查找表条目
 */
struct PupilEntry {
    cmpxFloat value;   // Pupil值 (cos + j*sin)
    bool valid;        // 是否有效 (在NA范围内)
};

// ============================================================
// 三角函数查找表接口
// ============================================================

/**
 * @brief 初始化sin/cos查找表
 * @param angle_table 输出角度值表
 * @param sin_table 输出sin值表
 * @param cos_table 输出cos值表
 */
void init_trig_lut(
    float angle_table[TRIG_TABLE_SIZE],
    float sin_table[TRIG_TABLE_SIZE],
    float cos_table[TRIG_TABLE_SIZE]
);

/**
 * @brief 查找sin值
 * @param angle 输入角度 (rad)
 * @param sin_table sin查找表
 * @return sin值
 */
float lut_sin(float angle, float sin_table[TRIG_TABLE_SIZE]);

/**
 * @brief 查找cos值
 * @param angle 输入角度 (rad)
 * @param cos_table cos查找表
 * @return cos值
 */
float lut_cos(float angle, float cos_table[TRIG_TABLE_SIZE]);

/**
 * @brief 查找sqrt值 (CORDIC近似)
 * @param value 输入值 (0-1范围)
 * @return sqrt值
 */
float lut_sqrt(float value);

// ============================================================
// Pupil函数计算模块
// ============================================================

/**
 * @brief 计算单个光源点的Pupil函数
 * @param sx 光源X坐标 (归一化)
 * @param sy 光源Y坐标 (归一化)
 * @param params 光刻参数
 * @param pupil_out 输出Pupil矩阵 (MAX_TCC_SIZE x MAX_TCC_SIZE)
 */
void calc_pupil_single(
    float sx,
    float sy,
    LithoParams &params,
    PupilEntry pupil_out[MAX_TCC_SIZE * MAX_TCC_SIZE]
);

/**
 * @brief 批量计算所有光源点的Pupil函数 (DATAFLOW)
 * @param src 光源矩阵 (srcSize x srcSize)
 * @param params 光刻参数
 * @param pupil_lut 输出Pupil查找表 (srcSize*srcSize x tccSize*tccSize)
 */
void calc_pupil_batch(
    float src[MAX_SRC_SIZE * MAX_SRC_SIZE],
    LithoParams &params,
    PupilEntry pupil_lut[MAX_SRC_SIZE * MAX_SRC_SIZE * MAX_TCC_SIZE * MAX_TCC_SIZE]
);

// ============================================================
// TCC矩阵计算模块
// ============================================================

/**
 * @brief 计算TCC矩阵上三角部分
 * @param src 光源矩阵
 * @param pupil_lut Pupil查找表
 * @param params 光刻参数
 * @param tcc_out 输出TCC矩阵 (tccSize x tccSize)
 */
void calc_tcc_upper_triangle(
    float src[MAX_SRC_SIZE * MAX_SRC_SIZE],
    PupilEntry pupil_lut[MAX_SRC_SIZE * MAX_SRC_SIZE * MAX_TCC_SIZE * MAX_TCC_SIZE],
    LithoParams &params,
    cmpxFloat tcc_out[MAX_TCC_SIZE * MAX_TCC_SIZE]
);

/**
 * @brief 填充TCC矩阵下三角部分 (对称性)
 * @param tcc_inout 输入/输出TCC矩阵 (上三角已计算)
 * @param tccSize TCC矩阵尺寸
 */
void fill_tcc_lower_triangle(
    cmpxFloat tcc_inout[MAX_TCC_SIZE * MAX_TCC_SIZE],
    int tccSize
);

/**
 * @brief 完整TCC计算流程 (DATAFLOW集成)
 * @param src 光源矩阵输入流
 * @param params 光刻参数输入流
 * @param tcc_out TCC矩阵输出流
 */
void hls_calc_tcc(
    hls::stream<float> &src_in,
    hls::stream<LithoParams> &params_in,
    hls::stream<cmpxFloat> &tcc_out
);

/**
 * @brief TCC计算简化顶层接口 (AXI-Stream版本)
 * 用于HLS C仿真和综合测试
 */
void hls_calc_tcc_simple(
    float src[MAX_SRC_SIZE * MAX_SRC_SIZE],
    float lambda,
    float NA,
    float defocus,
    int Lx,
    int Ly,
    int Nx,
    int Ny,
    int srcSize,
    cmpxFloat tcc_out[MAX_TCC_SIZE * MAX_TCC_SIZE]
);

/**
 * @brief TCC计算最简化顶层接口 (固定参数版本)
 * 用于快速C仿真测试 (64x64光源, 7x7 TCC)
 */
void hls_tcc_test_top(
    float src[64 * 64],
    cmpxFloat tcc_out[7 * 7]
);

// ============================================================
// 辅助函数
// ============================================================

/**
 * @brief 计算光源点是否有效
 * @param p 光源X索引
 * @param q 光源Y索引
 * @param sh 光源半径 (srcSize/2)
 * @return 是否有效
 */
bool is_source_valid(int p, int q, int sh);

/**
 * @brief 计算频域坐标是否在NA范围内
 * @param fx 频域X坐标
 * @param fy 频域Y坐标
 * @param NA 数值孔径
 * @return 是否有效
 */
bool is_pupil_valid(float fx, float fy, float NA);

#endif // HLS_TCC_H