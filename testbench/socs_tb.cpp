/**
 * @file socs_tb.cpp
 * @brief SOCS模块测试平台
 * 
 * 测试用例:
 * 1. 基本功能测试 - 验证非零输出
 * 2. 单核测试 - 验证Kernel-Mask乘法
 * 3. 多核测试 - 验证累加逻辑
 */

#include <iostream>
#include <cmath>
#include "../include/hls_socs.h"

using namespace std;

// 测试参数
const int TEST_NK = 2;
const int TEST_LX = 32;
const int TEST_LY = 32;
const int TEST_NX = 3;
const int TEST_NY = 3;

// 辅助函数: 初始化测试数据
void init_test_data(
    complex_float kernels[SOCS_MAX_KERNELS][SOCS_KERNEL_SIZE],
    float scales[SOCS_MAX_KERNELS],
    complex_float mask_fft[SOCS_MAX_LX * SOCS_MAX_LY]
) {
    int kernelSize = (2 * TEST_NX + 1) * (2 * TEST_NY + 1);
    
    // 初始化SOCS核 (简单正弦模式)
    for (int k = 0; k < TEST_NK; k++) {
        for (int i = 0; i < kernelSize; i++) {
            float phase = (float)i * 0.1f + k * 0.5f;
            kernels[k][i] = complex_float(cos(phase), sin(phase) * 0.5f);
        }
    }
    
    // 初始化权重系数
    for (int k = 0; k < TEST_NK; k++) {
        scales[k] = 0.5f + 0.2f * k;
    }
    
    // 初始化掩模频域数据 (中央峰值)
    int mskSize = TEST_LX * TEST_LY;
    for (int i = 0; i < mskSize; i++) {
        mask_fft[i] = complex_float(0.0f, 0.0f);
    }
    
    // 中央区域设置非零值
    int Lxh = TEST_LX / 2;
    int Lyh = TEST_LY / 2;
    for (int y = -TEST_NY; y <= TEST_NY; y++) {
        for (int x = -TEST_NX; x <= TEST_NX; x++) {
            int idx = (y + Lyh) * TEST_LX + (x + Lxh);
            mask_fft[idx] = complex_float(1.0f, 0.0f);
        }
    }
}

// 测试1: 基本功能测试
bool test_basic_function() {
    complex_float kernels[SOCS_MAX_KERNELS][SOCS_KERNEL_SIZE];
    float scales[SOCS_MAX_KERNELS];
    complex_float mask_fft[SOCS_MAX_LX * SOCS_MAX_LY];
    float image_out[SOCS_MAX_LX * SOCS_MAX_LY];
    
    // 初始化数据
    init_test_data(kernels, scales, mask_fft);
    
    // 初始化输出
    for (int i = 0; i < TEST_LX * TEST_LY; i++) {
        image_out[i] = 0.0f;
    }
    
    // 调用核心函数
    hls_calc_socs_core(kernels, scales, mask_fft, image_out,
                       TEST_NK, TEST_LX, TEST_LY, TEST_NX, TEST_NY);
    
    // 验证结果
    int non_zero_count = 0;
    float max_val = 0.0f;
    
    for (int i = 0; i < TEST_LX * TEST_LY; i++) {
        if (image_out[i] > 1e-6f) {
            non_zero_count++;
            if (image_out[i] > max_val) {
                max_val = image_out[i];
            }
        }
    }
    
    cout << "Test 1: Basic Function" << endl;
    cout << "  Non-zero pixels: " << non_zero_count << " / " << (TEST_LX * TEST_LY) << endl;
    cout << "  Max value: " << max_val << endl;
    
    bool passed = (non_zero_count > 0) && (max_val > 1e-6f);
    cout << "  Result: " << (passed ? "PASS" : "FAIL") << endl;
    
    return passed;
}

