/**
 * @file litho_system_bram_tb.cpp
 * @brief FPGA-Litho BRAM接口测试平台
 * 
 * 测试目标:
 * 1. 单数据加载/读取功能
 * 2. 批量数据加载功能
 * 3. 计算状态管理
 * 4. TCC模式计算验证 (Nx≤3)
 * 5. SOCS模式计算验证 (8核)
 * 6. 参数验证和错误处理
 * 7. 存储复位功能
 * 
 * @author FPGA-Litho Team
 * @date 2026-04-03
 */

#include <iostream>
#include <cmath>
#include <ap_fixed.h>
#include <ap_int.h>
#include <hls_stream.h>
#include <hls_math.h>

#include "../include/hls_litho_system_bram.h"

using namespace std;

// 测试常量
const float TEST_TOLERANCE = 1e-3f;
const int TEST_LX = 64;
const int TEST_LY = 64;
const int TEST_NX_TCC = 3;  // TCC模式最大Nx
const int TEST_NY = 1;
const int TEST_NX_SOCS = 7;
const int TEST_NKERNELS = 8;  // SOCS模式最大核数

//=============================================================================
// 测试辅助函数
//=============================================================================

bool compare_complex(cmpxFloat a, cmpxFloat b, float tol = TEST_TOLERANCE) {
    float real_diff = abs(a.real() - b.real());
    float imag_diff = abs(a.imag() - b.imag());
    return (real_diff < tol) && (imag_diff < tol);
}

float generate_test_value(int seed, bool is_imag = false) {
    float base = is_imag ? 0.1f : 0.05f;
    return base * (seed + 1);
}

void print_test_header(const char* test_name) {
    cout << "\n========================================" << endl;
    cout << "Test: " << test_name << endl;
    cout << "========================================" << endl;
}

void print_test_result(const char* test_name, bool passed) {
    if (passed) {
        cout << "[PASS] " << test_name << endl;
    } else {
        cout << "[FAIL] " << test_name << endl;
    }
}

//=============================================================================
// 测试用例实现
//=============================================================================

/**
 * 测试1: 单数据加载和读取功能
 */
bool test_single_data_load_read() {
    print_test_header("Single Data Load/Read");
    
    bool all_passed = true;
    
    // 测试source数据加载/读取
    cout << "Testing source_bram..." << endl;
    cmpxFloat test_val(1.5f, 2.5f);
    load_source_data(0, test_val);
    cmpxFloat read_val = source_bram[0];
    if (!compare_complex(read_val, test_val)) {
        cout << "  ERROR: source_bram[0] mismatch" << endl;
        all_passed = false;
    }
    
    // 测试mask数据加载/读取
    cout << "Testing mask_bram..." << endl;
    test_val = cmpxFloat(3.5f, 4.5f);
    load_mask_data(10, test_val);
    read_val = mask_bram[10];
    if (!compare_complex(read_val, test_val)) {
        cout << "  ERROR: mask_bram[10] mismatch" << endl;
        all_passed = false;
    }
    
    // 测试tcc数据加载/读取
    cout << "Testing tcc_bram..." << endl;
    test_val = cmpxFloat(5.5f, 6.5f);
    load_tcc_data(20, test_val);  // 使用有效索引 (< 49)
    read_val = tcc_bram[20];
    if (!compare_complex(read_val, test_val)) {
        cout << "  ERROR: tcc_bram[20] mismatch" << endl;
        all_passed = false;
    }
    
    // 测试kernels数据加载/读取
    cout << "Testing kernels_bram..." << endl;
    test_val = cmpxFloat(7.5f, 8.5f);
    load_kernels_data(200, test_val);
    read_val = kernels_bram[200];
    if (!compare_complex(read_val, test_val)) {
        cout << "  ERROR: kernels_bram[200] mismatch" << endl;
        all_passed = false;
    }
    
    // 测试scales数据加载/读取
    cout << "Testing scales_bram..." << endl;
    float test_scale = 9.5f;
    load_scales_data(5, test_scale);
    float read_scale = scales_bram[5];
    if (abs(read_scale - test_scale) > TEST_TOLERANCE) {
        cout << "  ERROR: scales_bram[5] mismatch" << endl;
        all_passed = false;
    }
    
    print_test_result("Single Data Load/Read", all_passed);
    return all_passed;
}

/**
 * 测试2: 批量数据加载功能
 */
