/*
 * FPGA-Litho HLS TCC Module HLS C Simulation Test Bench
 * TCC模块HLS C仿真专用测试平台
 * 
 * 用于Vitis HLS C仿真 (csim) 验证
 */

#include <iostream>
#include <cmath>
#include <complex>
#include "../include/hls_tcc.h"
#include "../include/hls_types.h"

using namespace std;

// ============================================================
// 测试辅助函数
// ============================================================

/**
 * @brief 生成简化Annular光源 (固定64x64尺寸)
 */
void generate_annular_source_64(float src[64 * 64]) {
    int sh = 31; // (64-1)/2
    float innerRadius = 0.5f;
    float outerRadius = 0.9f;
    
    // 初始化为0
    for (int i = 0; i < 64 * 64; i++) {
        src[i] = 0.0f;
    }
    
    // 设置Annular光源
    for (int q = -sh; q <= sh; q++) {
        for (int p = -sh; p <= sh; p++) {
            float sx = (float)p / (float)sh;
            float sy = (float)q / (float)sh;
            float r = sqrt(sx * sx + sy * sy);
            
            if (r >= innerRadius && r <= outerRadius) {
                int idx = (q + sh) * 64 + (p + sh);
                src[idx] = 1.0f;
            }
        }
    }
}

/**
 * @brief 验证TCC矩阵对称性
 */
bool verify_tcc_symmetry(cmpxFloat tcc[7 * 7], int tccSize) {
    bool valid = true;
    float max_diff = 0.0f;
    
    for (int i = 0; i < tccSize; i++) {
        for (int j = i + 1; j < tccSize; j++) {
            cmpxFloat val_ij = tcc[i * tccSize + j];
            cmpxFloat val_ji = tcc[j * tccSize + i];
            cmpxFloat conj_ij = cmpxFloat(val_ij.real(), -val_ij.imag());
            
            float diff = abs(val_ji - conj_ij);
            max_diff = max(max_diff, diff);
            
            if (diff > 1e-5f) {
                valid = false;
                cout << "Symmetry violation at [" << i << "," << j << "]: ";
                cout << "TCC[i,j]=" << val_ij << ", TCC[j,i]=" << val_ji << ", diff=" << diff << endl;
            }
        }
    }
    
    cout << "Max symmetry difference: " << max_diff << endl;
    return valid;
}

/**
 * @brief 验证TCC矩阵非零性
 */
bool verify_tcc_nonzero(cmpxFloat tcc[7 * 7], int tccSize) {
    int nonzero_count = 0;
    float max_val = 0.0f;
    
    for (int i = 0; i < tccSize * tccSize; i++) {
        float val = abs(tcc[i]);
        if (val > 1e-8f) {
            nonzero_count++;
            max_val = max(max_val, val);
        }
    }
    
    cout << "Non-zero TCC entries: " << nonzero_count << " / " << (tccSize * tccSize) << endl;
    cout << "Max TCC magnitude: " << max_val << endl;
    
    return (nonzero_count > 0) && (max_val > 0.1f);
}

// ============================================================
// 主测试函数
// ============================================================

int main() {
    cout << "FPGA-Litho HLS TCC C Simulation Test" << endl;
    cout << "=================================" << endl;
    cout << endl;
    
    // 测试数据
    float src[64 * 64];
    cmpxFloat tcc[7 * 7];
    
    // 生成光源
    cout << "Step 1: Generate Annular Source..." << endl;
    generate_annular_source_64(src);
    
    // 统计光源有效点数
    int src_count = 0;
    for (int i = 0; i < 64 * 64; i++) {
        if (src[i] > 0.5f) src_count++;
    }
    cout << "Source valid points: " << src_count << " / " << (64 * 64) << endl;
    cout << endl;
    
    // 计算TCC
    cout << "Step 2: Calculate TCC Matrix..." << endl;
    hls_tcc_test_top(src, tcc);
    cout << "TCC calculation completed!" << endl;
    cout << endl;
    
    // 验证结果
    cout << "Step 3: Verify TCC Results..." << endl;
    
    bool nonzero_valid = verify_tcc_nonzero(tcc, 7);
    bool symmetry_valid = verify_tcc_symmetry(tcc, 7);
    
    // 打印部分TCC矩阵值
    cout << endl << "Sample TCC values:" << endl;
    for (int i = 0; i < 7; i++) {
        for (int j = 0; j < 7; j++) {
            float val = abs(tcc[i * 7 + j]);
            cout << val << " ";
        }
        cout << endl;
    }
    
    // 最终判断
    cout << endl << "=== Test Result ===" << endl;
    if (nonzero_valid && symmetry_valid) {
        cout << "✓ TCC C Simulation PASSED" << endl;
        return 0;
    } else {
        cout << "✗ TCC C Simulation FAILED" << endl;
        if (!nonzero_valid) cout << "  Non-zero check failed" << endl;
        if (!symmetry_valid) cout << "  Symmetry check failed" << endl;
        return 1;
    }
}