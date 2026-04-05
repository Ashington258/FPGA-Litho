#!/usr/bin/env python3
"""
FPGA-Litho BRAM Python Host Application (Phase 6D)

BRAM版本的Python驱动程序 - 单函数架构

支持10种操作:
- OP_LOAD_SOURCE (0): 加载光源数据
- OP_LOAD_MASK (1): 加载mask数据
- OP_LOAD_TCC (2): 加载TCC矩阵
- OP_LOAD_KERNELS (3): 加载SOCS kernels
- OP_LOAD_SCALES (4): 加载SOCS scales
- OP_COMPUTE_TCC (5): TCC模式计算
- OP_COMPUTE_SOCS (6): SOCS模式计算
- OP_READ_IMGF (7): 读取imgf结果
- OP_READ_IMG_OUT (8): 读取img_out结果
- OP_RESET (9): 重置所有BRAM存储

@author FPGA-Litho Team
@date 2026-04-04
"""

import argparse
import numpy as np
import time
import sys
import os

# PyXRT import
try:
    import pyxrt as xrt
except ImportError:
    print("Error: pyxrt not found. Please install XRT Python bindings.")
    print("pip install pyxrt")
    sys.exit(1)

#=============================================================================
# BRAM Configuration Constants (匹配HLS头文件)
#=============================================================================

# BRAM存储尺寸限制
BRAM_MAX_LX = 64
BRAM_MAX_LY = 64
BRAM_MAX_NX_TCC = 3          # TCC模式最大Nx (BRAM容量限制)
BRAM_MAX_NX_SOCS = 15
BRAM_MAX_NY = 15
BRAM_MAX_KERNELS = 8

# BRAM存储数组尺寸
BRAM_SOURCE_SIZE = BRAM_MAX_LX * BRAM_MAX_LY         # 4096
BRAM_MASK_SIZE = BRAM_MAX_LX * BRAM_MAX_LY           # 4096
BRAM_TCC_SIZE = (2*BRAM_MAX_NX_TCC+1) ** 2           # 49
BRAM_KERNELS_SIZE = BRAM_MAX_KERNELS * 225           # 1800
BRAM_SCALES_SIZE = BRAM_MAX_KERNELS                  # 8
BRAM_IMGF_SIZE = BRAM_MAX_LX * BRAM_MAX_LY           # 4096
BRAM_IMG_OUT_SIZE = BRAM_MAX_LX * BRAM_MAX_LY        # 4096

# Operation codes
OP_LOAD_SOURCE  = 0
OP_LOAD_MASK    = 1
OP_LOAD_TCC     = 2
OP_LOAD_KERNELS = 3
OP_LOAD_SCALES  = 4
OP_COMPUTE_TCC  = 5
OP_COMPUTE_SOCS = 6
OP_READ_IMGF    = 7
OP_READ_IMG_OUT = 8
OP_RESET        = 9

#=============================================================================
# BRAM Device Manager
#=============================================================================

class BRAMDevice:
    """BRAM设备管理类"""
    
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

#=============================================================================
# BRAM Kernel Interface
#=============================================================================

