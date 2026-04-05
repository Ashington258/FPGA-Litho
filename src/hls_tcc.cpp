/*
 * FPGA-Litho HLS TCC Calculation Module Implementation
 * TCC矩阵计算HLS模块实现
 * 
 * 核心优化策略:
 * 1. 三角函数查找表 (cos/sin)
 * 2. sqrt CORDIC近似
 * 3. 循环展开 UNROLL
 * 4. 流水线化 PIPELINE II=1
 * 5. 数组分区 ARRAY_PARTITION
 */

#include "../include/hls_tcc.h"
#include <cmath>

// ============================================================
// 三角函数查找表实现
// ============================================================

/**
 * @brief 初始化sin/cos查找表
 * 使用静态数组避免每次调用时重新计算
 */
void init_trig_lut(
    float angle_table[TRIG_TABLE_SIZE],
    float sin_table[TRIG_TABLE_SIZE],
    float cos_table[TRIG_TABLE_SIZE]
) {
    // 编译时初始化查找表
    // HLS会将此转换为ROM
    for (int i = 0; i < TRIG_TABLE_SIZE; i++) {
        #pragma HLS UNROLL
        float angle = (float)i * 2.0f * M_PI / (float)TRIG_TABLE_SIZE;
        angle_table[i] = angle;
        sin_table[i] = std::sin(angle);
        cos_table[i] = std::cos(angle);
    }
}

/**
 * @brief 查找sin值 (线性插值)
 * @param angle 输入角度 (rad)
 * @param sin_table sin查找表 (可选，nullptr时使用std::sin)
 * @return sin值
 */
float lut_sin(float angle, float sin_table[TRIG_TABLE_SIZE]) {
    #pragma HLS INLINE
    
    // 如果查找表为空，使用标准函数
    if (sin_table == nullptr) {
        return std::sin(angle);
    }
    
    // 将角度映射到 [0, 2π) 范围
    float angle_norm = angle;
    while (angle_norm < 0) angle_norm += 2.0f * M_PI;
    while (angle_norm >= 2.0f * M_PI) angle_norm -= 2.0f * M_PI;
    
    // 计算查找表索引
    float idx_f = angle_norm * (float)TRIG_TABLE_SIZE / (2.0f * M_PI);
    int idx0 = (int)idx_f;
    int idx1 = (idx0 + 1) % TRIG_TABLE_SIZE;
    float frac = idx_f - (float)idx0;
    
    // 线性插值
    float val0 = sin_table[idx0];
    float val1 = sin_table[idx1];
    return val0 + frac * (val1 - val0);
}

/**
 * @brief 查找cos值 (线性插值)
 * @param angle 输入角度 (rad)
 * @param cos_table cos查找表 (可选，nullptr时使用std::cos)
 * @return cos值
 */
float lut_cos(float angle, float cos_table[TRIG_TABLE_SIZE]) {
    #pragma HLS INLINE
    
    // 如果查找表为空，使用标准函数
    if (cos_table == nullptr) {
        return std::cos(angle);
    }
    
    // 将角度映射到 [0, 2π) 范围
    float angle_norm = angle;
    while (angle_norm < 0) angle_norm += 2.0f * M_PI;
    while (angle_norm >= 2.0f * M_PI) angle_norm -= 2.0f * M_PI;
    
    // 计算查找表索引
    float idx_f = angle_norm * (float)TRIG_TABLE_SIZE / (2.0f * M_PI);
    int idx0 = (int)idx_f;
    int idx1 = (idx0 + 1) % TRIG_TABLE_SIZE;
    float frac = idx_f - (float)idx0;
    
    // 线性插值
    float val0 = cos_table[idx0];
    float val1 = cos_table[idx1];
    return val0 + frac * (val1 - val0);
}

/**
 * @brief sqrt查找表 (CORDIC近似)
 * 简化实现：使用分段线性近似
 */
float lut_sqrt(float value) {
    #pragma HLS INLINE
    
    // 分段线性近似 (0-1范围)
    // sqrt(x) ≈ 1.0 - 0.5*(1-x) for x close to 1
    //         ≈ 0.5 + 0.5*x for x close to 0
    
    if (value <= 0.0f) return 0.0f;
    if (value >= 1.0f) return 1.0f;
    
    // 简化近似: sqrt(x) ≈ x^0.5
    // 使用HLS内置sqrt函数 (会映射到CORDIC IP)
    return hls::sqrt(value);
}

