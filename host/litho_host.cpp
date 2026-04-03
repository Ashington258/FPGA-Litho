/**
 * @file litho_host.cpp
 * @brief K-Litho XRT Host Application
 * 
 * XRT/OpenCL主机程序，用于控制K-Litho光刻模拟FPGA内核
 * 
 * 支持两种工作模式:
 * - TCC模式: 计算频域图像 (mode=1)
 * - SOCS模式: 计算空间域图像 (mode=2)
 * 
 * @author K-Litho Team
 * @date 2026-04-03
 */

#include <iostream>
#include <fstream>
#include <vector>
#include <complex>
#include <chrono>
#include <cstring>
#include <algorithm>

// XRT headers
#include "xrt/xrt_device.h"
#include "xrt/xrt_kernel.h"
#include "xrt/xrt_bo.h"

// 项目头文件
#include "../include/hls_litho_system.h"

//=============================================================================
// 命令行参数解析
//=============================================================================

struct HostArgs {
    std::string xclbin_file;        // XCLBIN文件路径
    int device_index = 0;           // 设备索引
    int mode = 1;                   // 工作模式: 1=TCC, 2=SOCS
    int num_runs = 1;               // 运行次数
    bool verbose = false;           // 详细输出
    
    // 光学参数
    float lambda = 193.0f;          // 波长 (nm)
    float NA = 1.35f;               // 数值孔径
    float defocus = 0.0f;           // 离焦量 (nm)
    
    // 尺寸参数
    int Lx = 64;                    // 频域X尺寸
    int Ly = 64;                    // 频域Y尺寸
    int Nx = 3;                     // TCC/SOCS半宽
    int Ny = 3;                     // TCC/SOCS半高
    int srcSize = 32;               // 光源尺寸 (TCC模式)
    int nkernels = 4;               // SOCS核数量 (SOCS模式)
    
    // 数据文件路径
    std::string source_file;        // 光源数据文件
    std::string mask_file;          // 掩模数据文件
    std::string tcc_file;           // TCC矩阵文件
    std::string kernels_file;       // SOCS核文件
    std::string scales_file;        // SOCS权重文件
    std::string output_file;        // 输出文件
};

void print_usage(const char* prog_name) {
    std::cout << "K-Litho XRT Host Application\n";
    std::cout << "Usage: " << prog_name << " [options]\n\n";
    std::cout << "Options:\n";
    std::cout << "  --xclbin <file>       XCLBIN file path (required)\n";
    std::cout << "  --device <index>      Device index (default: 0)\n";
    std::cout << "  --mode <1|2>          Mode: 1=TCC, 2=SOCS (default: 1)\n";
    std::cout << "  --runs <n>            Number of runs (default: 1)\n";
    std::cout << "  --verbose             Enable verbose output\n";
    std::cout << "\nOptical Parameters:\n";
    std::cout << "  --lambda <nm>         Wavelength (default: 193nm)\n";
    std::cout << "  --NA <value>          Numerical aperture (default: 1.35)\n";
    std::cout << "  --defocus <nm>        Defocus amount (default: 0)\n";
    std::cout << "\nSize Parameters:\n";
    std::cout << "  --Lx <size>           Frequency domain X size (default: 64)\n";
    std::cout << "  --Ly <size>           Frequency domain Y size (default: 64)\n";
    std::cout << "  --Nx <size>           TCC/SOCS half-width (default: 3)\n";
    std::cout << "  --Ny <size>           TCC/SOCS half-height (default: 3)\n";
    std::cout << "  --srcSize <size>      Source size for TCC mode (default: 32)\n";
    std::cout << "  --nkernels <n>        Number of SOCS kernels (default: 4)\n";
    std::cout << "\nData Files:\n";
    std::cout << "  --source <file>       Source data file\n";
    std::cout << "  --mask <file>         Mask FFT data file\n";
    std::cout << "  --tcc <file>          TCC matrix file\n";
    std::cout << "  --kernels <file>      SOCS kernels file\n";
    std::cout << "  --scales <file>       SOCS scales file\n";
    std::cout << "  --output <file>       Output result file\n";
}

