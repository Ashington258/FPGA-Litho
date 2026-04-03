#!/usr/bin/env python3
"""
BRAM接口验证脚本 - 验证地址映射和数据格式正确性

用途: 在实现HLS代码之前验证接口设计合理性
测试: 地址映射、数据编码、参数传递、边界检查

运行: python verify_bram_interface.py

作者: FPGA-Litho项目组
日期: 2026-04-03
"""

import sys
import numpy as np
from litho_host_bram_mock import LithoBRAMMockDriver


def test_data_loading_cycle():
    """测试1: 数据加载/读取循环验证
    
    验证目标: 写入数据后能否正确读取回来
    """
    print("\n" + "="*70)
    print("TEST 1: Data Loading/Reading Cycle Verification")
    print("="*70)
    
    driver = LithoBRAMMockDriver()
    
    # 测试复数数据加载
    print("\n[A] Complex Data Loading Test")
    print("-" * 50)
    
    test_values = [
        complex(1.5, 2.3),
        complex(-3.7, 4.2),
        complex(0.0, -1.0),
        complex(100.5, 200.3),
    ]
    
    for i, val in enumerate(test_values):
        # 写入光源数据
        driver.load_source_data(i, val)
        
        # 通过地址直接读取验证
        addr = driver.ADDR_SOURCE_BASE + i * 8
        read_val = driver._read_complex(addr)
        
        # 验证数据一致性
        match = (read_val == val)
        status = "✓ PASS" if match else "✗ FAIL"
        print(f"  [{i}] Write: {val} | Read: {read_val} | {status}")
        
        if not match:
            print(f"      ERROR: Data mismatch!")
            return False
    
    # 测试浮点数据加载
    print("\n[B] Float Data Loading Test")
    print("-" * 50)
    
    test_floats = [1.5, -3.7, 0.0, 100.5, 3.14159]
    
    for i, val in enumerate(test_floats):
        # 写入权重数据
        driver.load_scales_data(i, val)
        
        # 读取验证
        addr = driver.ADDR_SCALES_BASE + i * 4
        read_val = driver._read_float(addr)
        
        # 验证数据一致性
        match = (abs(read_val - val) < 1e-6)
        status = "✓ PASS" if match else "✗ FAIL"
        print(f"  [{i}] Write: {val:.4f} | Read: {read_val:.4f} | {status}")
        
        if not match:
            print(f"      ERROR: Data mismatch!")
            return False
    
    print("\n✓ TEST 1 PASSED: All data loading cycles verified")
    return True


def test_parameter_passing():
    """测试2: 接口参数传递验证
    
    验证目标: 启动计算时所有参数是否正确传递到寄存器
    """
    print("\n" + "="*70)
    print("TEST 2: Parameter Passing Verification")
    print("="*70)
    
    driver = LithoBRAMMockDriver()
    
    # 测试TCC模式参数
    print("\n[A] TCC Mode Parameters Test")
    print("-" * 50)
    
    # 加载必要数据
    source = np.ones((64, 64), dtype=complex)
    mask = np.ones((64, 64), dtype=complex)
    driver.load_source_batch(source)
    driver.load_mask_batch(mask)
    
    # 启动TCC模式计算
    params_tcc = {
        'mode': 1,
        'Lx': 64,
        'Ly': 64,
        'Nx': 3,
        'Ny': 3,
        'srcSize': 64,
        'nkernels': 0,
    }
    
    driver.start_compute(**params_tcc)
    
    # 验证寄存器值
    print("\n  Checking TCC mode register values:")
    all_match = True
    for reg_name, expected in params_tcc.items():
        actual = driver.control_registers[reg_name]
        match = (actual == expected)
        status = "✓" if match else "✗"
        print(f"    {reg_name}: expected={expected}, actual={actual} {status}")
        if not match:
            all_match = False
    
    if not all_match:
        print("  ✗ TEST 2 FAILED: TCC parameter mismatch")
        return False
    
    # 测试SOCS模式参数
    print("\n[B] SOCS Mode Parameters Test")
    print("-" * 50)
    
    driver.reset_bram_storage()
    
    # 加载必要数据
    kernels = np.ones((8, 225), dtype=complex)
    scales = np.ones(8, dtype=float)
    driver.load_kernels_batch(kernels)
    driver.load_scales_batch(scales)
    driver.load_mask_batch(mask)
    
    # 启动SOCS模式计算
    params_socs = {
        'mode': 2,
        'Lx': 64,
        'Ly': 64,
        'Nx': 15,
        'Ny': 15,
        'srcSize': 64,
        'nkernels': 8,
    }
    
    driver.start_compute(**params_socs)
    
    # 验证寄存器值
    print("\n  Checking SOCS mode register values:")
    all_match = True
    for reg_name, expected in params_socs.items():
        actual = driver.control_registers[reg_name]
        match = (actual == expected)
        status = "✓" if match else "✗"
        print(f"    {reg_name}: expected={expected}, actual={actual} {status}")
        if not match:
            all_match = False
    
    if not all_match:
        print("  ✗ TEST 2 FAILED: SOCS parameter mismatch")
        return False
    
    print("\n✓ TEST 2 PASSED: All parameter passing verified")
    return True