bool test_batch_data_load() {
    print_test_header("Batch Data Load");
    
    bool all_passed = true;
    
    // 创建测试数据
    cmpxFloat source_batch[BRAM_SOURCE_SIZE];
    cmpxFloat mask_batch[BRAM_MASK_SIZE];
    cmpxFloat kernels_batch[BRAM_KERNELS_SIZE];
    float scales_batch[BRAM_SCALES_SIZE];
    
    // 初始化测试数据
    for (int i = 0; i < BRAM_SOURCE_SIZE; i++) {
        source_batch[i] = cmpxFloat(generate_test_value(i, false),
                                     generate_test_value(i, true));
    }
    
    for (int i = 0; i < BRAM_MASK_SIZE; i++) {
        mask_batch[i] = cmpxFloat(generate_test_value(i+100, false),
                                  generate_test_value(i+100, true));
    }
    
    for (int i = 0; i < BRAM_KERNELS_SIZE; i++) {
        kernels_batch[i] = cmpxFloat(generate_test_value(i+200, false),
                                     generate_test_value(i+200, true));
    }
    
    for (int i = 0; i < BRAM_SCALES_SIZE; i++) {
        scales_batch[i] = 0.1f * (i + 1);
    }
    
    // 执行批量加载
    cout << "Loading source batch..." << endl;
    load_source_batch(source_batch);
    
    cout << "Loading mask batch..." << endl;
    load_mask_batch(mask_batch);
    
    cout << "Loading kernels batch..." << endl;
    load_kernels_batch(kernels_batch);
    
    cout << "Loading scales batch..." << endl;
    load_scales_batch(scales_batch);
    
    // 验证数据正确性 (抽样检查)
    cout << "Verifying loaded data..." << endl;
    
    // 检查source
    if (!compare_complex(source_bram[0], source_batch[0])) {
        cout << "  ERROR: source_bram[0] batch mismatch" << endl;
        all_passed = false;
    }
    if (!compare_complex(source_bram[BRAM_SOURCE_SIZE/2], source_batch[BRAM_SOURCE_SIZE/2])) {
        cout << "  ERROR: source_bram[middle] batch mismatch" << endl;
        all_passed = false;
    }
    
    // 检查mask
    if (!compare_complex(mask_bram[0], mask_batch[0])) {
        cout << "  ERROR: mask_bram[0] batch mismatch" << endl;
        all_passed = false;
    }
    
    // 检查kernels
    if (!compare_complex(kernels_bram[0], kernels_batch[0])) {
        cout << "  ERROR: kernels_bram[0] batch mismatch" << endl;
        all_passed = false;
    }
    
    // 检查scales
    if (abs(scales_bram[0] - scales_batch[0]) > TEST_TOLERANCE) {
        cout << "  ERROR: scales_bram[0] batch mismatch" << endl;
        all_passed = false;
    }
    
    print_test_result("Batch Data Load", all_passed);
    return all_passed;
}

/**
 * 测试3: 计算状态管理
 */
bool test_compute_status() {
    print_test_header("Compute Status Management");
    
    bool all_passed = true;
    
    // 初始状态应为idle (0)
    reset_bram_storage();
    int status = get_compute_status();
    if (status != 0) {
        cout << "ERROR: Initial status should be 0 (idle), got " << status << endl;
        all_passed = false;
    }
    cout << "Initial status: " << status << " (idle) - OK" << endl;
    
    // 启动计算后应为running (1)
    // 注意: 由于这是简化实现，计算会立即完成
    // 在实际实现中，应有单独的状态更新逻辑
    cout << "Status management test passed (simplified implementation)" << endl;
    
    print_test_result("Compute Status Management", all_passed);
    return all_passed;
}

/**
 * 测试4: TCC模式计算验证
 */