// ============================================================
// Pupil函数计算模块
// ============================================================

/**
 * @brief 计算单个光源点的Pupil函数
 * 优化点:
 * 1. 使用查找表替代cos/sin
 * 2. 循环展开减少II
 * 3. 提前计算常量避免重复计算
 */
void calc_pupil_single(
    float sx,
    float sy,
    LithoParams &params,
    PupilEntry pupil_out[MAX_TCC_DIM * MAX_TCC_DIM]
) {
    #pragma HLS INLINE
    
    // 预计算常量
    float dz = params.defocus / (params.NA * params.NA / params.lambda);
    float k = 2.0f * M_PI / params.lambda;
    float Lx_norm = params.Lx * params.NA / params.lambda;
    float Ly_norm = params.Ly * params.NA / params.lambda;
    float NA2 = params.NA * params.NA;
    
    // 计算频域范围
    int nxMin = (int)((-1.0f - sx) * Lx_norm);
    int nxMax = (int)(( 1.0f - sx) * Lx_norm);
    int nyMin = (int)((-1.0f - sy) * Ly_norm);
    int nyMax = (int)(( 1.0f - sy) * Ly_norm);
    
    // 初始化输出
    for (int i = 0; i < MAX_TCC_DIM * MAX_TCC_DIM; i++) {
        #pragma HLS UNROLL factor=8
        pupil_out[i].valid = false;
        pupil_out[i].value = cmpxFloat(0.0f, 0.0f);
    }
    
    // 计算Pupil函数
    for (int ny = nyMin; ny <= nyMax; ny++) {
        #pragma HLS LOOP_FLATTEN
        for (int nx = nxMin; nx <= nxMax; nx++) {
            #pragma HLS PIPELINE II=1
            
            // 计算频域坐标
            float fx = (float)nx / Lx_norm + sx;
            float fy = (float)ny / Ly_norm + sy;
            float rho2 = fx * fx + fy * fy;
            
            // 检查是否在NA范围内
            if (rho2 <= 1.0f) {
                // 计算Pupil相位
                float arg = dz * k * lut_sqrt(1.0f - rho2 * NA2);
                
                // 使用std::cos/sin (查找表版本需要全局表，这里简化为直接计算)
                float cos_val = std::cos(arg);
                float sin_val = std::sin(arg);
                
                // 存储结果
                int idx = (ny + params.Ny) * (params.Nx * 2 + 1) + (nx + params.Nx);
                pupil_out[idx].valid = true;
                pupil_out[idx].value = cmpxFloat(cos_val, sin_val);
            }
        }
    }
}

/**
 * @brief 批量计算所有光源点的Pupil函数 (DATAFLOW)
 * 使用DATAFLOW pragma实现并行处理
 */
void calc_pupil_batch(
    float src[MAX_SRC_SIZE * MAX_SRC_SIZE],
    LithoParams &params,
    PupilEntry pupil_lut[MAX_SRC_SIZE * MAX_SRC_SIZE * MAX_TCC_SIZE * MAX_TCC_SIZE]
) {
    #pragma HLS DATAFLOW
    
    // 光源参数
    int sh = (params.srcSize - 1) / 2;
    int outerSigma = sh; // 简化处理
    
    // 初始化输出
    for (int i = 0; i < MAX_SRC_SIZE * MAX_SRC_SIZE * MAX_TCC_SIZE * MAX_TCC_SIZE; i++) {
        #pragma HLS UNROLL factor=16
        pupil_lut[i].valid = false;
        pupil_lut[i].value = cmpxFloat(0.0f, 0.0f);
    }
    
    // 批量计算每个光源点
    for (int q = -outerSigma; q <= outerSigma; q++) {
        for (int p = -outerSigma; p <= outerSigma; p++) {
            #pragma HLS PIPELINE II=1
            
            // 检查光源是否有效
            int srcID = (q + sh) * params.srcSize + (p + sh);
            if (src[srcID] != 0.0f && (p * p + q * q) <= sh * sh) {
                // 计算归一化光源坐标
                float sx = (float)p / (float)sh;
                float sy = (float)q / (float)sh;
                
                // 计算该光源点的Pupil函数
                PupilEntry pupil_temp[MAX_TCC_SIZE * MAX_TCC_SIZE];
                calc_pupil_single(sx, sy, params, pupil_temp);
                
                // 存储到查找表
                for (int i = 0; i < MAX_TCC_SIZE * MAX_TCC_SIZE; i++) {
                    #pragma HLS UNROLL factor=4
                    int lut_idx = srcID * MAX_TCC_SIZE * MAX_TCC_SIZE + i;
                    pupil_lut[lut_idx] = pupil_temp[i];
                }
            }
        }
    }
}

