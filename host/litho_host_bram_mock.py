#!/usr/bin/env python3
"""
BRAM接口模拟驱动 - 验证接口设计合理性

用途: 在实现HLS代码之前验证地址映射、数据格式、参数传递逻辑
无需: 实际FPGA硬件、比特流文件、Vitis环境

作者: FPGA-Litho项目组
日期: 2026-04-03
"""

import numpy as np
import struct
from typing import Tuple, Optional


class LithoBRAMMockDriver:
    """BRAM接口模拟驱动类
    
    模拟AXI-Lite接口访问本地BRAM存储，验证接口设计正确性
    """
    
    # BRAM存储区域地址映射 (参考 doc/BRAM_INTERFACE_MAPPING.md)
    ADDR_SOURCE_BASE    = 0x00010000  # 光源数据起始地址
    ADDR_MASK_BASE      = 0x00020000  # 掩模频谱起始地址
    ADDR_TCC_BASE       = 0x00030000  # TCC矩阵起始地址
    ADDR_KERNELS_BASE   = 0x00040000  # SOCS核起始地址
    ADDR_SCALES_BASE    = 0x00050000  # SOCS权重起始地址
    ADDR_IMGF_BASE      = 0x00060000  # 频域输出起始地址
    ADDR_IMG_OUT_BASE   = 0x00070000  # 空间域输出起始地址
    
    # AXI-Lite控制寄存器地址映射
    ADDR_MODE           = 0x00000000  # 工作模式 (1=TCC, 2=SOCS)
    ADDR_LX             = 0x00000004  # 频域尺寸Lx
    ADDR_LY             = 0x00000008  # 频域尺寸Ly
    ADDR_NX             = 0x0000000C  # TCC/SOCS尺寸Nx
    ADDR_NY             = 0x00000010  # TCC/SOCS尺寸Ny
    ADDR_SRC_SIZE       = 0x00000014  # 光源尺寸
    ADDR_NKERNELS       = 0x00000018  # SOCS核数量
    ADDR_START          = 0x0000001C  # 启动控制 (写1启动)
    ADDR_STATUS         = 0x00000020  # 状态寄存器 (0=idle, 1=running, 2=done, 3=error)
    ADDR_BRAM_ADDR      = 0x00000024  # BRAM访问地址
    ADDR_BRAM_DATA_REAL = 0x00000028  # BRAM数据实部
    ADDR_BRAM_DATA_IMAG = 0x0000002C  # BRAM数据虚部
    
    # BRAM存储尺寸限制
    MAX_SOURCE_SIZE     = 64 * 64      # 4096个复数元素
    MAX_MASK_SIZE       = 64 * 64      # 4096个复数元素
    MAX_TCC_SIZE        = 15 * 15      # 225个复数元素 (Nx=3时)
    MAX_KERNELS_SIZE    = 8 * 225      # 1800个复数元素 (8核)
    MAX_SCALES_SIZE     = 8            # 8个浮点数
    MAX_IMGF_SIZE       = 64 * 64      # 4096个复数元素
    MAX_IMG_OUT_SIZE    = 29 * 29      # 841个浮点数
    
    def __init__(self):
        """初始化模拟驱动"""
        # 模拟BRAM存储数组 (使用字典模拟地址空间)
        self.bram_memory = {}
        
        # 模拟控制寄存器
        self.control_registers = {
            'mode': 0,
            'Lx': 0,
            'Ly': 0,
            'Nx': 0,
            'Ny': 0,
            'srcSize': 0,
            'nkernels': 0,
            'start': 0,
            'status': 0,  # 0=idle, 1=running, 2=done, 3=error
        }
        
        # 计算结果缓存 (用于模拟计算)
        self.compute_result = None
        
        # 错误日志
        self.error_log = []
        
        print("✓ LithoBRAMMockDriver initialized")
        print(f"  BRAM capacity: ~230KB (105 blocks)")
        print(f"  Supported modes: TCC(Nx≤3), SOCS(full)")
    
    # ==================== AXI-Lite寄存器读写 ====================
    
    def write_register(self, addr: int, value: int) -> bool:
        """写入控制寄存器
        
        Args:
            addr: 寄存器地址
            value: 写入值 (int)
        
        Returns:
            bool: 写入是否成功
        """
        # 地址映射到寄存器名称
        reg_map = {
            self.ADDR_MODE: 'mode',
            self.ADDR_LX: 'Lx',
            self.ADDR_LY: 'Ly',
            self.ADDR_NX: 'Nx',
            self.ADDR_NY: 'Ny',
            self.ADDR_SRC_SIZE: 'srcSize',
            self.ADDR_NKERNELS: 'nkernels',
            self.ADDR_START: 'start',
            self.ADDR_STATUS: 'status',  # 通常只读，但模拟器允许写入
        }
        
        if addr in reg_map:
            reg_name = reg_map[addr]
            self.control_registers[reg_name] = value
            
            # 启动信号触发模拟计算
            if addr == self.ADDR_START and value == 1:
                return self._trigger_compute()
            
            return True
        else:
            self.error_log.append(f"Invalid register address: 0x{addr:08X}")
            return False
    
    def read_register(self, addr: int) -> int:
        """读取控制寄存器
        
        Args:
            addr: 寄存器地址
        
        Returns:
            int: 寄存器值
        """
        reg_map = {
            self.ADDR_MODE: 'mode',
            self.ADDR_LX: 'Lx',
            self.ADDR_LY: 'Ly',
            self.ADDR_NX: 'Nx',
            self.ADDR_NY: 'Ny',
            self.ADDR_SRC_SIZE: 'srcSize',
            self.ADDR_NKERNELS: 'nkernels',
            self.ADDR_START: 'start',
            self.ADDR_STATUS: 'status',
        }
        
        if addr in reg_map:
            return self.control_registers[reg_map[addr]]
        else:
            self.error_log.append(f"Invalid register address: 0x{addr:08X}")
            return -1
    
    # ==================== BRAM数据加载接口 ====================
    
    def load_source_data(self, idx: int, val: complex) -> bool:
        """加载光源数据 (单个复数)
        
        Args:
            idx: 数组索引 [0, 4095]
            val: 复数值
        
        Returns:
            bool: 加载是否成功
        """
        if idx < 0 or idx >= self.MAX_SOURCE_SIZE:
            self.error_log.append(f"Source index out of bounds: {idx}")
            self.control_registers['status'] = 3  # error
            return False
        
        # 计算地址: base + idx * 8 (每个复数8字节)
        addr = self.ADDR_SOURCE_BASE + idx * 8
        return self._write_complex(addr, val)
    
    def load_mask_data(self, idx: int, val: complex) -> bool:
        """加载掩模频谱数据 (单个复数)
        
        Args:
            idx: 数组索引 [0, 4095]
            val: 复数值
        
        Returns:
            bool: 加载是否成功
        """
        if idx < 0 or idx >= self.MAX_MASK_SIZE:
            self.error_log.append(f"Mask index out of bounds: {idx}")
            self.control_registers['status'] = 3
            return False
        
        addr = self.ADDR_MASK_BASE + idx * 8
        return self._write_complex(addr, val)
    
    def load_tcc_data(self, idx: int, val: complex) -> bool:
        """加载TCC矩阵数据 (单个复数)
        
        Args:
            idx: 数组索引 [0, 224] (Nx=3时)
            val: 复数值
        
        Returns:
            bool: 加载是否成功
        """
        if idx < 0 or idx >= self.MAX_TCC_SIZE:
            self.error_log.append(f"TCC index out of bounds: {idx}")
            self.control_registers['status'] = 3
            return False
        
        addr = self.ADDR_TCC_BASE + idx * 8
        return self._write_complex(addr, val)
    
    def load_kernels_data(self, idx: int, val: complex) -> bool:
        """加载SOCS核数据 (单个复数)
        
        Args:
            idx: 数组索引 [0, 1799] (8核)
            val: 复数值
        
        Returns:
            bool: 加载是否成功
        """
        if idx < 0 or idx >= self.MAX_KERNELS_SIZE:
            self.error_log.append(f"Kernels index out of bounds: {idx}")
            self.control_registers['status'] = 3
            return False
        
        addr = self.ADDR_KERNELS_BASE + idx * 8
        return self._write_complex(addr, val)
    
    def load_scales_data(self, idx: int, val: float) -> bool:
        """加载SOCS权重数据 (单个浮点数)
        
        Args:
            idx: 数组索引 [0, 7]
            val: 浮点数值
        
        Returns:
            bool: 加载是否成功
        """
        if idx < 0 or idx >= self.MAX_SCALES_SIZE:
            self.error_log.append(f"Scales index out of bounds: {idx}")
            self.control_registers['status'] = 3
            return False
        
        # 浮点数地址: base + idx * 4 (每个浮点4字节)
        addr = self.ADDR_SCALES_BASE + idx * 4
        return self._write_float(addr, val)
    
    # ==================== BRAM数据读取接口 ====================
    
    def read_imgf_data(self, idx: int) -> Optional[complex]:
        """读取频域输出数据 (单个复数)
        
        Args:
            idx: 数组索引 [0, 4095]
        
        Returns:
            complex: 复数值或None (失败时)
        """
        if idx < 0 or idx >= self.MAX_IMGF_SIZE:
            self.error_log.append(f"Imgf index out of bounds: {idx}")
            return None
        
        addr = self.ADDR_IMGF_BASE + idx * 8
        return self._read_complex(addr)
    
    def read_img_out_data(self, idx: int) -> Optional[float]:
        """读取空间域输出数据 (单个浮点数)
        
        Args:
            idx: 数组索引 [0, 840]
        
        Returns:
            float: 浮点数值或None (失败时)
        """
        if idx < 0 or idx >= self.MAX_IMG_OUT_SIZE:
            self.error_log.append(f"Img_out index out of bounds: {idx}")
            return None
        
        addr = self.ADDR_IMG_OUT_BASE + idx * 4
        return self._read_float(addr)
    
    # ==================== 批量加载接口 (高效) ====================
    
    def load_source_batch(self, data: np.ndarray) -> bool:
        """批量加载光源数据
        
        Args:
            data: 复数数组 (shape: [Lx, Ly] or [Lx*Ly])
        
        Returns:
            bool: 加载是否成功
        """
        flat_data = data.flatten()
        if len(flat_data) > self.MAX_SOURCE_SIZE:
            self.error_log.append(f"Source batch size exceeds limit: {len(flat_data)}")
            return False
        
        for i, val in enumerate(flat_data):
            if not self.load_source_data(i, val):
                return False
        
        print(f"✓ Loaded {len(flat_data)} source elements")
        return True
    
    def load_mask_batch(self, data: np.ndarray) -> bool:
        """批量加载掩模频谱数据"""
        flat_data = data.flatten()
        if len(flat_data) > self.MAX_MASK_SIZE:
            self.error_log.append(f"Mask batch size exceeds limit: {len(flat_data)}")
            return False
        
        for i, val in enumerate(flat_data):
            if not self.load_mask_data(i, val):
                return False
        
        print(f"✓ Loaded {len(flat_data)} mask elements")
        return True
    
    def load_kernels_batch(self, data: np.ndarray) -> bool:
        """批量加载SOCS核数据"""
        flat_data = data.flatten()
        if len(flat_data) > self.MAX_KERNELS_SIZE:
            self.error_log.append(f"Kernels batch size exceeds limit: {len(flat_data)}")
            return False
        
        for i, val in enumerate(flat_data):
            if not self.load_kernels_data(i, val):
                return False
        
        print(f"✓ Loaded {len(flat_data)} kernel elements")
        return True
    
    def load_scales_batch(self, data: np.ndarray) -> bool:
        """批量加载SOCS权重数据"""
        if len(data) > self.MAX_SCALES_SIZE:
            self.error_log.append(f"Scales batch size exceeds limit: {len(data)}")
            return False
        
        for i, val in enumerate(data):
            if not self.load_scales_data(i, float(val)):
                return False
        
        print(f"✓ Loaded {len(data)} scale elements")
        return True
    
    # ==================== 计算控制接口 ====================
    
    def start_compute(self, mode: int, Lx: int, Ly: int, Nx: int, Ny: int, 
                      srcSize: int = 64, nkernels: int = 8) -> bool:
        """启动计算
        
        Args:
            mode: 1=TCC模式, 2=SOCS模式
            Lx, Ly: 频域尺寸
            Nx, Ny: TCC/SOCS尺寸
            srcSize: 光源尺寸
            nkernels: SOCS核数量
        
        Returns:
            bool: 启动是否成功
        """
        # 参数验证
        if mode not in [1, 2]:
            self.error_log.append(f"Invalid mode: {mode}")
            self.control_registers['status'] = 3
            return False
        
        # TCC模式限制: Nx≤3 (BRAM容量限制)
        if mode == 1 and Nx > 3:
            self.error_log.append(f"TCC mode Nx exceeds limit: Nx={Nx} > 3")
            self.control_registers['status'] = 3
            return False
        
        # 尺寸限制检查
        if Lx > 64 or Ly > 64:
            self.error_log.append(f"Lx/Ly exceeds limit: Lx={Lx}, Ly={Ly}")
            self.control_registers['status'] = 3
            return False
        
        if nkernels > 8:
            self.error_log.append(f"nkernels exceeds limit: {nkernels}")
            self.control_registers['status'] = 3
            return False
        
        # 写入参数寄存器
        self.write_register(self.ADDR_MODE, mode)
        self.write_register(self.ADDR_LX, Lx)
        self.write_register(self.ADDR_LY, Ly)
        self.write_register(self.ADDR_NX, Nx)
        self.write_register(self.ADDR_NY, Ny)
        self.write_register(self.ADDR_SRC_SIZE, srcSize)
        self.write_register(self.ADDR_NKERNELS, nkernels)
        
        # 启动计算
        return self.write_register(self.ADDR_START, 1)
    
    def get_compute_status(self) -> int:
        """获取计算状态
        
        Returns:
            int: 0=idle, 1=running, 2=done, 3=error
        """
        return self.read_register(self.ADDR_STATUS)
    
    def wait_for_completion(self, timeout: int = 10) -> bool:
        """等待计算完成
        
        Args:
            timeout: 超时时间(秒)
        
        Returns:
            bool: 是否成功完成
        """
        import time
        start_time = time.time()
        
        while time.time() - start_time < timeout:
            status = self.get_compute_status()
            if status == 2:  # done
                return True
            elif status == 3:  # error
                print(f"✗ Compute failed: {self.error_log[-1]}")
                return False
            time.sleep(0.001)
        
        print(f"✗ Compute timeout after {timeout}s")
        return False
    
    # ==================== 内部辅助函数 ====================
    
    def _write_complex(self, addr: int, val: complex) -> bool:
        """写入复数数据到BRAM
        
        编码格式: [real, imag] 交替存储
        地址布局: addr=real, addr+4=imag
        """
        # 模拟内存写入
        self.bram_memory[addr] = val.real     # 实部
        self.bram_memory[addr + 4] = val.imag  # 虚部
        return True
    
    def _read_complex(self, addr: int) -> complex:
        """从BRAM读取复数数据"""
        real = self.bram_memory.get(addr, 0.0)
        imag = self.bram_memory.get(addr + 4, 0.0)
        return complex(real, imag)
    
    def _write_float(self, addr: int, val: float) -> bool:
        """写入浮点数到BRAM"""
        self.bram_memory[addr] = val
        return True
    
    def _read_float(self, addr: int) -> float:
        """从BRAM读取浮点数"""
        return self.bram_memory.get(addr, 0.0)
    
    def _trigger_compute(self) -> bool:
        """触发模拟计算
        
        模拟HLS核心计算逻辑:
        - TCC模式: source + mask -> TCC预计算 -> calcImage -> imgf
        - SOCS模式: kernels + mask -> SOCS核心 -> img_out
        """
        mode = self.control_registers['mode']
        self.control_registers['status'] = 1  # running
        
        if mode == 1:
            # TCC模式模拟计算
            print(f"  Running TCC mode (Nx={self.control_registers['Nx']})")
            success = self._simulate_tcc_compute()
        elif mode == 2:
            # SOCS模式模拟计算
            print(f"  Running SOCS mode (nkernels={self.control_registers['nkernels']})")
            success = self._simulate_socs_compute()
        else:
            self.error_log.append(f"Unknown mode: {mode}")
            self.control_registers['status'] = 3
            return False
        
        if success:
            self.control_registers['status'] = 2  # done
            print("✓ Compute completed")
        else:
            self.control_registers['status'] = 3  # error
        
        return success
    
    def _simulate_tcc_compute(self) -> bool:
        """模拟TCC模式计算
        
        简化实现: 直接生成随机输出结果
        实际HLS核心会执行完整的TCC+calcImage流程
        """
        Lx = self.control_registers['Lx']
        Ly = self.control_registers['Ly']
        
        # 检查输入数据是否已加载
        source_loaded = self.ADDR_SOURCE_BASE in self.bram_memory
        mask_loaded = self.ADDR_MASK_BASE in self.bram_memory
        
        if not source_loaded or not mask_loaded:
            self.error_log.append("TCC mode: source/mask data not loaded")
            return False
        
        # 模拟生成频域输出 (imgf)
        imgf_size = Lx * Ly
        for i in range(imgf_size):
            # 简化: 生成随机复数结果
            # 实际HLS会执行完整的calcImage计算
            result = complex(np.random.rand(), np.random.rand())
            addr = self.ADDR_IMGF_BASE + i * 8
            self._write_complex(addr, result)
        
        print(f"  Generated {imgf_size} imgf elements (simulated)")
        return True
    
    def _simulate_socs_compute(self) -> bool:
        """模拟SOCS模式计算
        
        简化实现: 直接生成随机输出结果
        实际HLS核心会执行完整的Kernel-Mask乘法+累加+移位
        """
        Lx = self.control_registers['Lx']
        Ly = self.control_registers['Ly']
        nkernels = self.control_registers['nkernels']
        
        # 检查输入数据是否已加载
        kernels_loaded = self.ADDR_KERNELS_BASE in self.bram_memory
        mask_loaded = self.ADDR_MASK_BASE in self.bram_memory
        scales_loaded = self.ADDR_SCALES_BASE in self.bram_memory
        
        if not kernels_loaded or not mask_loaded or not scales_loaded:
            self.error_log.append("SOCS mode: kernels/mask/scales data not loaded")
            return False
        
        # 模拟生成空间域输出 (img_out)
        img_out_size = Lx * Ly
        for i in range(img_out_size):
            # 简化: 生成随机浮点数结果
            # 实际HLS会执行完整的SOCS计算
            result = float(np.random.rand() * nkernels)
            addr = self.ADDR_IMG_OUT_BASE + i * 4
            self._write_float(addr, result)
        
        print(f"  Generated {img_out_size} img_out elements (simulated)")
        return True
    
    # ==================== 完整流程接口 ====================
    
    def run_tcc_mode(self, source: np.ndarray, mask: np.ndarray, 
                     Lx: int = 64, Ly: int = 64, Nx: int = 3, Ny: int = 3) -> Optional[np.ndarray]:
        """完整TCC模式流程
        
        Args:
            source: 光源数据 (复数数组)
            mask: 掩模频谱数据 (复数数组)
            Lx, Ly: 频域尺寸
            Nx, Ny: TCC尺寸 (Nx≤3限制)
        
        Returns:
            np.ndarray: 频域输出imgf或None (失败时)
        """
        print(f"\n{'='*60}")
        print(f"TCC Mode Workflow (Nx={Nx} constraint)")
        print(f"{'='*60}")
        
        # Step 1: 加载光源数据
        print("Step 1: Loading source data...")
        if not self.load_source_batch(source):
            return None
        
        # Step 2: 加载掩模数据
        print("Step 2: Loading mask data...")
        if not self.load_mask_batch(mask):
            return None
        
        # Step 3: 启动计算
        print("Step 3: Starting compute...")
        if not self.start_compute(mode=1, Lx=Lx, Ly=Ly, Nx=Nx, Ny=Ny):
            return None
        
        # Step 4: 等待完成
        print("Step 4: Waiting for completion...")
        if not self.wait_for_completion():
            return None
        
        # Step 5: 读取结果
        print("Step 5: Reading imgf output...")
        imgf = np.zeros((Lx, Ly), dtype=complex)
        for i in range(Lx * Ly):
            imgf.flat[i] = self.read_imgf_data(i)
        
        print(f"✓ TCC mode completed: output shape {imgf.shape}")
        print(f"{'='*60}\n")
        return imgf
    
    def run_socs_mode(self, kernels: np.ndarray, scales: np.ndarray, mask: np.ndarray,
                      Lx: int = 64, Ly: int = 64, nkernels: int = 8) -> Optional[np.ndarray]:
        """完整SOCS模式流程
        
        Args:
            kernels: SOCS核数据 (复数数组, shape: [nkernels, 225])
            scales: SOCS权重数据 (浮点数组, shape: [nkernels])
            mask: 掩模频谱数据 (复数数组)
            Lx, Ly: 频域尺寸
            nkernels: 核数量
        
        Returns:
            np.ndarray: 空间域输出img_out或None (失败时)
        """
        print(f"\n{'='*60}")
        print(f"SOCS Mode Workflow (nkernels={nkernels})")
        print(f"{'='*60}")
        
        # Step 1: 加载核数据
        print("Step 1: Loading kernels data...")
        if not self.load_kernels_batch(kernels):
            return None
        
        # Step 2: 加载权重数据
        print("Step 2: Loading scales data...")
        if not self.load_scales_batch(scales):
            return None
        
        # Step 3: 加载掩模数据
        print("Step 3: Loading mask data...")
        if not self.load_mask_batch(mask):
            return None
        
        # Step 4: 启动计算
        print("Step 4: Starting compute...")
        if not self.start_compute(mode=2, Lx=Lx, Ly=Ly, Nx=15, Ny=15, nkernels=nkernels):
            return None
        
        # Step 5: 等待完成
        print("Step 5: Waiting for completion...")
        if not self.wait_for_completion():
            return None
        
        # Step 6: 读取结果
        print("Step 6: Reading img_out output...")
        img_out = np.zeros((Lx, Ly), dtype=float)
        for i in range(Lx * Ly):
            img_out.flat[i] = self.read_img_out_data(i)
        
        print(f"✓ SOCS mode completed: output shape {img_out.shape}")
        print(f"{'='*60}\n")
        return img_out
    
    # ==================== 辅助工具函数 ====================
    
    def reset_bram_storage(self) -> bool:
        """重置所有BRAM存储"""
        self.bram_memory.clear()
        self.control_registers['status'] = 0
        print("✓ BRAM storage reset")
        return True
    
    def get_error_log(self) -> list:
        """获取错误日志"""
        return self.error_log
    
    def print_register_status(self):
        """打印寄存器状态"""
        print("\nControl Registers Status:")
        print(f"  mode     : {self.control_registers['mode']}")
        print(f"  Lx/Ly    : {self.control_registers['Lx']}/{self.control_registers['Ly']}")
        print(f"  Nx/Ny    : {self.control_registers['Nx']}/{self.control_registers['Ny']}")
        print(f"  nkernels : {self.control_registers['nkernels']}")
        print(f"  status   : {self.control_registers['status']} (0=idle, 1=run, 2=done, 3=err)")
    
    def verify_address_mapping(self) -> bool:
        """验证地址映射正确性
        
        检查所有地址区域是否不重叠
        """
        regions = [
            ('source', self.ADDR_SOURCE_BASE, self.MAX_SOURCE_SIZE * 8),
            ('mask', self.ADDR_MASK_BASE, self.MAX_MASK_SIZE * 8),
            ('tcc', self.ADDR_TCC_BASE, self.MAX_TCC_SIZE * 8),
            ('kernels', self.ADDR_KERNELS_BASE, self.MAX_KERNELS_SIZE * 8),
            ('scales', self.ADDR_SCALES_BASE, self.MAX_SCALES_SIZE * 4),
            ('imgf', self.ADDR_IMGF_BASE, self.MAX_IMGF_SIZE * 8),
            ('img_out', self.ADDR_IMG_OUT_BASE, self.MAX_IMG_OUT_SIZE * 4),
        ]
        
        # 检查区域重叠
        for i, (name1, start1, size1) in enumerate(regions):
            for j, (name2, start2, size2) in enumerate(regions):
                if i < j:
                    end1 = start1 + size1
                    end2 = start2 + size2
                    if start1 < end2 and start2 < end1:
                        self.error_log.append(f"Address overlap: {name1} and {name2}")
                        return False
        
        print("✓ Address mapping verified: no overlaps")
        return True


