#!/usr/bin/env python3
"""
K-Litho XRT Python Host Application

基于PyXRT的Python主机程序，用于控制K-Litho光刻模拟FPGA内核

支持两种工作模式:
- TCC模式: 计算频域图像 (mode=1)
- SOCS模式: 计算空间域图像 (mode=2)

@author K-Litho Team
@date 2026-04-03
"""

import argparse
import numpy as np
import time
import sys
import os

# PyXRT import (需要安装pyxrt)
try:
    import pyxrt as xrt
except ImportError:
    print("Error: pyxrt not found. Please install XRT Python bindings.")
    print("pip install pyxrt")
    sys.exit(1)

#=============================================================================
# 常量定义 (与HLS内核匹配)
#=============================================================================

SYS_MAX_LX = 64           # 最大频域X尺寸
SYS_MAX_LY = 64           # 最大频域Y尺寸
SYS_MAX_NX = 7            # 最大TCC/SOCS半宽
SYS_MAX_NY = 7            # 最大TCC/SOCS半高
SYS_MAX_KERNELS = 8       # 最大SOCS核数量
SYS_MAX_SRC_SIZE = 64     # 最大光源尺寸

SYS_TCC_DIM = (2*SYS_MAX_NX+1) * (2*SYS_MAX_NY+1)  # TCC矩阵维度
SYS_TCC_TOTAL = SYS_TCC_DIM * SYS_TCC_DIM          # TCC矩阵总元素
SYS_OUTPUT_SIZE = (4*SYS_MAX_NX+1) * (4*SYS_MAX_NY+1)  # SOCS输出尺寸

#=============================================================================
# 数据生成/加载函数
#=============================================================================

def generate_test_complex_data(size):
    """生成测试复数数据"""
    real = np.sin(np.linspace(0, 2*np.pi, size)).astype(np.float32)
    imag = np.cos(np.linspace(0, 2*np.pi, size)).astype(np.float32)
    # Interleave real and imag for HLS format
    return np.stack([real, imag], axis=-1).flatten()

def generate_test_float_data(size):
    """生成测试浮点数据"""
    return np.linspace(0, 1, size).astype(np.float32)

def load_complex_data(filename, expected_size):
    """从文件加载复数数据"""
    if not filename or not os.path.exists(filename):
        print(f"Warning: File not found {filename}, generating test data")
        return generate_test_complex_data(expected_size)
    
    data = np.fromfile(filename, dtype=np.float32)
    if len(data) < expected_size * 2:
        print(f"Warning: File truncated, generating test data")
        return generate_test_complex_data(expected_size)
    
    # Format: [real0, imag0, real1, imag1, ...]
    return data[:expected_size * 2]

def load_float_data(filename, expected_size):
    """从文件加载浮点数据"""
    if not filename or not os.path.exists(filename):
        print(f"Warning: File not found {filename}, generating test data")
        return generate_test_float_data(expected_size)
    
    data = np.fromfile(filename, dtype=np.float32)
    if len(data) < expected_size:
        print(f"Warning: File truncated, generating test data")
        return generate_test_float_data(expected_size)
    
    return data[:expected_size]

def save_complex_data(filename, data):
    """保存复数数据到文件"""
    if filename:
        data.astype(np.float32).tofile(filename)
        print(f"Saved {len(data)//2} complex values to {filename}")

def save_float_data(filename, data):
    """保存浮点数据到文件"""
    if filename:
        data.astype(np.float32).tofile(filename)
        print(f"Saved {len(data)} float values to {filename}")

#=============================================================================
# XRT设备管理
#=============================================================================

class LithoDevice:
    """XRT设备管理类"""
    
    def __init__(self, device_index=0, xclbin_path=None, verbose=False):
        self.verbose = verbose
        
        # 获取设备
        devices = xrt.get_devices()
        if len(devices) == 0:
            raise RuntimeError("No XRT devices found")
        
        if device_index >= len(devices):
            raise RuntimeError(f"Device index {device_index} out of range")
        
        self.device = devices[device_index]
        
        if self.verbose:
            print(f"Device: {self.device.get_name()}")
            print(f"BDF: {self.device.get_bdf()}")
        
        # 加载xclbin
        if xclbin_path:
            self.load_xclbin(xclbin_path)
    
    def load_xclbin(self, xclbin_path):
        """加载xclbin文件"""
        if not os.path.exists(xclbin_path):
            raise FileNotFoundError(f"xclbin not found: {xclbin_path}")
        
        self.xclbin = xrt.xclbin(xclbin_path)
        self.device.load_xclbin(self.xclbin)
        self.uuid = self.xclbin.get_uuid()
        
        if self.verbose:
            print(f"Loaded xclbin: {xclbin_path}")
            print(f"UUID: {self.uuid}")
    
    def print_info(self):
        """打印设备信息"""
        print("\n=== Device Information ===")
        print(f"Name: {self.device.get_name()}")
        print(f"BDF: {self.device.get_bdf()}")
        print(f"UUID: {self.uuid}")