// ============================================================
// TCC矩阵计算模块
// ============================================================

/**
 * @brief 计算TCC矩阵上三角部分
 * 核心优化:
 * 1. 仅计算上三角减少计算量
 * 2. 循环展开+流水线化
 * 3. 数组分区优化存储访问
 * 4. 复数乘累加DSP优化
 */
void calc_tcc_upper_triangle(
    float src[MAX_SRC_SIZE * MAX_SRC_SIZE],
    PupilEntry pupil_lut[MAX_SRC_SIZE * MAX_SRC_SIZE * MAX_TCC_SIZE * MAX_TCC_SIZE],
    LithoParams &params,
    cmpxFloat tcc_out[MAX_TCC_SIZE * MAX_TCC_SIZE]
) {
    #pragma HLS ARRAY_PARTITION variable=tcc_out cyclic factor=4 dim=1
    #pragma HLS ARRAY_PARTITION variable=pupil_lut cyclic factor=4 dim=1
    
    // 参数初始化
    int sh = (params.srcSize - 1) / 2;
    int outerSigma = sh;
    int tccSize = params.tccSize;
    
    // 初始化TCC矩阵
    for (int i = 0; i < MAX_TCC_SIZE * MAX_TCC_SIZE; i++) {
        #pragma HLS UNROLL factor=16
        tcc_out[i] = cmpxFloat(0.0f, 0.0f);
    }
    
    // 计算频域范围常量
    float Lx_norm = params.Lx * params.NA / params.lambda;
    float Ly_norm = params.Ly * params.NA / params.lambda;
    
    // 外层循环: 光源点遍历
    for (int q = -outerSigma; q <= outerSigma; q++) {
        for (int p = -outerSigma; p <= outerSigma; p++) {
            #pragma HLS PIPELINE II=1
            
            // 检查光源是否有效
            int srcID = (q + sh) * params.srcSize + (p + sh);
            if (src[srcID] != 0.0f && (p * p + q * q) <= sh * sh) {
                // 光源值
                cmpxFloat srcVal(src[srcID], 0.0f);
                
                // 计算频域范围
                float sx = (float)p / (float)sh;
                float sy = (float)q / (float)sh;
                int nxMin = (int)((-1.0f - sx) * Lx_norm);
                int nxMax = (int)(( 1.0f - sx) * Lx_norm);
                int nyMin = (int)((-1.0f - sy) * Ly_norm);
                int nyMax = (int)(( 1.0f - sy) * Ly_norm);
                
                // 中层循环: Pupil ID1 遍历
                for (int ny = nyMin; ny <= nyMax; ny++) {
                    #pragma HLS LOOP_FLATTEN
                    for (int nx = nxMin; nx <= nxMax; nx++) {
                        #pragma HLS PIPELINE II=1
                        
                        int ID1 = (ny + params.Ny) * (params.Nx * 2 + 1) + (nx + params.Nx);
                        int lut_idx1 = srcID * MAX_TCC_SIZE * MAX_TCC_SIZE + ID1;
                        
                        // 检查Pupil1是否有效
                        if (pupil_lut[lut_idx1].valid) {
                            // 预计算 pupil1 * srcVal
                            cmpxFloat pupil1 = pupil_lut[lut_idx1].value;
                            cmpxFloat tmpVal = pupil1 * srcVal;
                            
                            // 内层循环: Pupil ID2 遍历 (上三角)
                            for (int my = ny; my <= nyMax; my++) {
                                #pragma HLS LOOP_FLATTEN
                                int startmx = (ny == my) ? nx : nxMin;
                                
                                for (int mx = startmx; mx <= nxMax; mx++) {
                                    #pragma HLS UNROLL factor=2
                                    
                                    int ID2 = (my + params.Ny) * (params.Nx * 2 + 1) + (mx + params.Nx);
                                    int lut_idx2 = srcID * MAX_TCC_SIZE * MAX_TCC_SIZE + ID2;
                                    
                                    // 检查Pupil2是否有效
                                    if (pupil_lut[lut_idx2].valid) {
                                        // 计算累加项
                                        cmpxFloat pupil2 = pupil_lut[lut_idx2].value;
                                        cmpxFloat conj_pupil2 = cmpxFloat(pupil2.real(), -pupil2.imag());
                                        
                                        // TCC累加
                                        tcc_out[ID1 * tccSize + ID2] += tmpVal * conj_pupil2;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

/**
 * @brief 填充TCC矩阵下三角部分 (对称性)
 * 利用 TCC[j,i] = conj(TCC[i,j])
 */
void fill_tcc_lower_triangle(
    cmpxFloat tcc_inout[MAX_TCC_SIZE * MAX_TCC_SIZE],
    int tccSize
) {
    #pragma HLS ARRAY_PARTITION variable=tcc_inout cyclic factor=4 dim=1
    
    // 下三角填充
    for (int i = 0; i < tccSize; i++) {
        #pragma HLS PIPELINE II=1
        for (int j = i + 1; j < tccSize; j++) {
            #pragma HLS UNROLL factor=4
            
            // TCC[j,i] = conj(TCC[i,j])
            cmpxFloat val = tcc_inout[i * tccSize + j];
            tcc_inout[j * tccSize + i] = cmpxFloat(val.real(), -val.imag());
        }
    }
}

/**
 * @brief 完整TCC计算流程 (DATAFLOW集成)
 * 流程: Pupil计算 -> TCC上三角 -> 下三角填充
 */
void hls_calc_tcc(
    hls::stream<float> &src_in,
    hls::stream<LithoParams> &params_in,
    hls::stream<cmpxFloat> &tcc_out
) {
    #pragma HLS DATAFLOW
    
    // 内部存储
    float src_local[MAX_SRC_SIZE * MAX_SRC_SIZE];
    PupilEntry pupil_lut[MAX_SRC_SIZE * MAX_SRC_SIZE * MAX_TCC_SIZE * MAX_TCC_SIZE];
    cmpxFloat tcc_local[MAX_TCC_SIZE * MAX_TCC_SIZE];
    LithoParams params_local;
    
    #pragma HLS ARRAY_PARTITION variable=src_local cyclic factor=16 dim=1
    #pragma HLS ARRAY_PARTITION variable=pupil_lut cyclic factor=8 dim=1
    #pragma HLS ARRAY_PARTITION variable=tcc_local cyclic factor=4 dim=1
    
    // 读取参数
    params_local = params_in.read();
    
    // 读取光源数据
    int srcSize = params_local.srcSize;
    for (int i = 0; i < srcSize * srcSize; i++) {
        #pragma HLS PIPELINE II=1
        src_local[i] = src_in.read();
    }
    
    // Stage 1: 批量计算Pupil函数
    calc_pupil_batch(src_local, params_local, pupil_lut);
    
    // Stage 2: 计算TCC上三角
    calc_tcc_upper_triangle(src_local, pupil_lut, params_local, tcc_local);
    
    // Stage 3: 填充下三角
    fill_tcc_lower_triangle(tcc_local, params_local.tccSize);
    
    // 输出TCC矩阵
    int tccSize = params_local.tccSize;
    for (int i = 0; i < tccSize * tccSize; i++) {
        #pragma HLS PIPELINE II=1
        tcc_out.write(tcc_local[i]);
    }
}

// ============================================================
// 辅助函数实现
// ============================================================

/**
 * @brief 计算光源点是否有效
 */
bool is_source_valid(int p, int q, int sh) {
    #pragma HLS INLINE
    return (p * p + q * q) <= sh * sh;
}

/**
 * @brief 计算频域坐标是否在NA范围内
 */
bool is_pupil_valid(float fx, float fy, float NA) {
    #pragma HLS INLINE
    float rho2 = fx * fx + fy * fy;
    return rho2 <= 1.0f;
}