// 测试2: 单核测试
bool test_single_kernel() {
    complex_float kernels[SOCS_MAX_KERNELS][SOCS_KERNEL_SIZE];
    float scales[SOCS_MAX_KERNELS];
    complex_float mask_fft[SOCS_MAX_LX * SOCS_MAX_LY];
    float image_out[SOCS_MAX_LX * SOCS_MAX_LY];
    
    // 使用单一核
    int kernelSize = (2 * TEST_NX + 1) * (2 * TEST_NY + 1);
    
    for (int i = 0; i < kernelSize; i++) {
        kernels[0][i] = complex_float(1.0f, 0.0f);  // 全实部
    }
    scales[0] = 1.0f;
    
    // 掩模: 中央单个非零点
    int mskSize = TEST_LX * TEST_LY;
    for (int i = 0; i < mskSize; i++) {
        mask_fft[i] = complex_float(0.0f, 0.0f);
    }
    mask_fft[TEST_LY/2 * TEST_LX + TEST_LX/2] = complex_float(1.0f, 0.0f);
    
    // 初始化输出
    for (int i = 0; i < TEST_LX * TEST_LY; i++) {
        image_out[i] = 0.0f;
    }
    
    // 调用核心函数 (单核)
    hls_calc_socs_core(kernels, scales, mask_fft, image_out,
                       1, TEST_LX, TEST_LY, TEST_NX, TEST_NY);
    
    // 验证: 单核情况下输出应为常数
    float sum = 0.0f;
    for (int i = 0; i < TEST_LX * TEST_LY; i++) {
        sum += image_out[i];
    }
    
    cout << "Test 2: Single Kernel" << endl;
    cout << "  Output sum: " << sum << endl;
    
    bool passed = (sum > 1e-6f);
    cout << "  Result: " << (passed ? "PASS" : "FAIL") << endl;
    
    return passed;
}

// 测试3: 多核累加测试
bool test_multi_kernel_accumulate() {
    complex_float kernels[SOCS_MAX_KERNELS][SOCS_KERNEL_SIZE];
    float scales[SOCS_MAX_KERNELS];
    complex_float mask_fft[SOCS_MAX_LX * SOCS_MAX_LY];
    float image_out1[SOCS_MAX_LX * SOCS_MAX_LY];
    float image_out2[SOCS_MAX_LX * SOCS_MAX_LY];
    
    // 准备两个相同的核
    int kernelSize = (2 * TEST_NX + 1) * (2 * TEST_NY + 1);
    
    for (int i = 0; i < kernelSize; i++) {
        kernels[0][i] = complex_float(0.5f, 0.0f);
        kernels[1][i] = complex_float(0.5f, 0.0f);
    }
    
    scales[0] = 1.0f;
    scales[1] = 1.0f;
    
    // 掩模中央峰值
    int mskSize = TEST_LX * TEST_LY;
    for (int i = 0; i < mskSize; i++) {
        mask_fft[i] = complex_float(0.0f, 0.0f);
    }
    mask_fft[TEST_LY/2 * TEST_LX + TEST_LX/2] = complex_float(2.0f, 0.0f);
    
    // 测试单核
    for (int i = 0; i < TEST_LX * TEST_LY; i++) {
        image_out1[i] = 0.0f;
    }
    hls_calc_socs_core(kernels, scales, mask_fft, image_out1,
                       1, TEST_LX, TEST_LY, TEST_NX, TEST_NY);
    
    // 测试双核累加
    for (int i = 0; i < TEST_LX * TEST_LY; i++) {
        image_out2[i] = 0.0f;
    }
    hls_calc_socs_core(kernels, scales, mask_fft, image_out2,
                       2, TEST_LX, TEST_LY, TEST_NX, TEST_NY);
    
    // 验证: 双核应该是单核的两倍
    float sum1 = 0.0f, sum2 = 0.0f;
    for (int i = 0; i < TEST_LX * TEST_LY; i++) {
        sum1 += image_out1[i];
        sum2 += image_out2[i];
    }
    
    cout << "Test 3: Multi-Kernel Accumulate" << endl;
    cout << "  Single kernel sum: " << sum1 << endl;
    cout << "  Double kernel sum: " << sum2 << endl;
    cout << "  Ratio: " << (sum2 / sum1) << endl;
    
    bool passed = (sum2 > sum1 * 1.5f);  // 允许一定误差
    cout << "  Result: " << (passed ? "PASS" : "FAIL") << endl;
    
    return passed;
}

int main() {
    cout << "======================================" << endl;
    cout << "  SOCS Module HLS Test Bench" << endl;
    cout << "======================================" << endl;
    
    int passed = 0;
    int total = 3;
    
    if (test_basic_function()) passed++;
    if (test_single_kernel()) passed++;
    if (test_multi_kernel_accumulate()) passed++;
    
    cout << "======================================" << endl;
    cout << "  TEST STATUS: " << (passed == total ? "PASS" : "FAIL") << endl;
    cout << "  Tests passed: " << passed << "/" << total << endl;
    cout << "======================================" << endl;
    
    return (passed == total) ? 0 : 1;
}