#=============================================================================
# K-Litho内核管理
#=============================================================================

class LithoKernel:
    """K-Litho内核管理类"""
    
    def __init__(self, device, kernel_name="hls_litho_system", verbose=False):
        self.device = device
        self.verbose = verbose
        
        # 创建内核
        self.kernel = xrt.kernel(device.device, device.uuid, kernel_name)
        
        if self.verbose:
            print(f"Kernel: {kernel_name}")
            print(f"Number of arguments: {self.kernel.get_num_args()}")
    
    def create_buffer(self, size, bank=0):
        """创建缓冲对象"""
        return xrt.bo(self.device.device, size, bank)
    
    def create_input_buffer_complex(self, data, bank=0):
        """创建复数数据输入缓冲"""
        bo = self.create_buffer(len(data) * 4, bank)  # float32
        bo.write(data, 0)
        bo.sync(xrt.xcl_bosync_direction.XCL_BO_SYNC_BO_TO_DEVICE)
        return bo
    
    def create_input_buffer_float(self, data, bank=0):
        """创建浮点数据输入缓冲"""
        bo = self.create_buffer(len(data) * 4, bank)
        bo.write(data, 0)
        bo.sync(xrt.xcl_bosync_direction.XCL_BO_SYNC_BO_TO_DEVICE)
        return bo
    
    def create_output_buffer(self, size, bank=0):
        """创建输出缓冲"""
        return self.create_buffer(size * 4, bank)
    
    def read_output_complex(self, bo, size):
        """读取复数输出"""
        bo.sync(xrt.xcl_bosync_direction.XCL_BO_SYNC_BO_FROM_DEVICE)
        data = np.frombuffer(bo.map(), dtype=np.float32, count=size*2)
        return data
    
    def read_output_float(self, bo, size):
        """读取浮点输出"""
        bo.sync(xrt.xcl_bosync_direction.XCL_BO_SYNC_BO_FROM_DEVICE)
        data = np.frombuffer(bo.map(), dtype=np.float32, count=size)
        return data
    
    def run(self, args_list):
        """运行内核"""
        run_obj = xrt.run(self.kernel)
        
        # 设置参数
        for i, arg in enumerate(args_list):
            if isinstance(arg, xrt.bo):
                run_obj.set_arg(i, arg)
            else:
                run_obj.set_arg(i, arg)
        
        # 启动
        run_obj.start()
        
        # 等待完成
        state = run_obj.wait()
        
        return state

#=============================================================================
# 主运行函数
#=============================================================================

def run_litho(args):
    """运行K-Litho内核"""
    
    # 初始化设备
    device = LithoDevice(args.device, args.xclbin, args.verbose)
    device.print_info()
    
    # 创建内核
    kernel = LithoKernel(device, "hls_litho_system", args.verbose)
    
    # 计算数据大小
    tcc_dim = (2 * args.Nx + 1) * (2 * args.Ny + 1)
    tcc_total = tcc_dim * tcc_dim
    output_size = (4 * args.Nx + 1) * (4 * args.Ny + 1)
    
    # 加载/生成输入数据
    source_data = load_complex_data(args.source, args.srcSize * args.srcSize)
    mask_data = load_complex_data(args.mask, args.Lx * args.Ly)
    tcc_data = load_complex_data(args.tcc, tcc_total)
    kernels_data = load_complex_data(args.kernels, args.nkernels * tcc_dim)
    scales_data = load_float_data(args.scales, args.nkernels)
    
    # 创建缓冲对象
    if args.verbose:
        print("\n=== Creating Buffer Objects ===")
    
    source_bo = kernel.create_input_buffer_complex(source_data)
    mask_bo = kernel.create_input_buffer_complex(mask_data)
    tcc_bo = kernel.create_input_buffer_complex(tcc_data)
    kernels_bo = kernel.create_input_buffer_complex(kernels_data)
    scales_bo = kernel.create_input_buffer_float(scales_data)
    
    # 输出缓冲
    imgf_bo = kernel.create_output_buffer(args.Lx * args.Ly * 2)
    img_out_bo = kernel.create_output_buffer(output_size)
    
    # 运行内核
    execution_times = []
    
    for run_idx in range(args.num_runs):
        if args.verbose:
            print(f"\n=== Run {run_idx + 1}/{args.num_runs} ===")
        
        start_time = time.time()
        
        # 准备内核参数
        # 参数顺序: source, mask_fft, tcc, kernels, scales, imgf, img_out,
        #           lambda, NA, defocus, Lx, Ly, Nx, Ny, srcSize, nkernels, mode
        
        kernel_args = [
            source_bo,
            mask_bo,
            tcc_bo,
            kernels_bo,
            scales_bo,
            imgf_bo,
            img_out_bo,
            args.lambda,
            args.NA,
            args.defocus,
            args.Lx,
            args.Ly,
            args.Nx,
            args.Ny,
            args.srcSize,
            args.nkernels,
            args.mode
        ]
        
        # 运行
        state = kernel.run(kernel_args)
        
        end_time = time.time()
        duration = (end_time - start_time) * 1000000  # us
        execution_times.append(duration)
        
        if args.verbose:
            print(f"Execution time: {duration:.2f} us")
            print(f"Kernel state: {state}")
        
        # 读取结果
        if args.mode == 1:
            # TCC模式: 读取频域输出
            imgf_result = kernel.read_output_complex(imgf_bo, args.Lx * args.Ly)
            
            # 保存结果
            save_complex_data(args.output, imgf_result)
            
            # 统计
            if args.verbose or run_idx == args.num_runs - 1:
                # 计算非零元素和最大值
                real = imgf_result[::2]
                imag = imgf_result[1::2]
                mag = np.sqrt(real**2 + imag**2)
                non_zero = np.sum(mag > 1e-6)
                max_mag = np.max(mag)
                print(f"TCC Mode Result: {non_zero} non-zero elements, Max magnitude: {max_mag:.4f}")
        
        elif args.mode == 2:
            # SOCS模式: 读取空间域输出
            img_out_result = kernel.read_output_float(img_out_bo, output_size)
            
            # 保存结果
            save_float_data(args.output, img_out_result)
            
            # 统计
            if args.verbose or run_idx == args.num_runs - 1:
                non_zero = np.sum(img_out_result > 1e-6)
                max_val = np.max(img_out_result)
                print(f"SOCS Mode Result: {non_zero} non-zero pixels, Max value: {max_val:.4f}")
    
    # 性能统计
    if args.num_runs > 1:
        min_time = min(execution_times)
        max_time = max(execution_times)
        avg_time = sum(execution_times) / len(execution_times)
        
        print("\n=== Performance Statistics ===")
        print(f"Runs: {args.num_runs}")
        print(f"Min time: {min_time:.2f} us")
        print(f"Max time: {max_time:.2f} us")
        print(f"Avg time: {avg_time:.2f} us")
    
    print("\nK-Litho completed successfully!")

