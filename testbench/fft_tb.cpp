/*
 * K-Litho HLS Testbench
 * FFT模块测试平台
 * 
 * 参考: interface_stream/fft_tb.cpp
 */

#include <iostream>
#include <fstream>
#include <cmath>
#include <complex>
#include <vector>
#include "../include/hls_types.h"
#include "../include/hls_top.h"

// 注意: 不使用 using namespace std 以避免 std::vector 和 hls::vector 的命名冲突

// ============================================================
// 测试参数
// ============================================================

const int TB_SIZE_X = 32;  // 测试尺寸 (使用较小尺寸加速仿真)
const int TB_SIZE_Y = 32;
const int TB_TOTAL  = TB_SIZE_X * TB_SIZE_Y;

// 允许误差
const float ERROR_THRESHOLD = 1e-3f;

// ============================================================
// 辅助函数
// ============================================================

/**
 * @brief 生成测试数据 (简单正弦波)
 */
void generate_test_data(std::vector<realFloat>& data, int sizeX, int sizeY) {
    for (int y = 0; y < sizeY; y++) {
        for (int x = 0; x < sizeX; x++) {
            // 生成简单的正弦波信号
            float val = std::sin(2.0f * M_PI * x / sizeX) * std::cos(2.0f * M_PI * y / sizeY);
            data[y * sizeX + x] = val;
        }
    }
}

/**
 * @brief 生成随机测试数据
 */
void generate_random_data(std::vector<realFloat>& data, int size) {
    for (int i = 0; i < size; i++) {
        data[i] = (std::rand() % 1000) / 1000.0f - 0.5f;
    }
}

/**
 * @brief 打印数据 (用于调试)
 */
void print_data(const std::vector<realFloat>& data, int sizeX, int sizeY, const char* name) {
    std::cout << "=== " << name << " ===" << std::endl;
    for (int y = 0; y < (sizeY < 8 ? sizeY : 8); y++) {
        for (int x = 0; x < (sizeX < 8 ? sizeX : 8); x++) {
            std::cout << data[y * sizeX + x] << " ";
        }
        std::cout << std::endl;
    }
    std::cout << "..." << std::endl;
}

/**
 * @brief 计算误差统计
 */
void compute_error_stats(const std::vector<realFloat>& expected, 
                         const std::vector<realFloat>& actual,
                         float& max_error, float& avg_error) {
    max_error = 0.0f;
    avg_error = 0.0f;
    
    for (size_t i = 0; i < expected.size(); i++) {
        float err = std::abs(expected[i] - actual[i]);
        if (err > max_error) max_error = err;
        avg_error += err;
    }
    avg_error /= expected.size();
}

/**
 * @brief 写入结果文件
 */
void write_result_file(const char* filename, const std::vector<realFloat>& data) {
    std::ofstream ofs(filename);
    if (!ofs.is_open()) {
        std::cerr << "Error: Cannot open file " << filename << std::endl;
        return;
    }
    
    for (size_t i = 0; i < data.size(); i++) {
        ofs << data[i] << std::endl;
    }
    ofs.close();
    std::cout << "Result saved to " << filename << std::endl;
}