bool parse_args(int argc, char* argv[], HostArgs& args) {
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "--xclbin" && i + 1 < argc) {
            args.xclbin_file = argv[++i];
        } else if (arg == "--device" && i + 1 < argc) {
            args.device_index = std::stoi(argv[++i]);
        } else if (arg == "--mode" && i + 1 < argc) {
            args.mode = std::stoi(argv[++i]);
        } else if (arg == "--runs" && i + 1 < argc) {
            args.num_runs = std::stoi(argv[++i]);
        } else if (arg == "--verbose") {
            args.verbose = true;
        } else if (arg == "--lambda" && i + 1 < argc) {
            args.lambda = std::stof(argv[++i]);
        } else if (arg == "--NA" && i + 1 < argc) {
            args.NA = std::stof(argv[++i]);
        } else if (arg == "--defocus" && i + 1 < argc) {
            args.defocus = std::stof(argv[++i]);
        } else if (arg == "--Lx" && i + 1 < argc) {
            args.Lx = std::stoi(argv[++i]);
        } else if (arg == "--Ly" && i + 1 < argc) {
            args.Ly = std::stoi(argv[++i]);
        } else if (arg == "--Nx" && i + 1 < argc) {
            args.Nx = std::stoi(argv[++i]);
        } else if (arg == "--Ny" && i + 1 < argc) {
            args.Ny = std::stoi(argv[++i]);
        } else if (arg == "--srcSize" && i + 1 < argc) {
            args.srcSize = std::stoi(argv[++i]);
        } else if (arg == "--nkernels" && i + 1 < argc) {
            args.nkernels = std::stoi(argv[++i]);
        } else if (arg == "--source" && i + 1 < argc) {
            args.source_file = argv[++i];
        } else if (arg == "--mask" && i + 1 < argc) {
            args.mask_file = argv[++i];
        } else if (arg == "--tcc" && i + 1 < argc) {
            args.tcc_file = argv[++i];
        } else if (arg == "--kernels" && i + 1 < argc) {
            args.kernels_file = argv[++i];
        } else if (arg == "--scales" && i + 1 < argc) {
            args.scales_file = argv[++i];
        } else if (arg == "--output" && i + 1 < argc) {
            args.output_file = argv[++i];
        } else if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            return false;
        } else {
            std::cerr << "Unknown argument: " << arg << "\n";
            print_usage(argv[0]);
            return false;
        }
    }
    
    if (args.xclbin_file.empty()) {
        std::cerr << "Error: XCLBIN file path is required\n";
        print_usage(argv[0]);
        return false;
    }
    
    return true;
}

//=============================================================================
// 数据加载和生成辅助函数
//=============================================================================

// 复数数据类型 (与HLS内核匹配)
using ComplexFloat = std::complex<float>;

// 从文件加载复数数据
bool load_complex_data(const std::string& filename, 
                       std::vector<ComplexFloat>& data, 
                       int expected_size,
                       bool verbose = false) {
    if (filename.empty()) {
        if (verbose) std::cout << "No file specified, generating test data\n";
        generate_test_complex_data(data, expected_size);
        return true;
    }
    
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open file " << filename << "\n";
        generate_test_complex_data(data, expected_size);
        return false;
    }
    
    // 读取复数数据 (格式: float real, float imag)
    data.resize(expected_size);
    for (int i = 0; i < expected_size; i++) {
        float real, imag;
        if (!file.read(reinterpret_cast<char*>(&real), sizeof(float)) ||
            !file.read(reinterpret_cast<char*>(&imag), sizeof(float))) {
            std::cerr << "Warning: File truncated, filling with test data\n";
            generate_test_complex_data(data, expected_size);
            return true;
        }
        data[i] = ComplexFloat(real, imag);
    }
    
    if (verbose) std::cout << "Loaded " << expected_size << " complex values from " << filename << "\n";
    return true;
}

// 从文件加载浮点数据
bool load_float_data(const std::string& filename, 
                     std::vector<float>& data, 
                     int expected_size,
                     bool verbose = false) {
    if (filename.empty()) {
        if (verbose) std::cout << "No file specified, generating test data\n";
        generate_test_float_data(data, expected_size);
        return true;
    }
    
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open file " << filename << "\n";
        generate_test_float_data(data, expected_size);
        return false;
    }
    
    data.resize(expected_size);
    if (!file.read(reinterpret_cast<char*>(data.data()), expected_size * sizeof(float))) {
        std::cerr << "Warning: File truncated, filling with test data\n";
        generate_test_float_data(data, expected_size);
        return true;
    }
    
    if (verbose) std::cout << "Loaded " << expected_size << " float values from " << filename << "\n";
    return true;
}

