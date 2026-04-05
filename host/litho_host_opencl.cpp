/**
 * @file litho_host_opencl.cpp
 * @brief FPGA-Litho OpenCL Host Application (Alternative implementation)
 * 
 * 使用标准OpenCL API的主机程序，兼容更多平台
 * 
 * @author FPGA-Litho Team
 * @date 2026-04-03
 */

#include <CL/cl2.hpp>
#include <iostream>
#include <fstream>
#include <vector>
#include <complex>
#include <chrono>
#include <cstring>
#include <cstdlib>

using ComplexFloat = std::complex<float>;

//=============================================================================
// 常量定义 (与HLS内核匹配)
//=============================================================================

constexpr int SYS_MAX_LX = 64;
constexpr int SYS_MAX_LY = 64;
constexpr int SYS_MAX_NX = 7;
constexpr int SYS_MAX_NY = 7;
constexpr int SYS_MAX_KERNELS = 8;
constexpr int SYS_MAX_SRC_SIZE = 64;

//=============================================================================
// OpenCL辅助函数
//=============================================================================

const char* getErrorString(cl_int err) {
    switch (err) {
        case CL_SUCCESS: return "Success";
        case CL_DEVICE_NOT_FOUND: return "Device not found";
        case CL_DEVICE_NOT_AVAILABLE: return "Device not available";
        case CL_COMPILER_NOT_AVAILABLE: return "Compiler not available";
        case CL_MEM_OBJECT_ALLOCATION_FAILURE: return "Memory allocation failure";
        case CL_OUT_OF_RESOURCES: return "Out of resources";
        case CL_OUT_OF_HOST_MEMORY: return "Out of host memory";
        case CL_INVALID_DEVICE: return "Invalid device";
        case CL_INVALID_COMMAND_QUEUE: return "Invalid command queue";
        case CL_INVALID_MEM_OBJECT: return "Invalid memory object";
        case CL_INVALID_PROGRAM: return "Invalid program";
        case CL_INVALID_KERNEL: return "Invalid kernel";
        case CL_INVALID_ARG_INDEX: return "Invalid argument index";
        case CL_INVALID_ARG_VALUE: return "Invalid argument value";
        case CL_INVALID_ARG_SIZE: return "Invalid argument size";
        case CL_INVALID_KERNEL_ARGS: return "Invalid kernel arguments";
        case CL_INVALID_WORK_DIMENSION: return "Invalid work dimension";
        case CL_INVALID_WORK_GROUP_SIZE: return "Invalid work group size";
        default: return "Unknown error";
    }
}

void checkError(cl_int err, const char* operation) {
    if (err != CL_SUCCESS) {
        std::cerr << "Error during " << operation << ": " << getErrorString(err) << "\n";
        exit(1);
    }
}

//=============================================================================
// OpenCL设备初始化
//=============================================================================

class OpenCLDevice {
public:
    cl::Platform platform;
    cl::Device device;
    cl::Context context;
    cl::CommandQueue queue;
    