class BRAMKernel:
    """BRAM内核接口类 - 单函数架构"""
    
    def __init__(self, device, kernel_name="hls_litho_system_bram", verbose=False):
        self.device = device
        self.verbose = verbose
        
        # 创建内核
        self.kernel = xrt.kernel(device.device, device.uuid, kernel_name)
        
        if self.verbose:
            print(f"Kernel: {kernel_name}")
    
    #=========================================================================
    # Low-Level Operation Interface
    #=========================================================================
    
    def execute_operation(self, operation, idx=0, val_real=0.0, val_imag=0.0,
                         mode=0, Lx=0, Ly=0, Nx=0, Ny=0, srcSize=0, nkernels=0):
        """
        执行单个操作
        
        Args:
            operation: 操作码 (0-9)
            idx: 数组索引
            val_real: 值的实部
            val_imag: 值的虚部
            mode: 计算模式 (1=TCC, 2=SOCS)
            Lx, Ly: 频域尺寸
            Nx, Ny: TCC/SOCS参数
            srcSize: 光源尺寸
            nkernels: SOCS核数量
        
        Returns:
            (result_real, result_imag): 返回值的实部和虚部
        """
        # 创建运行对象
        run_obj = xrt.run(self.kernel)
        
        # 设置参数 (10个参数 + return)
        run_obj.set_arg(0, operation)
        run_obj.set_arg(1, idx)
        run_obj.set_arg(2, val_real)
        run_obj.set_arg(3, val_imag)
        run_obj.set_arg(4, mode)
        run_obj.set_arg(5, Lx)
        run_obj.set_arg(6, Ly)
        run_obj.set_arg(7, Nx)
        run_obj.set_arg(8, Ny)
        run_obj.set_arg(9, srcSize)
        run_obj.set_arg(10, nkernels)
        
        # 启动内核
        run_obj.start()
        
        # 等待完成
        run_obj.wait()
        
        # 读取返回值 (complex float = 2 floats)
        # Note: PyXRT返回值读取方式可能需要调整
        result_real = 0.0
        result_imag = 0.0
        
        return (result_real, result_imag)
    
    #=========================================================================
    # High-Level Data Loading Interface
    #=========================================================================
    
    def load_source_batch(self, source_data, Lx, Ly):
        """
        批量加载光源数据
        
        Args:
            source_data: 复数数组, shape=(Lx*Ly,) 或 (Lx, Ly)
            Lx, Ly: 尺寸
        """
        if isinstance(source_data, np.ndarray):
            if source_data.ndim == 2:
                source_data = source_data.flatten()
        
        size = min(len(source_data), BRAM_SOURCE_SIZE)
        
        if self.verbose:
            print(f"Loading {size} source elements...")
        
        start_time = time.time()
        
        for i in range(size):
            val = source_data[i]
            if isinstance(val, complex):
                self.execute_operation(OP_LOAD_SOURCE, i, val.real, val.imag)
            else:
                self.execute_operation(OP_LOAD_SOURCE, i, val, 0.0)
        
        elapsed = time.time() - start_time
        if self.verbose:
            print(f"  Loaded in {elapsed:.2f}s ({size/elapsed:.1f} elem/s)")
    
    def load_mask_batch(self, mask_data, Lx, Ly):
        """
        批量加载mask数据
        
        Args:
            mask_data: 复数数组
            Lx, Ly: 尺寸
        """
        if isinstance(mask_data, np.ndarray):
            if mask_data.ndim == 2:
                mask_data = mask_data.flatten()
        
        size = min(len(mask_data), BRAM_MASK_SIZE)
        
        if self.verbose:
            print(f"Loading {size} mask elements...")
        
        start_time = time.time()
        
        for i in range(size):
            val = mask_data[i]
            if isinstance(val, complex):
                self.execute_operation(OP_LOAD_MASK, i, val.real, val.imag)
            else:
                self.execute_operation(OP_LOAD_MASK, i, val, 0.0)
        
        elapsed = time.time() - start_time
        if self.verbose:
            print(f"  Loaded in {elapsed:.2f}s ({size/elapsed:.1f} elem/s)")
    
    def load_tcc_batch(self, tcc_data):
        """
        批量加载TCC矩阵
        
        Args:
            tcc_data: 复数数组, size <= 49
        """
        if isinstance(tcc_data, np.ndarray):
            tcc_data = tcc_data.flatten()
        
        size = min(len(tcc_data), BRAM_TCC_SIZE)
        
        if self.verbose:
            print(f"Loading {size} TCC elements...")
        
        for i in range(size):
            val = tcc_data[i]
            if isinstance(val, complex):
                self.execute_operation(OP_LOAD_TCC, i, val.real, val.imag)
            else:
                self.execute_operation(OP_LOAD_TCC, i, val, 0.0)
    
    def load_kernels_batch(self, kernels_data, nkernels):
        """
        批量加载SOCS kernels
        
        Args:
            kernels_data: 复数数组, shape=(nkernels, 225)
            nkernels: 核数量 (<=8)
        """
        if nkernels > BRAM_MAX_KERNELS:
            raise ValueError(f"nkernels {nkernels} exceeds max {BRAM_MAX_KERNELS}")
        
        if isinstance(kernels_data, np.ndarray):
            if kernels_data.ndim == 2:
                # Flatten to 1D
                kernels_data = kernels_data.flatten()
        
        size = nkernels * 225
        
        if self.verbose:
            print(f"Loading {size} kernel elements ({nkernels} kernels)...")
        
        start_time = time.time()
        
        for i in range(size):
            val = kernels_data[i]
            if isinstance(val, complex):
                self.execute_operation(OP_LOAD_KERNELS, i, val.real, val.imag)
            else:
                self.execute_operation(OP_LOAD_KERNELS, i, val, 0.0)
        
        elapsed = time.time() - start_time
        if self.verbose:
            print(f"  Loaded in {elapsed:.2f}s ({size/elapsed:.1f} elem/s)")
    
    def load_scales_batch(self, scales_data):
        """
        批量加载SOCS scales
        
        Args:
            scales_data: 浮点数组, size <= 8
        """
        if isinstance(scales_data, np.ndarray):
            scales_data = scales_data.flatten()
        
        size = min(len(scales_data), BRAM_SCALES_SIZE)
        
        if self.verbose:
            print(f"Loading {size} scale values...")
        
        for i in range(size):
            self.execute_operation(OP_LOAD_SCALES, i, float(scales_data[i]), 0.0)
    
    #=========================================================================
    # Compute Interface
    #=========================================================================
    
    def compute_tcc(self, Lx, Ly, Nx, Ny, srcSize):
        """
        执行TCC模式计算
        
        Args:
            Lx, Ly: 频域尺寸
            Nx, Ny: TCC参数 (Nx <= 3)
            srcSize: 光源尺寸
        
        Returns:
            success: bool
        """
        if Nx > BRAM_MAX_NX_TCC:
            raise ValueError(f"TCC Nx {Nx} exceeds max {BRAM_MAX_NX_TCC}")
        
        if self.verbose:
            print(f"Computing TCC mode (Lx={Lx}, Ly={Ly}, Nx={Nx}, Ny={Ny})...")
        
        start_time = time.time()
        
        result_real, result_imag = self.execute_operation(
            OP_COMPUTE_TCC, 0, 0.0, 0.0, 1, Lx, Ly, Nx, Ny, srcSize, 0
        )
        
        elapsed = time.time() - start_time
        
        success = (result_real == 1.0)
        
        if self.verbose:
            status = "SUCCESS" if success else "ERROR"
            print(f"  Compute {status} in {elapsed*1000:.2f}ms")
        
        return success
    
    def compute_socs(self, Lx, Ly, Nx, Ny, nkernels):
        """
        执行SOCS模式计算
        
        Args:
            Lx, Ly: 频域尺寸
            Nx, Ny: SOCS参数
            nkernels: 核数量 (<=8)
        
        Returns:
            success: bool
        """
        if nkernels > BRAM_MAX_KERNELS:
            raise ValueError(f"nkernels {nkernels} exceeds max {BRAM_MAX_KERNELS}")
        
        if self.verbose:
            print(f"Computing SOCS mode (Lx={Lx}, Ly={Ly}, nkernels={nkernels})...")
        
        start_time = time.time()
        
        result_real, result_imag = self.execute_operation(
            OP_COMPUTE_SOCS, 0, 0.0, 0.0, 2, Lx, Ly, Nx, Ny, 0, nkernels
        )
        
        elapsed = time.time() - start_time
        
        success = (result_real == 1.0)
        
        if self.verbose:
            status = "SUCCESS" if success else "ERROR"
            print(f"  Compute {status} in {elapsed*1000:.2f}ms")
        
        return success
    
    #=========================================================================
    # Data Read Interface
    #=========================================================================
    
    def read_imgf_batch(self, Lx, Ly):
        """
        批量读取imgf结果
        
        Args:
            Lx, Ly: 频域尺寸
        
        Returns:
            imgf_data: 复数数组
        """
        size = Lx * Ly
        
        if self.verbose:
            print(f"Reading {size} imgf elements...")
        
        imgf_data = np.zeros(size, dtype=complex)
        
        start_time = time.time()
        
        for i in range(size):
            result_real, result_imag = self.execute_operation(OP_READ_IMGF, i)
            imgf_data[i] = complex(result_real, result_imag)
        
        elapsed = time.time() - start_time
        if self.verbose:
            print(f"  Read in {elapsed:.2f}s ({size/elapsed:.1f} elem/s)")
        
        return imgf_data
    
    def read_img_out_batch(self, Lx, Ly):
        """
        批量读取img_out结果
        
        Args:
            Lx, Ly: 频域尺寸
        
        Returns:
            img_out_data: 浮点数组
        """
        size = Lx * Ly
        
        if self.verbose:
            print(f"Reading {size} img_out elements...")
        
        img_out_data = np.zeros(size, dtype=np.float32)
        
        start_time = time.time()
        
        for i in range(size):
            result_real, result_imag = self.execute_operation(OP_READ_IMG_OUT, i)
            img_out_data[i] = result_real
        
        elapsed = time.time() - start_time
        if self.verbose:
            print(f"  Read in {elapsed:.2f}s ({size/elapsed:.1f} elem/s)")
        
        return img_out_data
    
    #=========================================================================
    # System Operations
    #=========================================================================
    
    def reset(self):
        """
        重置所有BRAM存储
        
        Returns:
            success: bool
        """
        if self.verbose:
            print("Resetting BRAM storage...")
        
        result_real, result_imag = self.execute_operation(OP_RESET)
        
        success = (result_real == 1.0)
        
        if self.verbose:
            print(f"  Reset {'SUCCESS' if success else 'FAILED'}")
        
        return success

