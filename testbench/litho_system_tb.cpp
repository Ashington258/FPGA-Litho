/**
 * @file litho_system_tb.cpp
 * @brief K-Litho System Integration Testbench
 * 
 * 测试TCC和SOCS两种工作模式的系统集成
 * 
 * @author K-Litho Team
 * @date 2026-04-03
 */

#include <iostream>
#include <cmath>
#include <complex>
#include <cstring>
#include "../include/hls_litho_system.h"

using namespace std;

//=============================================================================
// Test Helper Functions
//=============================================================================

// 初始化TCC数据 (简化的对角占优矩阵)
void init_tcc_data(
    cmpxFloat tcc[],
    int Nx, int Ny
) {
    int tcc_dim = (2*Nx+1) * (2*Ny+1);
    
    // 简化TCC: 对角线元素为1.0，周围衰减
    for (int i = 0; i < tcc_dim; i++) {
        for (int j = 0; j < tcc_dim; j++) {
            int xi = (i % (2*Nx+1)) - Nx;
            int yi = (i / (2*Nx+1)) - Ny;
            int xj = (j % (2*Nx+1)) - Nx;
            int yj = (j / (2*Nx+1)) - Ny;
            
            // TCC元素: 高斯衰减
            float dx = xi - xj;
            float dy = yi - yj;
            float sigma = 2.0f;
            float val = exp(-(dx*dx + dy*dy) / (2 * sigma * sigma));
            
            tcc[i * tcc_dim + j] = cmpxFloat(val, 0.0f);
        }
    }
}

void init_test_data(
    cmpxFloat source[],
    cmpxFloat mask_fft[],
    cmpxFloat kernels[],
    float scales[],
    int srcSize,
    int Lx, int Ly,
    int Nx, int Ny,
    int nkernels
) {
    // 初始化光源数据 (简化为高斯分布)
    int src_total = srcSize * srcSize;
    for (int i = 0; i < src_total; i++) {
        float x = (i % srcSize) - srcSize / 2.0f;
        float y = (i / srcSize) - srcSize / 2.0f;
        float sigma = srcSize / 4.0f;
        float val = exp(-(x*x + y*y) / (2 * sigma * sigma));
        source[i] = cmpxFloat(val, 0.0f);
    }
    
    // 初始化掩模频谱 (矩形函数的FFT)
    int mask_total = Lx * Ly;
    for (int i = 0; i < mask_total; i++) {
        float x = (i % Lx) - Lx / 2.0f;
        float y = (i / Lx) - Ly / 2.0f;
        
        // 简化的矩形频谱
        float sinc_x = (x == 0) ? 1.0f : sin(M_PI * x / (Lx/4)) / (M_PI * x / (Lx/4));
        float sinc_y = (y == 0) ? 1.0f : sin(M_PI * y / (Ly/4)) / (M_PI * y / (Ly/4));
        float val = sinc_x * sinc_y;
        
        mask_fft[i] = cmpxFloat(val, 0.0f);
    }
    
    // 初始化SOCS核 (高斯核)
    int kernel_size = (2*Nx+1) * (2*Ny+1);
    for (int k = 0; k < nkernels; k++) {
        float sigma_k = 1.0f + k * 0.5f;
        for (int i = 0; i < kernel_size; i++) {
            int x = (i % (2*Nx+1)) - Nx;
            int y = (i / (2*Nx+1)) - Ny;
            float val = exp(-(x*x + y*y) / (2 * sigma_k * sigma_k));
            kernels[k * kernel_size + i] = cmpxFloat(val, 0.0f);
        }
        scales[k] = 1.0f / (k + 1);  // 递减权重
    }
}

//=============================================================================
// Test Cases
//=============================================================================

/**
 * @brief 测试1: TCC模式基本功能
 */