# ==================== 使用示例 ====================

def example_usage():
    """演示模拟驱动使用方法"""
    print("\n" + "="*60)
    print("LithoBRAMMockDriver Example Usage")
    print("="*60 + "\n")
    
    # 创建驱动实例
    driver = LithoBRAMMockDriver()
    
    # 示例1: 数据加载验证
    print("\n[Test 1] Data Loading Verification")
    print("-" * 40)
    
    test_complex = complex(1.5, 2.3)
    driver.load_source_data(0, test_complex)
    read_val = driver.read_imgf_data(0)  # 注意: 这里故意测试错误索引
    
    # 正确的读取测试
    driver._write_complex(driver.ADDR_SOURCE_BASE, test_complex)
    read_val = driver._read_complex(driver.ADDR_SOURCE_BASE)
    print(f"  Write: {test_complex}")
    print(f"  Read:  {read_val}")
    print(f"  Match: {read_val == test_complex} ✓")
    
    # 示例2: TCC模式完整流程
    print("\n[Test 2] TCC Mode Workflow")
    print("-" * 40)
    
    source = np.random.rand(64, 64) + np.random.rand(64, 64) * 1j
    mask = np.random.rand(64, 64) + np.random.rand(64, 64) * 1j
    
    imgf = driver.run_tcc_mode(source, mask, Lx=64, Ly=64, Nx=3, Ny=3)
    if imgf is not None:
        print(f"  Output shape: {imgf.shape}")
        print(f"  Max magnitude: {np.max(np.abs(imgf)):.4f}")
    
    # 示例3: SOCS模式完整流程
    print("\n[Test 3] SOCS Mode Workflow")
    print("-" * 40)
    
    driver.reset_bram_storage()
    
    kernels = np.random.rand(8, 225) + np.random.rand(8, 225) * 1j
    scales = np.random.rand(8)
    mask = np.random.rand(64, 64) + np.random.rand(64, 64) * 1j
    
    img_out = driver.run_socs_mode(kernels, scales, mask, Lx=64, Ly=64, nkernels=8)
    if img_out is not None:
        print(f"  Output shape: {img_out.shape}")
        print(f"  Max value: {np.max(img_out):.4f}")
    
    # 示例4: 边界检查验证
    print("\n[Test 4] Boundary Check")
    print("-" * 40)
    
    result = driver.load_source_data(9999, complex(1, 1))  # 越界索引
    print(f"  Out-of-bounds write: success={result} (expected False)")
    print(f"  Status: {driver.get_compute_status()} (expected 3=error)")
    print(f"  Error log: {driver.get_error_log()[-1]}")
    
    print("\n" + "="*60)
    print("Example completed successfully!")
    print("="*60 + "\n")


if __name__ == "__main__":
    example_usage()