#=============================================================================
# High-Level Application Interface
#=============================================================================

class LithoBRAMApp:
    """FPGA-Litho BRAM应用高级接口"""
    
    def __init__(self, device_index=0, xclbin_path=None, verbose=False):
        self.verbose = verbose
        
        # 初始化设备
        self.device = BRAMDevice(device_index, xclbin_path, verbose)
        
        # 初始化内核
        self.kernel = BRAMKernel(self.device, verbose=verbose)
    
    def run_tcc_mode(self, mask_data, tcc_data, Lx, Ly, Nx, Ny, srcSize=0):
        """
        运行TCC模式计算
        
        Args:
            mask_data: 输入mask频谱 (复数数组, Lx*Ly)
            tcc_data: TCC矩阵 (复数数组, <=49)
            Lx, Ly: 频域尺寸
            Nx, Ny: TCC参数
            srcSize: 光源尺寸
        
        Returns:
            imgf: 输出频域图像 (复数数组)
        """
        if self.verbose:
            print("\n=== Running TCC Mode ===")
        
        # 重置
        self.kernel.reset()
        
        # 加载数据
        self.kernel.load_mask_batch(mask_data, Lx, Ly)
        self.kernel.load_tcc_batch(tcc_data)
        
        # 计算
        success = self.kernel.compute_tcc(Lx, Ly, Nx, Ny, srcSize)
        
        if not success:
            raise RuntimeError("TCC compute failed")
        
        # 读取结果
        imgf = self.kernel.read_imgf_batch(Lx, Ly)
        
        if self.verbose:
            print(f"TCC mode completed. Output size: {len(imgf)}")
        
        return imgf
    
    def run_socs_mode(self, mask_data, kernels_data, scales_data, 
                      Lx, Ly, Nx, Ny, nkernels):
        """
        运行SOCS模式计算
        
        Args:
            mask_data: 输入mask频谱 (复数数组, Lx*Ly)
            kernels_data: SOCS kernels (复数数组, nkernels*225)
            scales_data: SOCS scales (浮点数组, nkernels)
            Lx, Ly: 频域尺寸
            Nx, Ny: SOCS参数
            nkernels: 核数量
        
        Returns:
            img_out: 输出空间域图像 (浮点数组)
        """
        if self.verbose:
            print("\n=== Running SOCS Mode ===")
        
        # 重置
        self.kernel.reset()
        
        # 加载数据
        self.kernel.load_mask_batch(mask_data, Lx, Ly)
        self.kernel.load_kernels_batch(kernels_data, nkernels)
        self.kernel.load_scales_batch(scales_data)
        
        # 计算
        success = self.kernel.compute_socs(Lx, Ly, Nx, Ny, nkernels)
        
        if not success:
            raise RuntimeError("SOCS compute failed")
        
        # 读取结果
        img_out = self.kernel.read_img_out_batch(Lx, Ly)
        
        if self.verbose:
            print(f"SOCS mode completed. Output size: {len(img_out)}")
        
        return img_out