int test_tcc_mode() {
    cout << "========================================" << endl;
    cout << "Test 1: TCC Mode Basic Functionality" << endl;
    cout << "========================================" << endl;
    
    // 测试参数
    const int Lx = 32, Ly = 32;
    const int Nx = 3, Ny = 3;
    const int srcSize = 16;
    
    // 分配测试数据
    cmpxFloat* source = new cmpxFloat[SYS_MAX_SRC_SIZE * SYS_MAX_SRC_SIZE];
    cmpxFloat* mask_fft = new cmpxFloat[SYS_MAX_LX * SYS_MAX_LY];
    cmpxFloat* imgf = new cmpxFloat[SYS_MAX_LX * SYS_MAX_LY];
    cmpxFloat* tcc = new cmpxFloat[SYS_TCC_TOTAL];
    cmpxFloat* kernels = new cmpxFloat[SYS_MAX_KERNELS * SYS_TCC_DIM];
    float* scales = new float[SYS_MAX_KERNELS];
    float* img_out = new float[SYS_OUTPUT_SIZE];
    
    // 初始化数据
    init_test_data(source, mask_fft, kernels, scales, srcSize, Lx, Ly, Nx, Ny, 4);
    init_tcc_data(tcc, Nx, Ny);  // 初始化TCC数据
    memset(imgf, 0, SYS_MAX_LX * SYS_MAX_LY * sizeof(cmpxFloat));
    
    // 调用系统 (TCC模式)
    hls_litho_system(
        source, mask_fft, tcc, kernels, scales, imgf, img_out,
        193.0f,  // lambda
        1.35f,   // NA
        0.0f,    // defocus
        Lx, Ly, Nx, Ny, srcSize, 4, 1  // mode=1 (TCC)
    );
    
    // 验证输出
    int non_zero = 0;
    float max_mag = 0.0f;
    
    for (int i = 0; i < Lx * Ly; i++) {
        float mag = abs(imgf[i]);
        if (mag > 1e-6f) {
            non_zero++;
            if (mag > max_mag) max_mag = mag;
        }
    }
    
    cout << "Output Statistics:" << endl;
    cout << "  Non-zero elements: " << non_zero << " / " << (Lx * Ly) << endl;
    cout << "  Max magnitude: " << max_mag << endl;
    
    bool pass = (non_zero > 0) && (max_mag > 0);
    
    // 清理
    delete[] source;
    delete[] mask_fft;
    delete[] imgf;
    delete[] tcc;
    delete[] kernels;
    delete[] scales;
    delete[] img_out;
    
    cout << "Result: " << (pass ? "PASS" : "FAIL") << endl << endl;
    return pass ? 0 : 1;
}

/**
 * @brief 测试2: SOCS模式基本功能
 */
int test_socs_mode() {
    cout << "========================================" << endl;
    cout << "Test 2: SOCS Mode Basic Functionality" << endl;
    cout << "========================================" << endl;
    
    // 测试参数
    const int Lx = 32, Ly = 32;
    const int Nx = 3, Ny = 3;
    const int nkernels = 4;
    
    // 分配测试数据
    cmpxFloat* source = new cmpxFloat[SYS_MAX_SRC_SIZE * SYS_MAX_SRC_SIZE];
    cmpxFloat* mask_fft = new cmpxFloat[SYS_MAX_LX * SYS_MAX_LY];
    cmpxFloat* imgf = new cmpxFloat[SYS_MAX_LX * SYS_MAX_LY];
    cmpxFloat* tcc = new cmpxFloat[SYS_TCC_TOTAL];
    cmpxFloat* kernels = new cmpxFloat[SYS_MAX_KERNELS * SYS_TCC_DIM];
    float* scales = new float[SYS_MAX_KERNELS];
    float* img_out = new float[SYS_OUTPUT_SIZE];
    
    // 初始化数据
    init_test_data(source, mask_fft, kernels, scales, 16, Lx, Ly, Nx, Ny, nkernels);
    memset(img_out, 0, SYS_OUTPUT_SIZE * sizeof(float));
    
    // 调用系统 (SOCS模式)
    hls_litho_system(
        source, mask_fft, tcc, kernels, scales, imgf, img_out,
        193.0f,  // lambda
        1.35f,   // NA
        0.0f,    // defocus
        Lx, Ly, Nx, Ny, 16, nkernels, 2  // mode=2 (SOCS)
    );
    
    // 验证输出
    int output_size = (4*Nx+1) * (4*Ny+1);
    int non_zero = 0;
    float max_val = 0.0f;
    float sum = 0.0f;
    
    for (int i = 0; i < output_size; i++) {
        if (img_out[i] > 1e-6f) {
            non_zero++;
            if (img_out[i] > max_val) max_val = img_out[i];
        }
        sum += img_out[i];
    }
    
    cout << "Output Statistics:" << endl;
    cout << "  Non-zero elements: " << non_zero << " / " << output_size << endl;
    cout << "  Max value: " << max_val << endl;
    cout << "  Sum: " << sum << endl;
    
    bool pass = (non_zero > 0) && (max_val > 0);
    
    // 清理
    delete[] source;
    delete[] mask_fft;
    delete[] imgf;
    delete[] tcc;
    delete[] kernels;
    delete[] scales;
    delete[] img_out;
    
    cout << "Result: " << (pass ? "PASS" : "FAIL") << endl << endl;
    return pass ? 0 : 1;
}