#=============================================================================
# 主函数
#=============================================================================

def main():
    parser = argparse.ArgumentParser(description='K-Litho XRT Host Application')
    
    # 必需参数
    parser.add_argument('--xclbin', required=True, help='XCLBIN file path')
    
    # 设备参数
    parser.add_argument('--device', type=int, default=0, help='Device index')
    parser.add_argument('--mode', type=int, default=1, help='Mode: 1=TCC, 2=SOCS')
    parser.add_argument('--runs', type=int, default=1, help='Number of runs')
    
    # 光学参数
    parser.add_argument('--lambda', type=float, default=193.0, help='Wavelength (nm)')
    parser.add_argument('--NA', type=float, default=1.35, help='Numerical aperture')
    parser.add_argument('--defocus', type=float, default=0.0, help='Defocus amount (nm)')
    
    # 尺寸参数
    parser.add_argument('--Lx', type=int, default=64, help='Frequency domain X size')
    parser.add_argument('--Ly', type=int, default=64, help='Frequency domain Y size')
    parser.add_argument('--Nx', type=int, default=3, help='TCC/SOCS half-width')
    parser.add_argument('--Ny', type=int, default=3, help='TCC/SOCS half-height')
    parser.add_argument('--srcSize', type=int, default=32, help='Source size for TCC mode')
    parser.add_argument('--nkernels', type=int, default=4, help='Number of SOCS kernels')
    
    # 数据文件
    parser.add_argument('--source', default='', help='Source data file')
    parser.add_argument('--mask', default='', help='Mask FFT data file')
    parser.add_argument('--tcc', default='', help='TCC matrix file')
    parser.add_argument('--kernels', default='', help='SOCS kernels file')
    parser.add_argument('--scales', default='', help='SOCS scales file')
    parser.add_argument('--output', default='', help='Output result file')
    
    # 输出选项
    parser.add_argument('--verbose', action='store_true', help='Verbose output')
    
    args = parser.parse_args()
    
    print("=" * 40)
    print("  K-Litho XRT Python Host Application")
    print("  (TCC & SOCS Lithography Simulation)")
    print("=" * 40)
    print()
    
    print("Configuration:")
    print(f"  Mode: {'TCC' if args.mode == 1 else 'SOCS'}")
    print(f"  Lambda: {args.lambda} nm")
    print(f"  NA: {args.NA}")
    print(f"  Defocus: {args.defocus} nm")
    print(f"  Lx/Ly: {args.Lx}/{args.Ly}")
    print(f"  Nx/Ny: {args.Nx}/{args.Ny}")
    if args.mode == 1:
        print(f"  Source size: {args.srcSize}")
    else:
        print(f"  Num kernels: {args.nkernels}")
    print()
    
    try:
        run_litho(args)
    except Exception as e:
        print(f"Error: {e}")
        return 1
    
    return 0

if __name__ == "__main__":
    sys.exit(main())