def test_complex_encoding():
    """测试3: 复数数据编码格式验证
    
    验证目标: 复数是否按 [real, imag] 交替存储
    """
    print("\n" + "="*70)
    print("TEST 3: Complex Data Encoding Verification")
    print("="*70)
    
    driver = LithoBRAMMockDriver()
    
    print("\n[A] Encoding Format Test")
    print("-" * 50)
    
    # 测试复数编码格式
    test_complex = complex(1.5, 2.3)
    
    # 写入复数
    driver.load_source_data(0, test_complex)
    
    # 检查内存布局
    addr_real = driver.ADDR_SOURCE_BASE + 0  # 实部地址
    addr_imag = driver.ADDR_SOURCE_BASE + 4  # 虚部地址
    
    real_in_memory = driver.bram_memory.get(addr_real, 0.0)
    imag_in_memory = driver.bram_memory.get(addr_imag, 0.0)
    
    print(f"  Input complex: {test_complex}")
    print(f"  Memory layout:")
    print(f"    Address 0x{addr_real:08X}: {real_in_memory} (real)")
    print(f"    Address 0x{addr_imag:08X}: {imag_in_memory} (imag)")
    
    # 验证格式正确性
    encoding_correct = (
        abs(real_in_memory - test_complex.real) < 1e-6 and
        abs(imag_in_memory - test_complex.imag) < 1e-6
    )
    
    if encoding_correct:
        print(f"  ✓ Encoding format verified: [real, imag] interleaved")
    else:
        print(f"  ✗ Encoding format incorrect!")
        return False
    
    # 测试批量编码
    print("\n[B] Batch Encoding Test")
    print("-" * 50)
    
    test_array = np.array([
        complex(1.0, 2.0),
        complex(3.0, 4.0),
        complex(5.0, 6.0),
    ])
    
    driver.load_source_batch(test_array)
    
    # 验证每个元素的编码
    for i, expected in enumerate(test_array):
        addr_base = driver.ADDR_SOURCE_BASE + i * 8
        real = driver.bram_memory.get(addr_base, 0.0)
        imag = driver.bram_memory.get(addr_base + 4, 0.0)
        actual = complex(real, imag)
        
        match = (actual == expected)
        status = "✓" if match else "✗"
        print(f"  [{i}] Expected: {expected} | Actual: {actual} | {status}")
        
        if not match:
            print("  ✗ TEST 3 FAILED: Batch encoding error")
            return False
    
    # 测试地址间隔正确性
    print("\n[C] Address Spacing Test")
    print("-" * 50)
    
    # 写入3个连续复数
    for i in range(3):
        driver.load_source_data(i, complex(i, i*10))
    
    # 检查地址间隔
    addr0 = driver.ADDR_SOURCE_BASE + 0 * 8  # 第0个复数
    addr1 = driver.ADDR_SOURCE_BASE + 1 * 8  # 第1个复数
    addr2 = driver.ADDR_SOURCE_BASE + 2 * 8  # 第2个复数
    
    spacing_correct = (addr1 - addr0 == 8 and addr2 - addr1 == 8)
    
    print(f"  Address[0]: 0x{addr0:08X}")
    print(f"  Address[1]: 0x{addr1:08X} (offset: {addr1-addr0} bytes)")
    print(f"  Address[2]: 0x{addr2:08X} (offset: {addr2-addr1} bytes)")
    
    if spacing_correct:
        print(f"  ✓ Address spacing verified: 8 bytes per complex")
    else:
        print(f"  ✗ Address spacing incorrect!")
        return False
    
    print("\n✓ TEST 3 PASSED: Complex encoding format verified")
    return True