// 生成测试复数数据
void generate_test_complex_data(std::vector<ComplexFloat>& data, int size) {
    data.resize(size);
    for (int i = 0; i < size; i++) {
        // 生成有规律的测试数据
        float real = static_cast<float>((i % 100) / 100.0);
        float imag = static_cast<float>((i % 50) / 50.0);
        data[i] = ComplexFloat(real, imag);
    }
}

// 生成测试浮点数据
void generate_test_float_data(std::vector<float>& data, int size) {
    data.resize(size);
    for (int i = 0; i < size; i++) {
        data[i] = static_cast<float>((i % 10) / 10.0);
    }
}

// 保存复数数据到文件
bool save_complex_data(const std::string& filename, 
                       const std::vector<ComplexFloat>& data,
                       bool verbose = false) {
    if (filename.empty()) return true;
    
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot create output file " << filename << "\n";
        return false;
    }
    
    for (const auto& c : data) {
        float real = c.real();
        float imag = c.imag();
        file.write(reinterpret_cast<char*>(&real), sizeof(float));
        file.write(reinterpret_cast<char*>(&imag), sizeof(float));
    }
    
    if (verbose) std::cout << "Saved " << data.size() << " complex values to " << filename << "\n";
    return true;
}

// 保存浮点数据到文件
bool save_float_data(const std::string& filename, 
                     const std::vector<float>& data,
                     bool verbose = false) {
    if (filename.empty()) return true;
    
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot create output file " << filename << "\n";
        return false;
    }
    
    file.write(reinterpret_cast<const char*>(data.data()), data.size() * sizeof(float));
    
    if (verbose) std::cout << "Saved " << data.size() << " float values to " << filename << "\n";
    return true;
}

//=============================================================================
// XRT设备管理
//=============================================================================

class LithoDevice {
public:
    LithoDevice(int device_index, const std::string& xclbin_file, bool verbose = false)
        : verbose_(verbose) {
        
        // 获取设备
        auto devices = xrt::device::get_devices();
        if (devices.empty()) {
            throw std::runtime_error("No XRT devices found");
        }
        
        if (device_index >= devices.size()) {
            throw std::runtime_error("Device index out of range");
        }
        
        device_ = devices[device_index];
        if (verbose_) {
            std::cout << "Device: " << device_.get_name() << "\n";
            std::cout << "BDF: " << device_.get_bdf() << "\n";
        }
        
        // 加载xclbin
        xclbin_ = xrt::xclbin(xclbin_file);
        device_.load_xclbin(xclbin_);
        
        if (verbose_) {
            std::cout << "Loaded xclbin: " << xclbin_file << "\n";
        }
        
        // 获取内核UUID
        kernel_uuid_ = xclbin_.get_uuid();
    }
    
    xrt::device& get_device() { return device_; }
    xrt::uuid get_kernel_uuid() { return kernel_uuid_; }
    
    void print_info() {
        std::cout << "\n=== Device Information ===\n";
        std::cout << "Name: " << device_.get_name() << "\n";
        std::cout << "BDF: " << device_.get_bdf() << "\n";
        std::cout << "Kernel UUID: " << kernel_uuid_.to_string() << "\n";
    }
    
private:
    xrt::device device_;
    xrt::xclbin xclbin_;
    xrt::uuid kernel_uuid_;
    bool verbose_;
};

//=============================================================================
// K-Litho内核管理
//=============================================================================

class LithoKernel {
public:
    LithoKernel(LithoDevice& device, const std::string& kernel_name = "hls_litho_system", 
                bool verbose = false)
        : device_(device), verbose_(verbose) {
        
        // 创建内核
        kernel_ = xrt::kernel(device_.get_device(), device_.get_kernel_uuid(), kernel_name);
        
        if (verbose_) {
            std::cout << "Created kernel: " << kernel_name << "\n";
            std::cout << "Number of arguments: " << kernel_.get_num_args() << "\n";
        }
    }
    
    xrt::kernel& get_kernel() { return kernel_; }
    
    // 创建缓冲对象
    xrt::bo create_buffer(size_t size, xrt::memory_group bank = 0) {
        return xrt::bo(device_.get_device(), size, bank);
    }
    