#=============================================================================
# Main Entry Point
#=============================================================================

def main():
    parser = argparse.ArgumentParser(description="FPGA-Litho BRAM Host Application")
    
    parser.add_argument("-d", "--device", type=int, default=0,
                       help="Device index (default: 0)")
    parser.add_argument("-x", "--xclbin", type=str, required=True,
                       help="Path to xclbin file")
    parser.add_argument("-m", "--mode", type=int, choices=[1, 2], required=True,
                       help="Compute mode: 1=TCC, 2=SOCS")
    parser.add_argument("-v", "--verbose", action="store_true",
                       help="Verbose output")
    
    # Size parameters
    parser.add_argument("--Lx", type=int, default=64, help="Lx size")
    parser.add_argument("--Ly", type=int, default=64, help="Ly size")
    parser.add_argument("--Nx", type=int, default=3, help="Nx size")
    parser.add_argument("--Ny", type=int, default=3, help="Ny size")
    parser.add_argument("--nkernels", type=int, default=8, help="Number of kernels")
    
    # Test mode
    parser.add_argument("--test", action="store_true",
                       help="Run in test mode with generated data")
    
    args = parser.parse_args()
    
    print("========================================")
    print("FPGA-Litho BRAM Host Application")
    print("Phase 6D: Single-Function Architecture")
    print("========================================")
    print(f"Mode: {'TCC' if args.mode == 1 else 'SOCS'}")
    print(f"Device: {args.device}")
    print(f"Xclbin: {args.xclbin}")
    print("========================================\n")
    
    # 创建应用
    app = LithoBRAMApp(args.device, args.xclbin, args.verbose)
    
    # 生成测试数据
    if args.test:
        print("Generating test data...")
        
        Lx, Ly = args.Lx, args.Ly
        Nx, Ny = args.Nx, args.Ny
        nkernels = args.nkernels
        
        if args.mode == 1:  # TCC
            # 生成测试数据
            mask_data = np.random.randn(Lx * Ly) + 1j * np.random.randn(Lx * Ly)
            tcc_data = np.random.randn(BRAM_TCC_SIZE) + 1j * np.random.randn(BRAM_TCC_SIZE)
            
            # 运行计算
            start_time = time.time()
            imgf = app.run_tcc_mode(mask_data, tcc_data, Lx, Ly, Nx, Ny)
            elapsed = time.time() - start_time
            
            print(f"\nResult: {len(imgf)} complex values")
            print(f"Total time: {elapsed:.2f}s")
            print(f"Sample output: imgf[0] = {imgf[0]}")
        
        else:  # SOCS
            # 生成测试数据
            mask_data = np.random.randn(Lx * Ly) + 1j * np.random.randn(Lx * Ly)
            kernels_data = np.random.randn(nkernels * 225) + 1j * np.random.randn(nkernels * 225)
            scales_data = np.random.randn(nkernels).astype(np.float32)
            
            # 运行计算
            start_time = time.time()
            img_out = app.run_socs_mode(mask_data, kernels_data, scales_data,
                                        Lx, Ly, Nx, Ny, nkernels)
            elapsed = time.time() - start_time
            
            print(f"\nResult: {len(img_out)} float values")
            print(f"Total time: {elapsed:.2f}s")
            print(f"Sample output: img_out[0] = {img_out[0]}")
    
    print("\nDone.")

if __name__ == "__main__":
    main()