def test_boundary_checking():
    """测试4: 地址边界验证
    
    验证目标: 越界访问是否被正确拦截
    """
    print("\n" + "="*70)
    print("TEST 4: Boundary Checking Verification")
    print("="*70)
    
    driver = LithoBRAMMockDriver()
    
    # 测试光源数据边界
    print("\n[A] Source Data Boundary Test")
    print("-" * 50)
    
    # 正常访问
    result_normal = driver.load_source_data(0, complex(1, 1))
    print(f"  Normal access (idx=0): success={result_normal} ✓")
    
    # 越界访问 (idx=9999)
    result_out_of_bounds = driver.load_source_data(9999, complex(1, 1))
    print(f"  Out-of-bounds (idx=9999): success={result_out_of_bounds} (expected False)")
    
    # 检查错误状态
    status = driver.get_compute_status()
    print(f"  Status after error: {status} (expected 3=ERROR)")
    
    if result_out_of_bounds or status != 3:
        print("  ✗ TEST 4 FAILED: Boundary check not working")
        return False
    else:
        print("  ✓ Boundary check working: rejected out-of-bounds access")
    
    # 检查错误日志
    error_log = driver.get_error_log()
    print(f"  Error logged: '{error_log[-1]}'")
    
    # 测试掩模数据边界
    print("\n[B] Mask Data Boundary Test")
    print("-" * 50)
    
    driver.reset_bram_storage()
    
    # 正常访问
    result_normal = driver.load_mask_data(4095, complex(1, 1))  # 最大合法索引
    print(f"  Normal access (idx=4095): success={result_normal} ✓")
    
    # 越界访问 (idx=4096)
    result_out_of_bounds = driver.load_mask_data(4096, complex(1, 1))
    print(f"  Out-of-bounds (idx=4096): success={result_out_of_bounds} (expected False)")
    
    if result_out_of_bounds:
        print("  ✗ TEST 4 FAILED: Mask boundary check not working")
        return False
    
    # 测试SOCS核边界
    print("\n[C] Kernels Data Boundary Test")
    print("-" * 50)
    
    driver.reset_bram_storage()
    
    # 正常访问
    result_normal = driver.load_kernels_data(1799, complex(1, 1))  # 最大合法索引 (8核×225-1)
    print(f"  Normal access (idx=1799): success={result_normal} ✓")
    
    # 越界访问
    result_out_of_bounds = driver.load_kernels_data(1800, complex(1, 1))
    print(f"  Out-of-bounds (idx=1800): success={result_out_of_bounds} (expected False)")
    
    if result_out_of_bounds:
        print("  ✗ TEST 4 FAILED: Kernels boundary check not working")
        return False
    
    # 测试参数边界
    print("\n[D] Parameter Boundary Test")
    print("-" * 50)
    
    driver.reset_bram_storage()
    
    # 加载必要数据
    source = np.ones((64, 64), dtype=complex)
    mask = np.ones((64, 64), dtype=complex)
    driver.load_source_batch(source)
    driver.load_mask_batch(mask)
    
    # 测试TCC模式Nx>3限制
    print("\n  Testing TCC mode Nx constraint:")
    result = driver.start_compute(mode=1, Lx=64, Ly=64, Nx=5, Ny=3)  # Nx=5违规
    print(f"    Nx=5 (limit is 3): success={result} (expected False)")
    
    if result:
        print("    ✗ TEST 4 FAILED: TCC Nx constraint not enforced")
        return False
    else:
        print("    ✓ TCC Nx constraint enforced: rejected Nx=5")
    
    error_log = driver.get_error_log()
    print(f"    Error logged: '{error_log[-1]}'")
    
    # 测试尺寸超限
    print("\n  Testing Lx/Ly size constraint:")
    driver.reset_bram_storage()
    driver.load_source_batch(source)
    driver.load_mask_batch(mask)
    
    result = driver.start_compute(mode=1, Lx=100, Ly=64, Nx=3, Ny=3)  # Lx=100违规
    print(f"    Lx=100 (limit is 64): success={result} (expected False)")
    
    if result:
        print("    ✗ TEST 4 FAILED: Lx constraint not enforced")
        return False
    
    print("\n✓ TEST 4 PASSED: All boundary checks verified")
    return True