    OpenCLDevice(int platform_id = 0, int device_id = 0, bool verbose = false) {
        // 获取平台
        std::vector<cl::Platform> platforms;
        cl_int err = cl::Platform::get(&platforms);
        checkError(err, "getting platforms");
        
        if (platforms.empty()) {
            throw std::runtime_error("No OpenCL platforms found");
        }
        
        if (platform_id >= platforms.size()) {
            throw std::runtime_error("Platform index out of range");
        }
        
        platform = platforms[platform_id];
        
        if (verbose) {
            std::string name;
            platform.getInfo(CL_PLATFORM_NAME, &name);
            std::cout << "Platform: " << name << "\n";
        }
        
        // 获取设备
        std::vector<cl::Device> devices;
        err = platform.getDevices(CL_DEVICE_TYPE_ALL, &devices);
        checkError(err, "getting devices");
        
        if (devices.empty()) {
            throw std::runtime_error("No OpenCL devices found");
        }
        
        if (device_id >= devices.size()) {
            throw std::runtime_error("Device index out of range");
        }
        
        device = devices[device_id];
        
        if (verbose) {
            std::string name;
            device.getInfo(CL_DEVICE_NAME, &name);
            std::cout << "Device: " << name << "\n";
            
            cl_uint compute_units;
            device.getInfo(CL_DEVICE_MAX_COMPUTE_UNITS, &compute_units);
            std::cout << "Compute units: " << compute_units << "\n";
            
            cl_ulong global_mem;
            device.getInfo(CL_DEVICE_GLOBAL_MEM_SIZE, &global_mem);
            std::cout << "Global memory: " << global_mem / (1024*1024) << " MB\n";
        }
        
        // 创建上下文
        context = cl::Context(device, nullptr, nullptr, nullptr, &err);
        checkError(err, "creating context");
        
        // 创建命令队列
        queue = cl::CommandQueue(context, device, CL_QUEUE_PROFILING_ENABLE, &err);
        checkError(err, "creating command queue");
    }
    
    void printInfo() {
        std::cout << "\n=== OpenCL Device Information ===\n";
        std::string platform_name, device_name;
        platform.getInfo(CL_PLATFORM_NAME, &platform_name);
        device.getInfo(CL_DEVICE_NAME, &device_name);
        std::cout << "Platform: " << platform_name << "\n";
        std::cout << "Device: " << device_name << "\n";
        
        cl_uint compute_units;
        device.getInfo(CL_DEVICE_MAX_COMPUTE_UNITS, &compute_units);
        std::cout << "Compute units: " << compute_units << "\n";
        
        cl_ulong global_mem, local_mem;
        device.getInfo(CL_DEVICE_GLOBAL_MEM_SIZE, &global_mem);
        device.getInfo(CL_DEVICE_LOCAL_MEM_SIZE, &local_mem);
        std::cout << "Global memory: " << global_mem / (1024*1024) << " MB\n";
        std::cout << "Local memory: " << local_mem / 1024 << " KB\n";
        
        cl_uint max_work_group;
        device.getInfo(CL_DEVICE_MAX_WORK_GROUP_SIZE, &max_work_group);
        std::cout << "Max work group size: " << max_work_group << "\n";
    }
};

//=============================================================================
// FPGA-Litho OpenCL内核管理
//=============================================================================

class LithoOpenCLKernel {
public:
    cl::Program program;
    cl::Kernel kernel;
    
    LithoOpenCLKernel(OpenCLDevice& dev, const std::string& xclbin_path, 
                      const std::string& kernel_name = "hls_litho_system",
                      bool verbose = false) {
        
        // 加载xclbin文件 (对于Xilinx平台)
        std::ifstream file(xclbin_path, std::ios::binary);
        if (!file.is_open()) {
            throw std::runtime_error("Cannot open xclbin file: " + xclbin_path);
        }
        
        // 读取文件内容
        file.seekg(0, std::ios::end);
        size_t size = file.tellg();
        file.seekg(0, std::ios::beg);
        
        std::vector<unsigned char> binary(size);
        file.read(reinterpret_cast<char*>(binary.data()), size);
        file.close();
        
        if (verbose) {
            std::cout << "Loaded xclbin: " << xclbin_path << " (" << size << " bytes)\n";
        }
        
        // 创建程序 (使用二进制)
        cl_int err;
        cl::Program::Binaries binaries;
        binaries.push_back({binary.data(), size});
        
        std::vector<cl::Device> devices = {dev.device};
        program = cl::Program(dev.context, devices, binaries, nullptr, &err);
        
        if (err != CL_SUCCESS) {
            // 尝试从源码编译 (如果二进制加载失败)
            std::cerr << "Binary load failed, trying source compilation...\n";
            // 对于非Xilinx平台，可能需要从源码编译
            program = cl::Program(dev.context, kernel_source, false, &err);
            checkError(err, "creating program from source");
            
            err = program.build("-cl-std=CL1.2");
            checkError(err, "building program");
        }
        
        // 创建内核
        kernel = cl::Kernel(program, kernel_name.c_str(), &err);
        checkError(err, "creating kernel");
        
        if (verbose) {
            std::cout << "Kernel: " << kernel_name << "\n";
            std::cout << "Num args: " << kernel.getInfo<CL_KERNEL_NUM_ARGS>() << "\n";
        }
    }
    
