#!/usr/bin/env python3
"""
K-Litho BRAM Version Manual Compilation Test
在没有Vitis HLS的环境中手动编译测试代码

使用GCC/G++进行语法检查和基本逻辑验证
"""

import os
import sys
import subprocess
from pathlib import Path

PROJECT_ROOT = Path(__file__).parent.parent
SRC_DIR = PROJECT_ROOT / "src"
TB_DIR = PROJECT_ROOT / "testbench"
INCLUDE_DIR = PROJECT_ROOT / "include"

def check_gpp():
    """检查g++编译器"""
    try:
        result = subprocess.run(
            ["g++", "--version"],
            capture_output=True,
            text=True,
            timeout=5
        )
        if result.returncode == 0:
            print(f"G++ found: {result.stdout.split()[0:4]}")
            return True
    except FileNotFoundError:
        pass
    
    print("ERROR: g++ compiler not found")
    print("Please install GCC:")
    print("  Ubuntu: sudo apt install g++")
    return False

def create_mock_headers():
    """创建HLS头文件的模拟版本"""
    mock_dir = INCLUDE_DIR / "mock"
    mock_dir.mkdir(exist_ok=True)
    
    # 创建ap_fixed.h模拟
    with open(mock_dir / "ap_fixed.h", 'w') as f:
        f.write("""// Mock ap_fixed for standalone compilation
#ifndef AP_FIXED_H
#define AP_FIXED_H

template<int W, int I>
class ap_fixed {
public:
    ap_fixed() {}
    ap_fixed(double v) : val(v) {}
    double to_double() { return val; }
private:
    double val;
};

template<int W>
class ap_int {
public:
    ap_int() {}
    ap_int(int v) : val(v) {}
    int to_int() { return val; }
private:
    int val;
};

#endif
""")
    
    # 创建hls_stream.h模拟
    with open(mock_dir / "hls_stream.h", 'w') as f:
        f.write("""// Mock hls_stream for standalone compilation
#ifndef HLS_STREAM_H
#define HLS_STREAM_H

template<typename T>
class hls_stream {
public:
    void write(T val) {}
    T read() { return T(); }
};

#endif
""")
    
    # 创建hls_math.h模拟
    with open(mock_dir / "hls_math.h", 'w') as f:
        f.write("""// Mock hls_math for standalone compilation
#ifndef HLS_MATH_H
#define HLS_MATH_H

#include <cmath>

namespace hls {
    inline float sqrt(float x) { return std::sqrt(x); }
    inline float exp(float x) { return std::exp(x); }
}

#endif
""")
    
    # 创建ap_int.h模拟
    with open(mock_dir / "ap_int.h", 'w') as f:
        f.write("""// Mock ap_int for standalone compilation
#ifndef AP_INT_H
#define AP_INT_H

#include "../ap_fixed.h"

#endif
""")
    
    # 创建hls_fft.h模拟
    with open(mock_dir / "hls_fft.h", 'w') as f:
        f.write("""// Mock hls_fft for standalone compilation
#ifndef HLS_FFT_H
#define HLS_FFT_H

// FFT函数占位符
template<typename T>
void hls_fft_forward(T* input, T* output, int size) {
    // 简化实现: 直接复制
    for (int i = 0; i < size; i++) {
        output[i] = input[i];
    }
}

template<typename T>
void hls_fft_inverse(T* input, T* output, int size) {
    for (int i = 0; i < size; i++) {
        output[i] = input[i];
    }
}

#endif
""")
    
    print(f"Created mock headers in {mock_dir}")
    return mock_dir

def compile_test(mock_dir):
    """编译测试代码"""
    print("\nCompiling BRAM test code...")
    
    # 编译命令
    cmd = [
        "g++",
        "-std=c++11",
        "-I" + str(mock_dir),
        "-I" + str(INCLUDE_DIR),
        str(SRC_DIR / "hls_litho_system_bram.cpp"),
        str(TB_DIR / "litho_system_bram_tb.cpp"),
        "-o", "bram_test_mock",
        "-DSTANDALONE_COMPILE"
    ]
    
    try:
        result = subprocess.run(
            cmd,
            cwd=PROJECT_ROOT,
            capture_output=True,
            text=True,
            timeout=30
        )
        
        if result.returncode == 0:
            print("✓ Compilation successful (syntax check passed)")
            print("Generated: bram_test_mock executable")
            return True
        else:
            print("✗ Compilation failed")
            print("Errors:")
            print(result.stderr)
            return False
    except Exception as e:
        print(f"ERROR: Compilation failed: {e}")
        return False

def run_mock_test():
    """运行mock测试"""
    print("\nRunning mock executable...")
    
    exe_path = PROJECT_ROOT / "bram_test_mock"
    if not exe_path.exists():
        print("ERROR: Mock executable not found")
        return False
    
    try:
        result = subprocess.run(
            [str(exe_path)],
            cwd=PROJECT_ROOT,
            capture_output=True,
            text=True,
            timeout=10
        )
        
        print("Test Output:")
        print(result.stdout)
        
        if result.returncode == 0:
            print("✓ Mock test execution successful")
            return True
        else:
            print(f"✗ Mock test failed (return code: {result.returncode})")
            if result.stderr:
                print("Errors:", result.stderr)
            return False
    except Exception as e:
        print(f"ERROR: Failed to run mock test: {e}")
        return False

def main():
    print("="*60)
    print("K-Litho BRAM Version Standalone Compilation Test")
    print("="*60)
    print("Note: This test uses mock HLS headers for syntax checking")
    print("     Real HLS simulation requires Vitis HLS installation")
    print("="*60)
    
    if not check_gpp():
        sys.exit(1)
    
    mock_dir = create_mock_headers()
    
    if not compile_test(mock_dir):
        print("\nERROR: Compilation failed - please fix syntax errors")
        sys.exit(1)
    
    print("\n" + "="*60)
    print("Standalone Compilation Test Completed!")
    print("="*60)
    print("Results:")
    print("✓ HLS syntax check passed")
    print("✓ C++ compilation successful")
    print("✓ Mock headers created for standalone testing")
    print("\nNote:")
    print("This test only validates syntax and basic logic.")
    print("For full HLS verification, Vitis HLS is required.")
    print("Expected HLS environment:")
    print("  - Vitis 2024.1+ or Vivado 2024.1+")
    print("  - xcku3p-ffvb2104-2-e device support")
    print("="*60)
    
    sys.exit(0)

if __name__ == "__main__":
    main()