/**
 * @brief 测试3: 多核SOCS累加验证
 */
int test_socs_accumulation() {
    cout << "========================================" << endl;
    cout << "Test 3: SOCS Multi-Kernel Accumulation" << endl;
    cout << "========================================" << endl;
    
    const int Lx = 16, Ly = 16;
    const int Nx = 2, Ny = 2;
    
    // 分配测试数据
    cmpxFloat* source = new cmpxFloat[SYS_MAX_SRC_SIZE * SYS_MAX_SRC_SIZE];
    cmpxFloat* mask_fft = new cmpxFloat[SYS_MAX_LX * SYS_MAX_LY];
    cmpxFloat* imgf = new cmpxFloat[SYS_MAX_LX * SYS_MAX_LY];
    cmpxFloat* tcc = new cmpxFloat[SYS_TCC_TOTAL];
    cmpxFloat* kernels = new cmpxFloat[SYS_MAX_KERNELS * SYS_TCC_DIM];
    float* scales = new float[SYS_MAX_KERNELS];
    float* img_out = new float[SYS_OUTPUT_SIZE];
    float* img_out_ref = new float[SYS_OUTPUT_SIZE];
    
    // 初始化
    init_test_data(source, mask_fft, kernels, scales, 8, Lx, Ly, Nx, Ny, 4);
    
    // 测试1核
    hls_litho_system(
        source, mask_fft, tcc, kernels, scales, imgf, img_out,
        193.0f, 1.35f, 0.0f, Lx, Ly, Nx, Ny, 8, 1, 2
    );
    
    int output_size = (4*Nx+1) * (4*Ny+1);
    for (int i = 0; i < output_size; i++) {
        img_out_ref[i] = img_out[i];
    }
    
    // 测试4核
    hls_litho_system(
        source, mask_fft, tcc, kernels, scales, imgf, img_out,
        193.0f, 1.35f, 0.0f, Lx, Ly, Nx, Ny, 8, 4, 2
    );
    
    // 验证: 4核结果应该比1核更大 (累加效应)
    int larger_count = 0;
    for (int i = 0; i < output_size; i++) {
        if (img_out[i] > img_out_ref[i] * 1.1f) {
            larger_count++;
        }
    }
    
    cout << "Output Comparison (4 kernels vs 1 kernel):" << endl;
    cout << "  Pixels with larger values: " << larger_count << " / " << output_size << endl;
    
    bool pass = (larger_count > output_size / 4);  // 至少25%像素更大
    
    // 清理
    delete[] source;
    delete[] mask_fft;
    delete[] imgf;
    delete[] tcc;
    delete[] kernels;
    delete[] scales;
    delete[] img_out;
    delete[] img_out_ref;
    
    cout << "Result: " << (pass ? "PASS" : "FAIL") << endl << endl;
    return pass ? 0 : 1;
}

//=============================================================================
// Main Test Runner
//=============================================================================

int main() {
    cout << "========================================" << endl;
    cout << "K-Litho System Integration Test" << endl;
    cout << "========================================" << endl;
    cout << "Testing TCC and SOCS modes..." << endl << endl;
    
    int failures = 0;
    
    failures += test_tcc_mode();
    failures += test_socs_mode();
    failures += test_socs_accumulation();
    
    cout << "========================================" << endl;
    cout << "TEST SUMMARY" << endl;
    cout << "========================================" << endl;
    cout << "Total tests: 3" << endl;
    cout << "Passed: " << (3 - failures) << endl;
    cout << "Failed: " << failures << endl;
    cout << "========================================" << endl;
    
    if (failures == 0) {
        cout << "ALL TESTS PASSED!" << endl;
        return 0;
    } else {
        cout << "SOME TESTS FAILED!" << endl;
        return 1;
    }
}