def test_address_mapping():
    """测试5: 地址映射正确性验证
    
    验证目标: 所有存储区域地址不重叠
    """
    print("\n" + "="*70)
    print("TEST 5: Address Mapping Verification")
    print("="*70)
    
    driver = LithoBRAMMockDriver()
    
    # 验证地址映射无重叠
    print("\n[A] Address Region Overlap Check")
    print("-" * 50)
    
    result = driver.verify_address_mapping()
    
    if not result:
        print("  ✗ TEST 5 FAILED: Address regions overlap!")
        return False
    
    # 打印地址区域信息
    print("\n[B] Address Region Summary")
    print("-" * 50)
    
    regions = [
        ('source', driver.ADDR_SOURCE_BASE, driver.MAX_SOURCE_SIZE * 8),
        ('mask', driver.ADDR_MASK_BASE, driver.MAX_MASK_SIZE * 8),
        ('tcc', driver.ADDR_TCC_BASE, driver.MAX_TCC_SIZE * 8),
        ('kernels', driver.ADDR_KERNELS_BASE, driver.MAX_KERNELS_SIZE * 8),
        ('scales', driver.ADDR_SCALES_BASE, driver.MAX_SCALES_SIZE * 4),
        ('imgf', driver.ADDR_IMGF_BASE, driver.MAX_IMGF_SIZE * 8),
        ('img_out', driver.ADDR_IMG_OUT_BASE, driver.MAX_IMG_OUT_SIZE * 4),
    ]
    
    print("  Region         Start-End              Size(KB)")
    print("  " + "-" * 50)
    
    for name, start, size in regions:
        end = start + size
        size_kb = size / 1024
        print(f"  {name:<12} 0x{start:08X}-0x{end:08X}  {size_kb:>6.1f}KB")
    
    # 计算总存储需求
    total_kb = sum(size for _, _, size in regions) / 1024
    print("  " + "-" * 50)
    print(f"  Total:        {total_kb:.1f}KB (230KB available)")
    
    if total_kb > 230:
        print(f"  ✗ Storage exceeds BRAM capacity!")
        return False
    else:
        print(f"  ✓ Storage within BRAM capacity ({230-total_kb:.1f}KB free)")
    
    print("\n✓ TEST 5 PASSED: Address mapping verified")
    return True