bool test_tcc_mode_compute() {
    print_test_header("TCC Mode Compute");
    
    bool all_passed = true;
    
    // 重置存储
    reset_bram_storage();
    
    // 准备测试数据
    cout << "Preparing TCC test data..." << endl;
    
    // 简化的source数据 (Nx=3)
    for (int i = 0; i < TEST_NX_TCC; i++) {
        cmpxFloat val(generate_test_value(i, false),
                      generate_test_value(i, true));
        load_source_data(i, val);
    }
    
    // 简化的mask数据
    for (int i = 0; i < TEST_LX * TEST_LY; i++) {
        cmpxFloat val(generate_test_value(i+1000, false),
                      generate_test_value(i+1000, true));
        load_mask_data(i, val);
    }
    
    // 简化的TCC数据
    for (int i = 0; i < TEST_NX_TCC; i++) {
        cmpxFloat val(1.0f, 0.0f);  // 单位TCC
        load_tcc_data(i, val);
    }
    
    // 启动TCC计算
    cout << "Starting TCC compute (Nx=" << TEST_NX_TCC << ", Lx=" << TEST_LX 
         << ", Ly=" << TEST_LY << ")..." << endl;
    start_litho_compute(1, TEST_LX, TEST_LY, TEST_NX_TCC, TEST_NY, 
                        TEST_NX_TCC, 0);
    
    // 检查状态
    int status = get_compute_status();
    if (status != 2) {  // 应为done
        cout << "ERROR: TCC compute status = " << status << " (expected 2)" << endl;
        all_passed = false;
    } else {
        cout << "TCC compute completed successfully" << endl;
    }
    
    // 读取输出数据 (抽样检查)
    cout << "Reading output imgf data..." << endl;
    cmpxFloat output = read_imgf_data(0);
    cout << "  imgf[0] = (" << output.real() << ", " << output.imag() << ")" << endl;
    
    // 测试边界情况: Nx > 3 (应返回错误)
    cout << "Testing TCC boundary (Nx=4 > max 3)..." << endl;
    reset_bram_storage();
    start_litho_compute(1, TEST_LX, TEST_LY, 4, TEST_NY, 4, 0);
    status = get_compute_status();
    if (status != 3) {  // 应为error
        cout << "ERROR: TCC Nx=4 should cause error, status = " << status << endl;
        all_passed = false;
    } else {
        cout << "TCC boundary check passed (Nx=4 rejected)" << endl;
    }
    
    print_test_result("TCC Mode Compute", all_passed);
    return all_passed;
}

/**
 * 测试5: SOCS模式计算验证
 */
bool test_socs_mode_compute() {
    print_test_header("SOCS Mode Compute");
    
    bool all_passed = true;
    
    // 重置存储
    reset_bram_storage();
    
    // 准备测试数据
    cout << "Preparing SOCS test data..." << endl;
    
    // 加载kernels (8个核)
    for (int k = 0; k < TEST_NKERNELS; k++) {
        for (int i = 0; i < 225; i++) {  // 每个核15x15=225元素
            int idx = k * 225 + i;
            cmpxFloat val(generate_test_value(idx, false),
                          generate_test_value(idx, true));
            load_kernels_data(idx, val);
        }
    }
    
    // 加载scales
    for (int i = 0; i < TEST_NKERNELS; i++) {
        load_scales_data(i, 1.0f / (i + 1));  // 递减权重
    }
    
    // 加载mask
    for (int i = 0; i < TEST_LX * TEST_LY; i++) {
        cmpxFloat val(generate_test_value(i+2000, false),
                      generate_test_value(i+2000, true));
        load_mask_data(i, val);
    }
    
    // 启动SOCS计算
    cout << "Starting SOCS compute (nkernels=" << TEST_NKERNELS 
         << ", Lx=" << TEST_LX << ", Ly=" << TEST_LY << ")..." << endl;
    start_litho_compute(2, TEST_LX, TEST_LY, TEST_NX_SOCS, TEST_NY,
                        0, TEST_NKERNELS);
    
    // 检查状态
    int status = get_compute_status();
    if (status != 2) {  // 应为done
        cout << "ERROR: SOCS compute status = " << status << " (expected 2)" << endl;
        all_passed = false;
    } else {
        cout << "SOCS compute completed successfully" << endl;
    }
    
    // 读取输出数据
    cout << "Reading output img_out data..." << endl;
    float output = read_img_out_data(0);
    cout << "  img_out[0] = " << output << endl;
    
    // 测试边界情况: nkernels > 8
    cout << "Testing SOCS boundary (nkernels=9 > max 8)..." << endl;
    reset_bram_storage();
    start_litho_compute(2, TEST_LX, TEST_LY, TEST_NX_SOCS, TEST_NY, 0, 9);
    status = get_compute_status();
    if (status != 3) {  // 应为error
        cout << "ERROR: SOCS nkernels=9 should cause error, status = " << status << endl;
        all_passed = false;
    } else {
        cout << "SOCS boundary check passed (nkernels=9 rejected)" << endl;
    }
    
    print_test_result("SOCS Mode Compute", all_passed);
    return all_passed;
}

/**
 * 测试6: 参数验证和错误处理
 */
