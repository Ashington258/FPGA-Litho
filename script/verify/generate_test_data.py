#!/usr/bin/env python3
"""
K-Litho BRAM 测试数据生成脚本
生成用于完整功能验证的测试数据文件
"""

import numpy as np
import struct
import json
import os

# 创建数据目录
DATA_DIR = "data/bram_test"
os.makedirs(DATA_DIR, exist_ok=True)

def float_to_hex(f):
    """将float转换为IEEE 754十六进制表示"""
    return struct.unpack('<I', struct.pack('<f', f))[0]

def hex_to_float(h):
    """将IEEE 754十六进制转换为float"""
    return struct.unpack('<f', struct.pack('<I', h))[0]

def generate_point_source(Lx=16, Ly=16):
    """生成点光源数据 (中心点)"""
    source = np.zeros((Ly, Lx), dtype=np.complex64)
    # 中心点光源
    source[Ly//2, Lx//2] = 1.0 + 0j
    # 扩散到周围几个点
    source[Ly//2-1, Lx//2] = 0.5 + 0j
    source[Ly//2+1, Lx//2] = 0.5 + 0j
    source[Ly//2, Lx//2-1] = 0.5 + 0j
    source[Ly//2, Lx//2+1] = 0.5 + 0j
    return source

def generate_annular_source(Lx=16, Ly=16, inner_radius=0.3, outer_radius=0.5):
    """生成环形光源数据"""
    source = np.zeros((Ly, Lx), dtype=np.complex64)
    
    # 坐标归一化到[-1, 1]
    for j in range(Ly):
        for i in range(Lx):
            # 归一化坐标
            fx = (i - Lx/2) / (Lx/2)
            fy = (j - Ly/2) / (Ly/2)
            # 极坐标半径
            r = np.sqrt(fx**2 + fy**2)
            # 环形区域判断
            if inner_radius <= r <= outer_radius:
                source[j, i] = 1.0 + 0j
    
    # 归一化
    total = np.sum(np.abs(source))
    if total > 0:
        source = source / total
    
    return source

def generate_constant_mask(Lx=16, Ly=16):
    """生成常数掩模数据 (全1)"""
    mask = np.ones((Ly, Lx), dtype=np.complex64)
    return mask

def generate_linespace_mask(Lx=16, Ly=16, period=4, width=2):
    """生成线条掩模数据"""
    mask = np.zeros((Ly, Lx), dtype=np.complex64)
    for i in range(Lx):
        if (i % period) < width:
            mask[:, i] = 1.0 + 0j
    return mask

def generate_socs_kernels(Nx=3, Ny=3, nkernels=4):
    """生成SOCS核数据"""
    # 核尺寸: (2Nx+1) x (2Ny+1) = 7x7
    kernel_size = (2*Nx+1) * (2*Ny+1)
    kernels = np.zeros((nkernels, kernel_size), dtype=np.complex64)
    
    # 生成不同特征的核
    for k in range(nkernels):
        kernel = np.zeros((2*Ny+1, 2*Nx+1), dtype=np.complex64)
        
        # 核1: 中心点
        if k == 0:
            kernel[Ny, Nx] = 1.0 + 0j
        # 核2: 水平线
        elif k == 1:
            kernel[Ny, :] = 0.5 + 0j
        # 核3: 垂直线
        elif k == 2:
            kernel[:, Nx] = 0.5 + 0j
        # 核4: 对角线
        elif k == 3:
            for d in range(min(2*Nx+1, 2*Ny+1)):
                kernel[d, d] = 0.25 + 0j
        
        kernels[k] = kernel.flatten()
    
    return kernels

def generate_scales(nkernels=4):
    """生成SOCS权重数据"""
    # 均匀权重
    scales = np.ones(nkernels, dtype=np.float32) / nkernels
    return scales

def save_data_hex(data, filename):
    """保存数据为十六进制格式 (用于TCL脚本)"""
    with open(filename, 'w') as f:
        flat_data = data.flatten()
        for i, val in enumerate(flat_data):
            if np.iscomplexobj(val):
                real_hex = float_to_hex(val.real)
                imag_hex = float_to_hex(val.imag)
                f.write(f"{i} {real_hex:08X} {imag_hex:08X}\n")
            else:
                real_hex = float_to_hex(val)
                f.write(f"{i} {real_hex:08X}\n")

def save_data_binary(data, filename):
    """保存数据为二进制格式 (用于快速加载)"""
    with open(filename, 'wb') as f:
        flat_data = data.flatten()
        if np.iscomplexobj(data):
            # 复数: real, imag交替
            for val in flat_data:
                f.write(struct.pack('<f', val.real))
                f.write(struct.pack('<f', val.imag))
        else:
            # 实数
            for val in flat_data:
                f.write(struct.pack('<f', val))

def generate_test_dataset():
    """生成完整测试数据集"""
    print("生成测试数据集...")
    
    # 参数配置
    config = {
        "Lx": 16,
        "Ly": 16,
        "Nx": 3,
        "Ny": 3,
        "srcSize": 16,
        "nkernels": 4,
        "description": "SOCS模式完整验证数据"
    }
    
    # 生成光源数据
    print("  - 光源数据 (环形光源)")
    source = generate_annular_source(Lx=config["Lx"], Ly=config["Ly"])
    save_data_hex(source, f"{DATA_DIR}/source_hex.txt")
    save_data_binary(source, f"{DATA_DIR}/source.bin")
    
    # 生成掩模数据
    print("  - 掩模数据 (线条掩模)")
    mask = generate_linespace_mask(Lx=config["Lx"], Ly=config["Ly"])
    save_data_hex(mask, f"{DATA_DIR}/mask_hex.txt")
    save_data_binary(mask, f"{DATA_DIR}/mask.bin")
    
    # 生成SOCS核数据
    print("  - SOCS核数据 (4核)")
    kernels = generate_socs_kernels(Nx=config["Nx"], Ny=config["Ny"], nkernels=config["nkernels"])
    save_data_hex(kernels, f"{DATA_DIR}/kernels_hex.txt")
    save_data_binary(kernels, f"{DATA_DIR}/kernels.bin")
    
    # 生成权重数据
    print("  - 权重数据")
    scales = generate_scales(nkernels=config["nkernels"])
    save_data_hex(scales, f"{DATA_DIR}/scales_hex.txt")
    save_data_binary(scales, f"{DATA_DIR}/scales.bin")
    
    # 保存配置
    print("  - 配置文件")
    with open(f"{DATA_DIR}/config.json", 'w') as f:
        json.dump(config, f, indent=2)
    
    # 生成数据摘要 (转换numpy类型为Python原生类型)
    summary = {
        "source": {
            "size": list(source.shape),
            "total_elements": int(source.size),
            "nonzero": int(np.sum(np.abs(source) > 0)),
            "max_magnitude": float(np.max(np.abs(source)))
        },
        "mask": {
            "size": list(mask.shape),
            "total_elements": int(mask.size),
            "nonzero": int(np.sum(np.abs(mask) > 0)),
            "max_magnitude": float(np.max(np.abs(mask)))
        },
        "kernels": {
            "num_kernels": int(kernels.shape[0]),
            "kernel_size": int(kernels.shape[1]),
            "nonzero_per_kernel": [int(np.sum(np.abs(k) > 0)) for k in kernels]
        },
        "scales": {
            "num_scales": int(scales.size),
            "values": [float(s) for s in scales]
        }
    }
    
    with open(f"{DATA_DIR}/summary.json", 'w') as f:
        json.dump(summary, f, indent=2)
    
    print(f"\n数据生成完成!")
    print(f"输出目录: {DATA_DIR}/")
    print(f"\n文件列表:")
    for filename in os.listdir(DATA_DIR):
        filepath = f"{DATA_DIR}/{filename}"
        filesize = os.path.getsize(filepath)
        print(f"  {filename}: {filesize} bytes")
    
    return config, summary

def generate_tcl_load_script(config):
    """生成TCL数据加载脚本"""
    print("\n生成TCL加载脚本...")
    
    tcl_script = f"""# 自动生成的数据加载脚本
# 配置: Lx={config['Lx']}, Ly={config['Ly']}, Nx={config['Nx']}, Ny={config['Ny']}

puts "加载测试数据..."

# 配置参数
axi_write $LX_OFFSET {config['Lx']}
axi_write $LY_OFFSET {config['Ly']}
axi_write $N_OFFSET {config['Nx']}
axi_write $M_OFFSET {config['Ny']}
axi_write $SRC_SIZE {config['srcSize']}
axi_write $NKERNELS {config['nkernels']}
"""

    # 加载光源数据 (从十六进制文件)
    with open(f"{DATA_DIR}/source_hex.txt", 'r') as f:
        lines = f.readlines()[:20]  # 只加载前20个点作为演示
        for line in lines:
            idx, real_hex, imag_hex = line.strip().split()
            tcl_script += f"""
axi_write $IDX_LOW {idx}
axi_write $VAL_IN_REAL 0x{real_hex}
axi_write $VAL_IN_IMAG 0x{imag_hex}
axi_write $OPERATION $OP_LOAD_SOURCE
start_kernel
"""

    tcl_script += "\nputs \"数据加载完成\"\n"
    
    with open(f"{DATA_DIR}/load_data.tcl", 'w') as f:
        f.write(tcl_script)
    
    print(f"  TCL脚本: {DATA_DIR}/load_data.tcl")

if __name__ == "__main__":
    config, summary = generate_test_dataset()
    generate_tcl_load_script(config)
    
    print("\n" + "="*50)
    print("测试数据生成完成!")
    print("="*50)
    print("\n使用方法:")
    print(f"1. 在Vivado Hardware Manager中运行:")
    print(f"   source script/verify/board_verify_full.tcl")
    print(f"\n2. 或使用自动加载脚本:")
    print(f"   source data/bram_test/load_data.tcl")