def run_full_workflow_test():
    """测试6: 完整工作流程验证
    
    验证目标: TCC和SOCS模式完整流程可正常运行
    """
    print("\n" + "="*70)
    print("TEST 6: Full Workflow Verification")
    print("="*70)
    
    driver = LithoBRAMMockDriver()
    
    # 测试TCC完整流程
    print("\n[A] TCC Mode Full Workflow")
    print("-" * 50)
    
    source = np.random.rand(64, 64) + np.random.rand(64, 64) * 1j
    mask = np.random.rand(64, 64) + np.random.rand(64, 64) * 1j
    
    imgf = driver.run_tcc_mode(source, mask, Lx=64, Ly=64, Nx=3, Ny=3)
    
    if imgf is None:
        print("  ✗ TEST 6 FAILED: TCC workflow failed")
        return False
    
    # 验证输出形状
    if imgf.shape != (64, 64):
        print(f"  ✗ TEST 6 FAILED: Output shape incorrect {imgf.shape}")
        return False
    
    # 验证输出有数据
    if np.all(imgf == 0):
        print("  ✗ TEST 6 FAILED: Output is all zeros")
        return False
    
    print(f"  ✓ TCC workflow completed: output shape {imgf.shape}")
    print(f"  ✓ Output magnitude range: [{np.min(np.abs(imgf)):.4f}, {np.max(np.abs(imgf)):.4f}]")
    
    # 测试SOCS完整流程
    print("\n[B] SOCS Mode Full Workflow")
    print("-" * 50)
    
    driver.reset_bram_storage()
    
    kernels = np.random.rand(8, 225) + np.random.rand(8, 225) * 1j
    scales = np.random.rand(8)
    mask = np.random.rand(64, 64) + np.random.rand(64, 64) * 1j
    
    img_out = driver.run_socs_mode(kernels, scales, mask, Lx=64, Ly=64, nkernels=8)
    
    if img_out is None:
        print("  ✗ TEST 6 FAILED: SOCS workflow failed")
        return False
    
    # 验证输出形状
    if img_out.shape != (64, 64):
        print(f"  ✗ TEST 6 FAILED: Output shape incorrect {img_out.shape}")
        return False
    
    # 验证输出有数据
    if np.all(img_out == 0):
        print("  ✗ TEST 6 FAILED: Output is all zeros")
        return False
    
    print(f"  ✓ SOCS workflow completed: output shape {img_out.shape}")
    print(f"  ✓ Output value range: [{np.min(img_out):.4f}, {np.max(img_out):.4f}]")
    
    print("\n✓ TEST 6 PASSED: Full workflows verified")
    return True


def main():
    """运行所有验证测试"""
    print("\n" + "="*70)
    print("BRAM Interface Verification Suite")
    print("="*70)
    print("\nRunning 6 verification tests...")
    print("Purpose: Validate interface design before HLS implementation")
    
    # 运行测试
    tests = [
        ("Data Loading Cycle", test_data_loading_cycle),
        ("Parameter Passing", test_parameter_passing),
        ("Complex Encoding", test_complex_encoding),
        ("Boundary Checking", test_boundary_checking),
        ("Address Mapping", test_address_mapping),
        ("Full Workflow", run_full_workflow_test),
    ]
    
    results = []
    
    for test_name, test_func in tests:
        try:
            result = test_func()
            results.append((test_name, result))
        except Exception as e:
            print(f"\n✗ TEST FAILED: {test_name}")
            print(f"  Exception: {e}")
            results.append((test_name, False))
    
    # 打印测试总结
    print("\n" + "="*70)
    print("Verification Summary")
    print("="*70)
    
    passed = sum(1 for _, result in results if result)
    total = len(results)
    
    print("\nTest Results:")
    for test_name, result in results:
        status = "✓ PASS" if result else "✗ FAIL"
        print(f"  {test_name:<25} {status}")
    
    print("\n" + "-"*70)
    print(f"Total: {passed}/{total} tests passed")
    
    if passed == total:
        print("\n✓ ALL TESTS PASSED!")
        print("  Interface design validated successfully")
        print("  Ready for HLS implementation")
        return 0
    else:
        print("\n✗ SOME TESTS FAILED!")
        print("  Please review interface design before HLS implementation")
        return 1


if __name__ == "__main__":
    exit_code = main()
    sys.exit(exit_code)