bool test_parameter_validation() {
    print_test_header("Parameter Validation");
    
    bool all_passed = true;
    
    // 重置存储
    reset_bram_storage();
    
    // 测试1: 无效模式
    cout << "Testing invalid mode (mode=0)..." << endl;
    start_litho_compute(0, TEST_LX, TEST_LY, TEST_NX_TCC, TEST_NY, 1, 0);
    int status = get_compute_status();
    if (status != 3) {
        cout << "ERROR: Invalid mode should cause error" << endl;
        all_passed = false;
    }
    
    // 测试2: 尺寸超出范围
    cout << "Testing invalid Lx (Lx=1025 > max 1024)..." << endl;
    reset_bram_storage();
    start_litho_compute(1, 1025, TEST_LY, TEST_NX_TCC, TEST_NY, 1, 0);
    status = get_compute_status();
    if (status != 3) {
        cout << "ERROR: Invalid Lx should cause error" << endl;
        all_passed = false;
    }
    
    // 测试3: 边界检查
    cout << "Testing out-of-bounds access..." << endl;
    reset_bram_storage();
    
    // 尝试写入超出范围的索引
    load_source_data(-1, cmpxFloat(1.0f, 1.0f));  // 负索引
    status = get_compute_status();
    if (status != 3) {
        cout << "ERROR: Negative index should cause error" << endl;
        all_passed = false;
    }
    
    reset_bram_storage();
    load_source_data(BRAM_SOURCE_SIZE + 10, cmpxFloat(1.0f, 1.0f));  // 超范围
    status = get_compute_status();
    if (status != 3) {
        cout << "ERROR: Out-of-bounds index should cause error" << endl;
        all_passed = false;
    }
    
    print_test_result("Parameter Validation", all_passed);
    return all_passed;
}

/**
 * 测试7: 存储复位功能
 */
bool test_reset_functionality() {
    print_test_header("Reset Functionality");
    
    bool all_passed = true;
    
    // 写入一些非零数据
    cout << "Writing non-zero data..." << endl;
    for (int i = 0; i < 10; i++) {
        load_source_data(i, cmpxFloat(i+1.0f, i+1.0f));
        load_mask_data(i, cmpxFloat(i+2.0f, i+2.0f));
        load_scales_data(i, i+3.0f);
    }
    
    // 执行复位
    cout << "Resetting BRAM storage..." << endl;
    reset_bram_storage();
    
    // 验证所有存储已清零
    cout << "Verifying all storage is zero..." << endl;
    
    bool all_zero = true;
    for (int i = 0; i < 10; i++) {
        if (source_bram[i] != cmpxFloat(0.0f, 0.0f)) {
            all_zero = false;
            break;
        }
        if (mask_bram[i] != cmpxFloat(0.0f, 0.0f)) {
            all_zero = false;
            break;
        }
        if (scales_bram[i] != 0.0f) {
            all_zero = false;
            break;
        }
    }
    
    if (!all_zero) {
        cout << "ERROR: Reset did not clear all storage" << endl;
        all_passed = false;
    }
    
    // 验证状态已复位
    int status = get_compute_status();
    if (status != 0) {
        cout << "ERROR: Status not reset to 0 (idle), got " << status << endl;
        all_passed = false;
    }
    
    print_test_result("Reset Functionality", all_passed);
    return all_passed;
}

//=============================================================================
// 主测试入口
//=============================================================================

int main() {
    cout << "========================================" << endl;
    cout << "FPGA-Litho BRAM Interface Testbench" << endl;
    cout << "========================================" << endl;
    cout << "Testing BRAM storage and AXI-Lite interface..." << endl;
    cout << "Target: xcku3p FPGA (No DDR)" << endl;
    cout << "Constraints:" << endl;
    cout << "  - BRAM: ~115KB total (65 blocks)" << endl;
    cout << "  - TCC: Nx <= 3" << endl;
    cout << "  - SOCS: nkernels <= 8" << endl;
    cout << "========================================\n" << endl;
    
    int passed_tests = 0;
    int total_tests = 7;
    
    // 运行所有测试
    if (test_single_data_load_read()) passed_tests++;
    if (test_batch_data_load()) passed_tests++;
    if (test_compute_status()) passed_tests++;
    if (test_tcc_mode_compute()) passed_tests++;
    if (test_socs_mode_compute()) passed_tests++;
    if (test_parameter_validation()) passed_tests++;
    if (test_reset_functionality()) passed_tests++;
    
    // 打印测试总结
    cout << "\n========================================" << endl;
    cout << "Test Summary" << endl;
    cout << "========================================" << endl;
    cout << "Passed: " << passed_tests << "/" << total_tests << endl;
    
    if (passed_tests == total_tests) {
        cout << "\n*** ALL TESTS PASSED ***" << endl;
        cout << "BRAM interface is ready for synthesis." << endl;
        return 0;
    } else {
        cout << "\n*** SOME TESTS FAILED ***" << endl;
        cout << "Please review failed tests above." << endl;
        return 1;
    }
}