    // 创建复数数据缓冲
    xrt::bo create_complex_buffer(const std::vector<ComplexFloat>& data, 
                                  xrt::memory_group bank = 0) {
        size_t size = data.size() * sizeof(ComplexFloat);
        xrt::bo bo = create_buffer(size, bank);
        
        // 复制数据到缓冲
        void* bo_map = bo.map();
        memcpy(bo_map, data.data(), size);
        bo.sync(XCL_BO_SYNC_BO_TO_DEVICE);
        
        return bo;
    }
    
    // 创建浮点数据缓冲
    xrt::bo create_float_buffer(const std::vector<float>& data, 
                                xrt::memory_group bank = 0) {
        size_t size = data.size() * sizeof(float);
        xrt::bo bo = create_buffer(size, bank);
        
        void* bo_map = bo.map();
        memcpy(bo_map, data.data(), size);
        bo.sync(XCL_BO_SYNC_BO_TO_DEVICE);
        
        return bo;
    }
    
    // 创建输出缓冲
    xrt::bo create_output_buffer(size_t size, xrt::memory_group bank = 0) {
        return create_buffer(size, bank);
    }
    
    // 读取输出缓冲到复数向量
    void read_complex_buffer(xrt::bo& bo, std::vector<ComplexFloat>& data, size_t count) {
        bo.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
        void* bo_map = bo.map();
        memcpy(data.data(), bo_map, count * sizeof(ComplexFloat));
    }
    
    // 读取输出缓冲到浮点向量
    void read_float_buffer(xrt::bo& bo, std::vector<float>& data, size_t count) {
        bo.sync(XCL_BO_SYNC_BO_FROM_DEVICE);
        void* bo_map = bo.map();
        memcpy(data.data(), bo_map, count * sizeof(float));
    }
    
private:
    LithoDevice& device_;
    xrt::kernel kernel_;
    bool verbose_;
};

//=============================================================================
// 主运行函数
//=============================================================================