    cl::Buffer createBuffer(OpenCLDevice& dev, size_t size, cl_mem_flags flags = CL_MEM_READ_WRITE) {
        cl_int err;
        cl::Buffer buffer(dev.context, flags, size, nullptr, &err);
        checkError(err, "creating buffer");
        return buffer;
    }
    
    void writeBuffer(OpenCLDevice& dev, cl::Buffer& buffer, const void* data, size_t size) {
        cl_int err = dev.queue.enqueueWriteBuffer(buffer, CL_TRUE, 0, size, data);
        checkError(err, "writing buffer");
    }
    
    void readBuffer(OpenCLDevice& dev, cl::Buffer& buffer, void* data, size_t size) {
        cl_int err = dev.queue.enqueueReadBuffer(buffer, CL_TRUE, 0, size, data);
        checkError(err, "reading buffer");
    }
    
    cl::Event execute(OpenCLDevice& dev) {
        cl::Event event;
        cl_int err = dev.queue.enqueueTask(kernel, nullptr, &event);
        checkError(err, "executing kernel");
        event.wait();
        return event;
    }
    
private:
    // 如果需要源码编译，可以在这里定义内核源码
    static const char* kernel_source;
};

//=============================================================================
// 主运行函数
//=============================================================================

int run_litho_opencl(int argc, char* argv[]) {
    // 解析参数
    std::string xclbin_file = "";
    int device_index = 0;
    int mode = 1;
    int num_runs = 1;
    bool verbose = false;
    
    float lambda = 193.0f;
    float NA = 1.35f;
    float defocus = 0.0f;
    int Lx = 64, Ly = 64;
    int Nx = 3, Ny = 3;
    int srcSize = 32;
    int nkernels = 4;
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--xclbin" && i + 1 < argc) xclbin_file = argv[++i];
        else if (arg == "--device" && i + 1 < argc) device_index = std::stoi(argv[++i]);
        else if (arg == "--mode" && i + 1 < argc) mode = std::stoi(argv[++i]);
        else if (arg == "--runs" && i + 1 < argc) num_runs = std::stoi(argv[++i]);
        else if (arg == "--verbose") verbose = true;
        else if (arg == "--lambda" && i + 1 < argc) lambda = std::stof(argv[++i]);
        else if (arg == "--NA" && i + 1 < argc) NA = std::stof(argv[++i]);
        else if (arg == "--defocus" && i + 1 < argc) defocus = std::stof(argv[++i]);
        else if (arg == "--Lx" && i + 1 < argc) Lx = std::stoi(argv[++i]);
        else if (arg == "--Ly" && i + 1 < argc) Ly = std::stoi(argv[++i]);
        else if (arg == "--Nx" && i + 1 < argc) Nx = std::stoi(argv[++i]);
        else if (arg == "--Ny" && i + 1 < argc) Ny = std::stoi(argv[++i]);
        else if (arg == "--srcSize" && i + 1 < argc) srcSize = std::stoi(argv[++i]);
        else if (arg == "--nkernels" && i + 1 < argc) nkernels = std::stoi(argv[++i]);
    }
    
    if (xclbin_file.empty()) {
        std::cerr << "Error: --xclbin is required\n";
        return 1;
    }
    
    std::cout << "========================================\n";
    std::cout << "  FPGA-Litho OpenCL Host Application\n";
    std::cout << "========================================\n\n";
    
    try {
        // 初始化设备
        OpenCLDevice dev(0, device_index, verbose);
        dev.printInfo();
        
        // 创建内核
        LithoOpenCLKernel litho(dev, xclbin_file, "hls_litho_system", verbose);
        
        // 计算数据大小
        int tcc_dim = (2 * Nx + 1) * (2 * Ny + 1);
        int tcc_total = tcc_dim * tcc_dim;
        int output_size = (4 * Nx + 1) * (4 * Ny + 1);
        
        // 生成测试数据
        std::cout << "\n=== Generating Test Data ===\n";
        
        std::vector<ComplexFloat> source_data(srcSize * srcSize);
        std::vector<ComplexFloat> mask_data(Lx * Ly);
        std::vector<ComplexFloat> tcc_data(tcc_total);
        std::vector<ComplexFloat> kernels_data(nkernels * tcc_dim);
        std::vector<float> scales_data(nkernels);
        
        for (int i = 0; i < srcSize * srcSize; i++) {
            source_data[i] = ComplexFloat(std::sin(i * 0.1f), std::cos(i * 0.1f));
        }
        for (int i = 0; i < Lx * Ly; i++) {
            mask_data[i] = ComplexFloat(1.0f, 0.0f);
        }
        for (int i = 0; i < tcc_total; i++) {
            tcc_data[i] = ComplexFloat(0.1f * (i % 10), 0.0f);
        }
        for (int i = 0; i < nkernels * tcc_dim; i++) {
            kernels_data[i] = ComplexFloat(0.5f, 0.0f);
        }
        for (int k = 0; k < nkernels; k++) {
            scales_data[k] = 1.0f / nkernels;
        }
        
        std::cout << "Data sizes:\n";
        std::cout << "  Source: " << source_data.size() << " complex\n";
        std::cout << "  Mask: " << mask_data.size() << " complex\n";
        std::cout << "  TCC: " << tcc_data.size() << " complex\n";
        std::cout << "  Kernels: " << kernels_data.size() << " complex\n";
        std::cout << "  Scales: " << scales_data.size() << " float\n";
        
        // 创建缓冲
        std::cout << "\n=== Creating OpenCL Buffers ===\n";
        
        cl::Buffer source_buf = litho.createBuffer(dev, source_data.size() * sizeof(ComplexFloat), CL_MEM_READ_ONLY);
        cl::Buffer mask_buf = litho.createBuffer(dev, mask_data.size() * sizeof(ComplexFloat), CL_MEM_READ_ONLY);
        cl::Buffer tcc_buf = litho.createBuffer(dev, tcc_data.size() * sizeof(ComplexFloat), CL_MEM_READ_ONLY);
        cl::Buffer kernels_buf = litho.createBuffer(dev, kernels_data.size() * sizeof(ComplexFloat), CL_MEM_READ_ONLY);
        cl::Buffer scales_buf = litho.createBuffer(dev, scales_data.size() * sizeof(float), CL_MEM_READ_ONLY);
        cl::Buffer imgf_buf = litho.createBuffer(dev, Lx * Ly * sizeof(ComplexFloat), CL_MEM_WRITE_ONLY);
        cl::Buffer img_out_buf = litho.createBuffer(dev, output_size * sizeof(float), CL_MEM_WRITE_ONLY);
        
        // 写入数据
        litho.writeBuffer(dev, source_buf, source_data.data(), source_data.size() * sizeof(ComplexFloat));
        litho.writeBuffer(dev, mask_buf, mask_data.data(), mask_data.size() * sizeof(ComplexFloat));
        litho.writeBuffer(dev, tcc_buf, tcc_data.data(), tcc_data.size() * sizeof(ComplexFloat));
        litho.writeBuffer(dev, kernels_buf, kernels_data.data(), kernels_data.size() * sizeof(ComplexFloat));
        litho.writeBuffer(dev, scales_buf, scales_data.data(), scales_data.size() * sizeof(float));
        
        // 设置内核参数
        litho.kernel.setArg(0, source_buf);
        litho.kernel.setArg(1, mask_buf);
        litho.kernel.setArg(2, tcc_buf);
        litho.kernel.setArg(3, kernels_buf);
        litho.kernel.setArg(4, scales_buf);
        litho.kernel.setArg(5, imgf_buf);
        litho.kernel.setArg(6, img_out_buf);
        litho.kernel.setArg(7, lambda);
        litho.kernel.setArg(8, NA);
        litho.kernel.setArg(9, defocus);
        litho.kernel.setArg(10, Lx);
        litho.kernel.setArg(11, Ly);
        litho.kernel.setArg(12, Nx);
        litho.kernel.setArg(13, Ny);
        litho.kernel.setArg(14, srcSize);
        litho.kernel.setArg(15, nkernels);
        litho.kernel.setArg(16, mode);
        
        // 执行内核
        std::cout << "\n=== Executing Kernel ===\n";
        
        std::vector<long> execution_times;
        
        for (int run = 0; run < num_runs; run++) {
            if (verbose) std::cout << "Run " << run + 1 << "/" << num_runs << "\n";
            
            auto start = std::chrono::high_resolution_clock::now();
            
            cl::Event event = litho.execute(dev);
            
            auto end = std::chrono::high_resolution_clock::now();
            long duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
            execution_times.push_back(duration);
            
            if (verbose) {
                cl_ulong time_start, time_end;
                event.getProfilingInfo(CL_PROFILING_COMMAND_START, &time_start);
                event.getProfilingInfo(CL_PROFILING_COMMAND_END, &time_end);
                cl_ulong kernel_time = time_end - time_start;
                std::cout << "  Host time: " << duration << " us\n";
                std::cout << "  Kernel time: " << kernel_time / 1000 << " us\n";
            }
        }
        
        // 读取结果
        std::cout << "\n=== Reading Results ===\n";
        
        if (mode == 1) {
            std::vector<ComplexFloat> imgf_result(Lx * Ly);
            litho.readBuffer(dev, imgf_buf, imgf_result.data(), Lx * Ly * sizeof(ComplexFloat));
            
            int non_zero = 0;
            float max_mag = 0.0f;
            for (const auto& c : imgf_result) {
                float mag = std::abs(c);
                if (mag > 1e-6) {
                    non_zero++;
                    max_mag = std::max(max_mag, mag);
                }
            }
            std::cout << "TCC Mode Result:\n";
            std::cout << "  Non-zero elements: " << non_zero << "\n";
            std::cout << "  Max magnitude: " << max_mag << "\n";
        } else {
            std::vector<float> img_out_result(output_size);
            litho.readBuffer(dev, img_out_buf, img_out_result.data(), output_size * sizeof(float));
            
            int non_zero = 0;
            float max_val = 0.0f;
            for (float v : img_out_result) {
                if (v > 1e-6) {
                    non_zero++;
                    max_val = std::max(max_val, v);
                }
            }
            std::cout << "SOCS Mode Result:\n";
            std::cout << "  Non-zero pixels: " << non_zero << "\n";
            std::cout << "  Max value: " << max_val << "\n";
        }
        
        // 性能统计
        if (num_runs > 1) {
            long min_time = *std::min_element(execution_times.begin(), execution_times.end());
            long max_time = *std::max_element(execution_times.begin(), execution_times.end());
            long avg_time = 0;
            for (long t : execution_times) avg_time += t;
            avg_time /= execution_times.size();
            
            std::cout << "\n=== Performance Statistics ===\n";
            std::cout << "Runs: " << num_runs << "\n";
            std::cout << "Min time: " << min_time << " us\n";
            std::cout << "Max time: " << max_time << " us\n";
            std::cout << "Avg time: " << avg_time << " us\n";
        }
        
        std::cout << "\nFPGA-Litho OpenCL completed successfully!\n";
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
    return run_litho_opencl(argc, argv);
}