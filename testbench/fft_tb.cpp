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
// 官方stimulus文件格式支持
// ============================================================

/**
 * @brief 误差统计结构体
 */
struct error_stats {
    double max_error;
    double avg_error;
    double rms_error;
    int error_count;
};

/**
 * @brief 从十六进制字符串解析定点数
 * 官方格式: hex_re hex_im float_re float_im
 */
cmpxFixedOut parse_hex_to_fixed(const std::string& hex_re, const std::string& hex_im) {
    // 解析16位有符号整数
    int16_t re_int = (int16_t)std::stoi(hex_re, nullptr, 16);
    int16_t im_int = (int16_t)std::stoi(hex_im, nullptr, 16);
    
    // 转换为定点数 (ap_fixed<16,1>)
    // 范围: [-1, 1) 对应 [-32768, 32767]
    const double scale = 1.0 / 32768.0;  // 2^15
    fft_data_out_t re_val(re_int * scale);
    fft_data_out_t im_val(im_int * scale);
    
    return cmpxFixedOut(re_val, im_val);
}

/**
 * @brief 读取官方.golden结果文件
 * 格式: status1 status2 data_lines...
 * 每行: hex_re hex_im float_re float_im
 */
bool read_golden_res(const char* filename,
                     std::vector<cmpxFixedOut>& golden_data) {
    std::ifstream ifs(filename);
    if (!ifs.is_open()) {
        std::cerr << "Warning: Cannot open golden file " << filename << std::endl;
        return false;
    }
    
    std::string line;
    int line_num = 0;
    
    // 跳过前两行状态信息
    std::getline(ifs, line); line_num++;
    std::getline(ifs, line); line_num++;
    
    while (std::getline(ifs, line)) {
        line_num++;
        if (line.empty()) continue;
        
        // 解析数据行: hex_re hex_im float_re float_im
        std::istringstream iss(line);
        std::string hex_re, hex_im;
        float float_re, float_im;
        
        if (iss >> hex_re >> hex_im >> float_re >> float_im) {
            cmpxFixedOut val = parse_hex_to_fixed(hex_re, hex_im);
            golden_data.push_back(val);
        }
    }
    
    ifs.close();
    std::cout << "Loaded " << golden_data.size() << " golden samples from " << filename << std::endl;
    return true;
}

/**
 * @brief 对比输出与黄金结果
 */
bool verify_against_golden(const std::vector<cmpxFixedOut>& output,
                          const std::vector<cmpxFixedOut>& golden,
                          error_stats* stats) {
    if (output.size() != golden.size()) {
        std::cerr << "Error: Size mismatch - output=" << output.size() 
                  << ", golden=" << golden.size() << std::endl;
        return false;
    }
    
    stats->max_error = 0.0;
    stats->avg_error = 0.0;
    stats->rms_error = 0.0;
    stats->error_count = 0;
    
    const double threshold = ERROR_THRESHOLD;
    
    for (size_t i = 0; i < output.size(); i++) {
        double err_re = std::abs(output[i].real().to_float() - golden[i].real().to_float());
        double err_im = std::abs(output[i].imag().to_float() - golden[i].imag().to_float());
        double err = std::max(err_re, err_im);
        
        if (err > stats->max_error) stats->max_error = err;
        stats->avg_error += err;
        stats->rms_error += err * err;
        
        if (err > threshold) stats->error_count++;
    }
    
    stats->avg_error /= output.size();
    stats->rms_error = std::sqrt(stats->rms_error / output.size());
    
    return stats->max_error <= threshold;
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
    // Test 4: 溢出检测测试 (scaled模式应防止溢出)
    // ============================================================
    
    std::cout << "Test 4: Overflow detection (scaled mode)" << std::endl;
    
    hls::stream<realFloat> data_in4("data_in4");
    hls::stream<realFloat> data_out4("data_out4");
    
    // 大幅度输入测试: 接近定点范围上限
    // ap_fixed<16,1> 范围约 [-2, 2)
    // 测试输入幅度 0.9 (接近上限但应安全)
    for (int i = 0; i < TB_TOTAL; i++) {
        float large_val = 0.9f * ((i % 2 == 0) ? 1.0f : -1.0f);
        input_data[i] = large_val;
        data_in4.write(large_val);
    }
    
    // 执行FFT/IFFT流程
    hls_top_simple(data_in4, data_out4, TB_SIZE_X, TB_SIZE_Y);
    
    // 读取输出
    for (int i = 0; i < TB_TOTAL; i++) {
        output_data[i] = data_out4.read();
    }
    
    // 验证scaled模式是否有效防止溢出
    // 即使输入幅度较大，输出仍应正确恢复
    compute_error_stats(input_data, output_data, max_error, avg_error);
    
    std::cout << "  Input amplitude: 0.9 (large)" << std::endl;
    std::cout << "  Max error: " << max_error << std::endl;
    std::cout << "  Avg error: " << avg_error << std::endl;
    
    // 检查是否有明显的溢出迹象 (误差突然增大)
    int test4_pass = (max_error < ERROR_THRESHOLD * 2.0f) ? 1 : 0;  // 允许稍大误差
    std::cout << "  Overflow prevention: " << (test4_pass ? "OK" : "FAIL (possible overflow)") << std::endl;
    std::cout << "  Test 4 result: " << (test4_pass ? "PASS" : "FAIL") << std::endl;
    std::cout << std::endl;

    // ============================================================
    // Test 5: 动态缩放调度测试
    // ============================================================
    
    std::cout << "Test 5: Dynamic scaling schedule" << std::endl;
    
    // 测试 compute_scaling_schedule 函数
    float test_amplitudes[] = {0.001f, 0.01f, 0.1f, 0.5f, 0.9f};
    int test_stages = FFT_NFFT_MAX;  // 10级
    
    std::cout << "  Testing compute_scaling_schedule():" << std::endl;
    for (int i = 0; i < 5; i++) {
        ap_uint<15> schedule = compute_scaling_schedule(test_amplitudes[i], test_stages);
        std::cout << "    input_max=" << test_amplitudes[i] 
                  << " -> schedule=0x" << std::hex << schedule << std::dec << std::endl;
    }
    
    // 验证: 大幅度输入应产生更大的缩放值
    ap_uint<15> schedule_small = compute_scaling_schedule(0.001f, test_stages);
    ap_uint<15> schedule_large = compute_scaling_schedule(0.5f, test_stages);
    
    int test5_pass = (schedule_large >= schedule_small) ? 1 : 0;
    std::cout << "  Scaling logic: " << (test5_pass ? "CORRECT" : "INCORRECT") << std::endl;
    std::cout << "  Test 5 result: " << (test5_pass ? "PASS" : "FAIL") << std::endl;
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
    std::cout << "Test 4 (Overflow):     " << (test4_pass ? "PASS" : "FAIL") << std::endl;
    std::cout << "Test 5 (Dyn scaling):  " << (test5_pass ? "PASS" : "FAIL") << std::endl;
    std::cout << std::endl;
    
    int all_pass = test1_pass && test2_pass && test3_pass && test4_pass && test5_pass;
    std::cout << "Overall: " << (all_pass ? "ALL TESTS PASSED" : "SOME TESTS FAILED") << std::endl;
    std::cout << "========================================" << std::endl;
    
    if (all_pass) {
        std::cout << "ALL TESTS PASSED!" << std::endl;
    }
    
    // 写入结果文件
    write_result_file("data/tb_output.res", output_data);
    
    return all_pass ? 0 : 1;
}