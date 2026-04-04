#!/usr/bin/env python3
"""
K-Litho BRAM Version XCLBIN Build Script
Phase 6E: Compile Vitis Kernel to XCLBIN

将HLS导出的XO文件编译为xclbin，用于实际硬件测试

@author K-Litho Team
@date 2026-04-04
"""

import subprocess
import os
import sys
import argparse

# Paths
VITIS_PATH = "/root/AMDDesignTools/2025.2/Vitis/bin"
XRT_PATH = "/opt/xilinx/xrt"

def build_xclbin(kernel_name, platform, output_dir):
    """Build xclbin from HLS XO file"""
    
    xo_file = f"{kernel_name}.xo"
    xclbin_file = f"{kernel_name}.{platform}.xclbin"
    
    if not os.path.exists(xo_file):
        print(f"[ERROR] XO file not found: {xo_file}")
        print("Run HLS package first: vitis-run --mode hls --tcl script/run_package_bram.tcl")
        return False
    
    # v++ command for Vitis Kernel flow
    cmd = [
        f"{VITIS_PATH}/v++",
        "--compile",
        "--target", "hw",
        "--platform", platform,
        "--kernel", kernel_name,
        "--output", xclbin_file,
        "--work_dir", output_dir,
        xo_file
    ]
    
    print(f"[INFO] Building xclbin for {platform}")
    print(f"[CMD] {' '.join(cmd)}")
    
    try:
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=3600)
        if result.returncode == 0:
            print(f"[SUCCESS] xclbin created: {xclbin_file}")
            return True
        else:
            print(f"[ERROR] v++ failed:")
            print(result.stderr)
            return False
    except subprocess.TimeoutExpired:
        print("[ERROR] Build timeout (1 hour)")
        return False
    except Exception as e:
        print(f"[ERROR] Build exception: {e}")
        return False

def main():
    parser = argparse.ArgumentParser(description='Build K-Litho BRAM xclbin')
    parser.add_argument('--kernel', default='hls_litho_system_bram', help='Kernel name')
    parser.add_argument('--platform', default='xcku3p-ffvb676-2-e', help='Target platform')
    parser.add_argument('--output_dir', default='vitis_build', help='Build output directory')
    
    args = parser.parse_args()
    
    # Check environment
    if not os.path.exists(VITIS_PATH):
        print(f"[ERROR] Vitis not found: {VITIS_PATH}")
        return 1
    
    # Build
    success = build_xclbin(args.kernel, args.platform, args.output_dir)
    return 0 if success else 1

if __name__ == "__main__":
    sys.exit(main())