int run_litho(HostArgs& args) {
    try {
        // 初始化设备
        LithoDevice device(args.device_index, args.xclbin_file, args.verbose);
        device.print_info();
        
        // 创建内核
        LithoKernel kernel(device, "hls_litho_system", args.verbose);
        
        //=====================================================================
        // 准备输入数据
        //=====================================================================
        
        // 计算数据大小
        int tcc_dim = (2 * args.Nx + 1) * (2 * args.Ny + 1);
        int tcc_total = tcc_dim * tcc_dim;
        int output_size = (4 * args.Nx + 1) * (4 * args.Ny + 1);
        
        // 加载/生成输入数据
        std::vector<ComplexFloat> source_data;
        std::vector<ComplexFloat> mask_fft_data;
        std::vector<ComplexFloat> tcc_data;
        std::vector<ComplexFloat> kernels_data;
        std::vector<float> scales_data;
        
        load_complex_data(args.source_file, source_data, 
                         args.srcSize * args.srcSize, args.verbose);
        load_complex_data(args.mask_file, mask_fft_data, 
                         args.Lx * args.Ly, args.verbose);
        load_complex_data(args.tcc_file, tcc_data, tcc_total, args.verbose);
        load_complex_data(args.kernels_file, kernels_data, 
                         args.nkernels * tcc_dim, args.verbose);
        load_float_data(args.scales_file, scales_data, args.nkernels, args.verbose);
        
        //=====================================================================
        // 创建XRT缓冲对象
        //=====================================================================
        
        if (args.verbose) {
            std::cout << "\n=== Creating Buffer Objects ===\n";
        }
        
        // 输入缓冲
        xrt::bo source_bo = kernel.create_complex_buffer(source_data);
        xrt::bo mask_bo = kernel.create_complex_buffer(mask_fft_data);
        xrt::bo tcc_bo = kernel.create_complex_buffer(tcc_data);
        xrt::bo kernels_bo = kernel.create_complex_buffer(kernels_data);
        xrt::bo scales_bo = kernel.create_float_buffer(scales_data);
        
        // 输出缓冲
        xrt::bo imgf_bo = kernel.create_output_buffer(args.Lx * args.Ly * sizeof(ComplexFloat));
        xrt::bo img_out_bo = kernel.create_output_buffer(output_size * sizeof(float));
        
        //=====================================================================
        // 运行内核
        //=====================================================================
        
        std::vector<long> execution_times;
        
        for (int run = 0; run < args.num_runs; run++) {
            if (args.verbose) {
                std::cout << "\n=== Run " << (run + 1) << "/" << args.num_runs << " ===\n";
            }
            
            auto start_time = std::chrono::high_resolution_clock::now();
            
            // 创建运行对象
            xrt::run run_obj = kernel.get_kernel()(source_bo, mask_bo, tcc_bo, 
                                                   kernels_bo, scales_bo,
                                                   imgf_bo, img_out_bo,
                                                   args.lambda, args.NA, args.defocus,
                                                   args.Lx, args.Ly,
                                                   args.Nx, args.Ny,
                                                   args.srcSize, args.nkernels,
                                                   args.mode);
            
            // 等待完成
            run_obj.wait();
            
            auto end_time = std::chrono::high_resolution_clock::now();
            long duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
            execution_times.push_back(duration);
            
            if (args.verbose) {
                std::cout << "Execution time: " << duration << " μs\n";
            }
            
            //=================================================================
            // 读取结果
            //=================================================================
            
            if (args.mode == 1) {
                // TCC模式: 读取频域输出
                std::vector<ComplexFloat> imgf_result(args.Lx * args.Ly);
                kernel.read_complex_buffer(imgf_bo, imgf_result, args.Lx * args.Ly);
                
                // 保存结果
                save_complex_data(args.output_file, imgf_result, args.verbose);
                
                // 统计输出
                if (args.verbose || run == args.num_runs - 1) {
                    int non_zero = 0;
                    float max_mag = 0.0f;
                    for (const auto& c : imgf_result) {
                        float mag = std::abs(c);
                        if (mag > 1e-6) {
                            non_zero++;
                            max_mag = std::max(max_mag, mag);
                        }
                    }
                    std::cout << "TCC Mode Result: " << non_zero << " non-zero elements, Max magnitude: " << max_mag << "\n";
                }
                
            } else if (args.mode == 2) {
                // SOCS模式: 读取空间域输出
                std::vector<float> img_out_result(output_size);
                kernel.read_float_buffer(img_out_bo, img_out_result, output_size);
                
                // 保存结果
                save_float_data(args.output_file, img_out_result, args.verbose);
                
                // 统计输出
                if (args.verbose || run == args.num_runs - 1) {
                    int non_zero = 0;
                    float max_val = 0.0f;
                    for (float v : img_out_result) {
                        if (v > 1e-6) {
                            non_zero++;
                            max_val = std::max(max_val, v);
                        }
                    }
                    std::cout << "SOCS Mode Result: " << non_zero << " non-zero pixels, Max value: " << max_val << "\n";
                }
            }
        }
        
        //=====================================================================
        // 性能统计
        //=====================================================================
        
        if (args.num_runs > 1) {
            long min_time = *std::min_element(execution_times.begin(), execution_times.end());
            long max_time = *std::max_element(execution_times.begin(), execution_times.end());
            long avg_time = 0;
            for (long t : execution_times) avg_time += t;
            avg_time /= execution_times.size();
            
            std::cout << "\n=== Performance Statistics ===\n";
            std::cout << "Runs: " << args.num_runs << "\n";
            std::cout << "Min time: " << min_time << " μs\n";
            std::cout << "Max time: " << max_time << " μs\n";
            std::cout << "Avg time: " << avg_time << " μs\n";
        }
        
        std::cout << "\nK-Litho execution completed successfully!\n";
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}

//=============================================================================
// 主函数
//=============================================================================

int main(int argc, char* argv[]) {
    std::cout << "========================================\n";
    std::cout << "  K-Litho XRT Host Application\n";
    std::cout << "  (TCC & SOCS Lithography Simulation)\n";
    std::cout << "========================================\n\n";
    
    HostArgs args;
    if (!parse_args(argc, argv, args)) {
        return 1;
    }
    
    std::cout << "Configuration:\n";
    std::cout << "  Mode: " << (args.mode == 1 ? "TCC" : "SOCS") << "\n";
    std::cout << "  Lambda: " << args.lambda << " nm\n";
    std::cout << "  NA: " << args.NA << "\n";
    std::cout << "  Defocus: " << args.defocus << " nm\n";
    std::cout << "  Lx/Ly: " << args.Lx << "/" << args.Ly << "\n";
    std::cout << "  Nx/Ny: " << args.Nx << "/" << args.Ny << "\n";
    if (args.mode == 1) {
        std::cout << "  Source size: " << args.srcSize << "\n";
    } else {
        std::cout << "  Num kernels: " << args.nkernels << "\n";
    }
    std::cout << "\n";
    
    return run_litho(args);
}