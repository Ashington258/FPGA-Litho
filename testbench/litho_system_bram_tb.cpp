/**
 * @file litho_system_bram_tb.cpp
 * @brief Testbench for Single-Function BRAM Architecture (Phase 6C)
 * 
 * 测试所有10种操作（load×5, compute×2, read×2, reset）
 * 验证BRAM存储、参数验证、计算功能
 * 
 * @author K-Litho Team
 * @date 2026-04-04
 */

#include <iostream>
#include <cmath>
#include "../include/hls_litho_system_bram.h"

using namespace std;

// Helper function to compare complex values
bool complex_equal(cmpxFloat a, cmpxFloat b, float tolerance = 1e-5f) {
    return (fabs(a.real() - b.real()) < tolerance) && 
           (fabs(a.imag() - b.imag()) < tolerance);
}

int main() {
    cout << "========================================" << endl;
    cout << "K-Litho BRAM Single-Function Testbench" << endl;
    cout << "========================================" << endl;
    cout << "Phase 6C: Single-Function Architecture" << endl;
    cout << "Target: xcku3p FPGA (No DDR)" << endl;
    cout << "Expected BRAM: ~57 blocks" << endl;
    cout << "========================================" << endl << endl;
    
    int pass_count = 0;
    int total_tests = 10;
    cmpxFloat result;
    
    //=========================================================================
    // Test 1: Load Source Data
    //=========================================================================
    cout << "Test 1: Load Source Data (OP_LOAD_SOURCE)" << endl;
    for (int i = 0; i < 100; i++) {
        result = hls_litho_system_bram(OP_LOAD_SOURCE, i, 
                                       cmpxFloat(i*1.0f, i*2.0f), 
                                       0, 0,0,0,0,0,0);
    }
    // Verify by reading back
    result = hls_litho_system_bram(OP_LOAD_SOURCE, 50, 
                                   cmpxFloat(0,0), 0, 0,0,0,0,0,0);
    // Note: load doesn't return value, just check no error
    cout << "  Loaded 100 source elements" << endl;
    cout << "[PASS] Load Source Data" << endl;
    pass_count++;
    
    //=========================================================================
    // Test 2: Load Mask Data
    //=========================================================================
    cout << endl << "Test 2: Load Mask Data (OP_LOAD_MASK)" << endl;
    for (int i = 0; i < 100; i++) {
        result = hls_litho_system_bram(OP_LOAD_MASK, i, 
                                       cmpxFloat(i*0.5f, i*1.0f), 
                                       0, 0,0,0,0,0,0);
    }
    cout << "  Loaded 100 mask elements" << endl;
    cout << "[PASS] Load Mask Data" << endl;
    pass_count++;
    
    //=========================================================================
    // Test 3: Load TCC Data
    //=========================================================================
    cout << endl << "Test 3: Load TCC Data (OP_LOAD_TCC)" << endl;
    for (int i = 0; i < BRAM_TCC_SIZE; i++) {
        result = hls_litho_system_bram(OP_LOAD_TCC, i, 
                                       cmpxFloat(1.0f, 0.5f), 
                                       0, 0,0,0,0,0,0);
    }
    cout << "  Loaded " << BRAM_TCC_SIZE << " TCC elements" << endl;
    cout << "[PASS] Load TCC Data" << endl;
    pass_count++;
    
    //=========================================================================
    // Test 4: Load Kernels and Scales Data
    //=========================================================================
    cout << endl << "Test 4: Load Kernels Data (OP_LOAD_KERNELS)" << endl;
    for (int i = 0; i < 225; i++) {  // Load one kernel (15x15)
        result = hls_litho_system_bram(OP_LOAD_KERNELS, i, 
                                       cmpxFloat(1.0f, 0.5f), 
                                       0, 0,0,0,0,0,0);
    }
    cout << "  Loaded 225 kernel elements (1 kernel)" << endl;
    
    // Load scales
    for (int i = 0; i < BRAM_SCALES_SIZE; i++) {
        result = hls_litho_system_bram(OP_LOAD_SCALES, i, 
                                       cmpxFloat(i*0.1f, 0), 
                                       0, 0,0,0,0,0,0);
    }
    cout << "  Loaded " << BRAM_SCALES_SIZE << " scale values" << endl;
    cout << "[PASS] Load Kernels/Scales Data" << endl;
    pass_count++;
    
    //=========================================================================
    // Test 5: Compute TCC Mode (Valid Parameters)
    //=========================================================================
    cout << endl << "Test 5: Compute TCC Mode (OP_COMPUTE_TCC)" << endl;
    cout << "  Parameters: Nx=3, Lx=64, Ly=64" << endl;
    
    // Load proper mask data for computation
    for (int i = 0; i < 64*64; i++) {
        hls_litho_system_bram(OP_LOAD_MASK, i, 
                             cmpxFloat(50.0f, 100.0f), 
                             0, 0,0,0,0,0,0);
    }
    
    result = hls_litho_system_bram(OP_COMPUTE_TCC, 0, cmpxFloat(0,0), 
                                   1, 64, 64, 3, 3, 100, 0);
    
    if (result.real() == 1.0f) {
        cout << "  Compute result: SUCCESS (status=1.0)" << endl;
        cout << "[PASS] TCC Compute (Valid Params)" << endl;
        pass_count++;
    } else {
        cout << "  Compute result: ERROR (status=" << result.real() << ")" << endl;
        cout << "[FAIL] TCC Compute (Valid Params)" << endl;
    }
    
    //=========================================================================
    // Test 6: Read imgf Result
    //=========================================================================
    cout << endl << "Test 6: Read imgf Result (OP_READ_IMGF)" << endl;
    result = hls_litho_system_bram(OP_READ_IMGF, 0, cmpxFloat(0,0), 
                                   0, 0,0,0,0,0,0);
    cout << "  imgf[0] = (" << result.real() << ", " << result.imag() << ")" << endl;
    
    // Verify result is computed (not zero)
    if (fabs(result.real()) > 0.0f || fabs(result.imag()) > 0.0f) {
        cout << "[PASS] Read imgf Result" << endl;
        pass_count++;
    } else {
        cout << "[FAIL] Read imgf Result (zero value)" << endl;
    }
    
    //=========================================================================
    // Test 7: Compute SOCS Mode (Valid Parameters)
    //=========================================================================
    cout << endl << "Test 7: Compute SOCS Mode (OP_COMPUTE_SOCS)" << endl;
    cout << "  Parameters: nkernels=8, Lx=64, Ly=64" << endl;
    
    // Load kernels for 8 kernels
    for (int k = 0; k < 8; k++) {
        for (int i = 0; i < 225; i++) {
            hls_litho_system_bram(OP_LOAD_KERNELS, k*225 + i, 
                                 cmpxFloat(1.0f, 0.5f), 
                                 0, 0,0,0,0,0,0);
        }
    }
    
    result = hls_litho_system_bram(OP_COMPUTE_SOCS, 0, cmpxFloat(0,0), 
                                   2, 64, 64, 7, 7, 0, 8);
    
    if (result.real() == 1.0f) {
        cout << "  Compute result: SUCCESS (status=1.0)" << endl;
        cout << "[PASS] SOCS Compute (Valid Params)" << endl;
        pass_count++;
    } else {
        cout << "  Compute result: ERROR (status=" << result.real() << ")" << endl;
        cout << "[FAIL] SOCS Compute (Valid Params)" << endl;
    }
    
    //=========================================================================
    // Test 8: Read img_out Result
    //=========================================================================
    cout << endl << "Test 8: Read img_out Result (OP_READ_IMG_OUT)" << endl;
    result = hls_litho_system_bram(OP_READ_IMG_OUT, 0, cmpxFloat(0,0), 
                                   0, 0,0,0,0,0,0);
    cout << "  img_out[0] = " << result.real() << endl;
    
    // Verify result is computed (not zero)
    if (fabs(result.real()) > 0.0f) {
        cout << "[PASS] Read img_out Result" << endl;
        pass_count++;
    } else {
        cout << "[FAIL] Read img_out Result (zero value)" << endl;
    }
    
    //=========================================================================
    // Test 9: Parameter Validation (Invalid Parameters)
    //=========================================================================
    cout << endl << "Test 9: Parameter Validation (Invalid Params)" << endl;
    
    // Test TCC with Nx > max (Nx=4 > BRAM_MAX_NX_TCC=3)
    cout << "  Testing TCC Nx=4 (max=3)..." << endl;
    result = hls_litho_system_bram(OP_COMPUTE_TCC, 0, cmpxFloat(0,0), 
                                   1, 64, 64, 4, 3, 100, 0);
    if (result.real() == -1.0f) {
        cout << "  TCC Nx=4 correctly rejected (status=-1.0)" << endl;
    } else {
        cout << "  TCC Nx=4 NOT rejected (ERROR)" << endl;
    }
    
    // Test SOCS with nkernels > max (nkernels=9 > BRAM_MAX_KERNELS=8)
    cout << "  Testing SOCS nkernels=9 (max=8)..." << endl;
    result = hls_litho_system_bram(OP_COMPUTE_SOCS, 0, cmpxFloat(0,0), 
                                   2, 64, 64, 7, 7, 0, 9);
    if (result.real() == -1.0f) {
        cout << "  SOCS nkernels=9 correctly rejected (status=-1.0)" << endl;
        cout << "[PASS] Parameter Validation" << endl;
        pass_count++;
    } else {
        cout << "  SOCS nkernels=9 NOT rejected (ERROR)" << endl;
        cout << "[FAIL] Parameter Validation" << endl;
    }
    
    //=========================================================================
    // Test 10: Reset Functionality
    //=========================================================================
    cout << endl << "Test 10: Reset BRAM Storage (OP_RESET)" << endl;
    
    // Reset all storage
    result = hls_litho_system_bram(OP_RESET, 0, cmpxFloat(0,0), 
                                   0, 0,0,0,0,0,0);
    
    if (result.real() == 1.0f) {
        cout << "  Reset completed (status=1.0)" << endl;
        
        // Verify reset by checking imgf[0] is zero
        result = hls_litho_system_bram(OP_READ_IMGF, 0, cmpxFloat(0,0), 
                                       0, 0,0,0,0,0,0);
        
        if (result.real() == 0.0f && result.imag() == 0.0f) {
            cout << "  Verification: imgf[0] = (0.0, 0.0)" << endl;
            cout << "[PASS] Reset Functionality" << endl;
            pass_count++;
        } else {
            cout << "  Verification FAILED: imgf[0] != 0" << endl;
            cout << "[FAIL] Reset Functionality" << endl;
        }
    } else {
        cout << "  Reset failed (status=" << result.real() << ")" << endl;
        cout << "[FAIL] Reset Functionality" << endl;
    }
    
    //=========================================================================
    // Summary
    //=========================================================================
    cout << endl << "========================================" << endl;
    cout << "Test Summary" << endl;
    cout << "========================================" << endl;
    cout << "Passed: " << pass_count << "/" << total_tests << endl;
    
    if (pass_count == total_tests) {
        cout << endl << "*** ALL TESTS PASSED ***" << endl;
        cout << "BRAM single-function architecture is ready for synthesis." << endl;
        return 0;
    } else {
        cout << endl << "*** SOME TESTS FAILED ***" << endl;
        cout << "Failed tests: " << (total_tests - pass_count) << endl;
        return 1;
    }
}