// ============================================================
// 主测试函数
// ============================================================

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "K-Litho HLS FFT Testbench" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Test size: " << TB_SIZE_X << " x " << TB_SIZE_Y << std::endl;
    std::cout << "Error threshold: " << ERROR_THRESHOLD << std::endl;
    std::cout << std::endl;

    // 测试数据
    std::vector<realFloat> input_data(TB_TOTAL);
    std::vector<realFloat> output_data(TB_TOTAL);

    // HLS流对象
    hls::stream<realFloat> data_in("data_in");
    hls::stream<realFloat> data_out("data_out");

    // ============================================================
    // Test 1: 正弦波测试
    // ============================================================
    
    std::cout << "Test 1: Sine wave input" << std::endl;
    
    // 生成测试数据
    generate_test_data(input_data, TB_SIZE_X, TB_SIZE_Y);
    
    // 写入输入流
    for (int i = 0; i < TB_TOTAL; i++) {
        data_in.write(input_data[i]);
    }
    
    // 执行FFT/IFFT流程
    hls_top_simple(data_in, data_out, TB_SIZE_X, TB_SIZE_Y);
    
    // 读取输出
    for (int i = 0; i < TB_TOTAL; i++) {
        output_data[i] = data_out.read();
    }
    
    // 计算误差
    float max_error, avg_error;
    compute_error_stats(input_data, output_data, max_error, avg_error);
    
    std::cout << "  Max error: " << max_error << std::endl;
    std::cout << "  Avg error: " << avg_error << std::endl;
    
    // 验证结果
    int test1_pass = (max_error < ERROR_THRESHOLD) ? 1 : 0;
    std::cout << "  Test 1 result: " << (test1_pass ? "PASS" : "FAIL") << std::endl;
    std::cout << std::endl;

    // ============================================================
    // Test 2: 随机数据测试
    // ============================================================
    
    std::cout << "Test 2: Random input" << std::endl;
    
    // 重置流
    hls::stream<realFloat> data_in2("data_in2");
    hls::stream<realFloat> data_out2("data_out2");
    
    // 生成随机数据
    generate_random_data(input_data, TB_TOTAL);
    
    // 写入输入流
    for (int i = 0; i < TB_TOTAL; i++) {
        data_in2.write(input_data[i]);
    }
    
    // 执行FFT/IFFT流程
    hls_top_simple(data_in2, data_out2, TB_SIZE_X, TB_SIZE_Y);
    
    // 读取输出
    for (int i = 0; i < TB_TOTAL; i++) {
        output_data[i] = data_out2.read();
    }
    
    // 计算误差
    compute_error_stats(input_data, output_data, max_error, avg_error);
    
    std::cout << "  Max error: " << max_error << std::endl;
    std::cout << "  Avg error: " << avg_error << std::endl;
    
    // 验证结果
    int test2_pass = (max_error < ERROR_THRESHOLD) ? 1 : 0;
    std::cout << "  Test 2 result: " << (test2_pass ? "PASS" : "FAIL") << std::endl;
    std::cout << std::endl;

    // ============================================================
    // Test 3: 常数输入测试
    // ============================================================
    
    std::cout << "Test 3: Constant input" << std::endl;
    
    hls::stream<realFloat> data_in3("data_in3");
    hls::stream<realFloat> data_out3("data_out3");
    
    // 常数输入
    for (int i = 0; i < TB_TOTAL; i++) {
        input_data[i] = 1.0f;
        data_in3.write(1.0f);
    }
    
    // 执行FFT/IFFT流程
    hls_top_simple(data_in3, data_out3, TB_SIZE_X, TB_SIZE_Y);
    
    // 读取输出
    for (int i = 0; i < TB_TOTAL; i++) {
        output_data[i] = data_out3.read();
    }
    
    // 对于常数输入, FFT后只有DC分量, IFFT后应恢复常数
    // 但由于归一化, 结果应为输入值
    compute_error_stats(input_data, output_data, max_error, avg_error);
    
    std::cout << "  Max error: " << max_error << std::endl;
    std::cout << "  Avg error: " << avg_error << std::endl;
    
    int test3_pass = (max_error < ERROR_THRESHOLD) ? 1 : 0;
    std::cout << "  Test 3 result: " << (test3_pass ? "PASS" : "FAIL") << std::endl;
    std::cout << std::endl;

    // ============================================================
    // 结果汇总
    // ============================================================
    
    std::cout << "========================================" << std::endl;
    std::cout << "Test Summary" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Test 1 (Sine wave):    " << (test1_pass ? "PASS" : "FAIL") << std::endl;
    std::cout << "Test 2 (Random):       " << (test2_pass ? "PASS" : "FAIL") << std::endl;
    std::cout << "Test 3 (Constant):     " << (test3_pass ? "PASS" : "FAIL") << std::endl;
    std::cout << std::endl;
    
    int all_pass = test1_pass && test2_pass && test3_pass;
    std::cout << "Overall: " << (all_pass ? "ALL TESTS PASSED" : "SOME TESTS FAILED") << std::endl;
    
    // 写入结果文件
    write_result_file("data/tb_output.res", output_data);
    
    return all_pass ? 0 : 1;
}