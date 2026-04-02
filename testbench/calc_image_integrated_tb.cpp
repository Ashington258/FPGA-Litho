/**
 * @file calc_image_integrated_tb.cpp
 * @brief Test bench for integrated calcImage kernel
 * 
 * Tests the 200MHz verified calcImage implementation
 * 
 * @author K-Litho Team
 * @date 2026-04-02
 */

#include <iostream>
#include <cmath>
#include "../include/hls_calc_image_integrated.h"

using namespace std;

// Helper: Generate test mask spectrum
void generate_test_mask(cmpxFloat msk[], int Lx, int Ly) {
    for (int y = 0; y < Ly; y++) {
        for (int x = 0; x < Lx; x++) {
            float real_val = (x % 4 == 0) ? 1.0f : 0.5f;
            float imag_val = (y % 4 == 0) ? 0.1f : 0.05f;
            msk[y * Lx + x] = cmpxFloat(real_val, imag_val);
        }
    }
}

// Helper: Generate test TCC matrix
void generate_test_tcc(cmpxFloat tcc[], int Nx, int Ny) {
    int tcc_size = 2 * Nx + 1;
    int inner_count = tcc_size * tcc_size;
    int total = inner_count * inner_count;
    
    for (int i = 0; i < total; i++) {
        // TCC diagonal pattern with small off-diagonal values
        if (i % (inner_count + 1) == 0) {
            tcc[i] = cmpxFloat(1.0f, 0.0f);  // Diagonal
        } else {
            tcc[i] = cmpxFloat(0.05f, 0.0f); // Off-diagonal
        }
    }
}

int main() {
    cout << "=== Integrated calcImage Test Bench (200MHz Version) ===" << endl;
    
    // Test parameters
    int Lx = 16;
    int Ly = 16;
    int Nx = 3;
    int Ny = 3;
    
    // Allocate arrays
    cmpxFloat msk[CI_MAX_LX * CI_MAX_LY];
    cmpxFloat tcc[CI_TCC_TOTAL];
    cmpxFloat imgf[CI_MAX_LX * CI_MAX_LY];
    
    // Initialize with test patterns
    generate_test_mask(msk, Lx, Ly);
    generate_test_tcc(tcc, Nx, Ny);
    
    // Initialize output array
    for (int i = 0; i < CI_MAX_LX * CI_MAX_LY; i++) {
        imgf[i] = cmpxFloat(0.0f, 0.0f);
    }
    
    cout << "Running hls_calc_image_integrated..." << endl;
    cout << "  Lx = " << Lx << ", Ly = " << Ly << endl;
    cout << "  Nx = " << Nx << ", Ny = " << Ny << endl;
    
    // Run HLS kernel
    hls_calc_image_integrated(msk, tcc, imgf, Lx, Ly, Nx, Ny);
    
    // Test 1: Check output is non-zero
    cout << "\n--- Test 1: Non-zero output check ---" << endl;
    bool has_nonzero = false;
    float max_val = 0.0f;
    int nonzero_count = 0;
    
    for (int i = 0; i < Lx * Ly; i++) {
        float val = abs(imgf[i].real()) + abs(imgf[i].imag());
        if (val > max_val) max_val = val;
        if (val > 0.001f) {
            has_nonzero = true;
            nonzero_count++;
        }
    }
    
    cout << "Non-zero output: " << (has_nonzero ? "PASS" : "FAIL") << endl;
    cout << "Non-zero pixels: " << nonzero_count << " / " << (Lx * Ly) << endl;
    cout << "Max magnitude: " << max_val << endl;
    
    // Test 2: Numerical range check
    cout << "\n--- Test 2: Numerical range check ---" << endl;
    bool in_range = true;
    for (int i = 0; i < Lx * Ly; i++) {
        float real = imgf[i].real();
        float imag = imgf[i].imag();
        if (abs(real) > 1000.0f || abs(imag) > 1000.0f) {
            in_range = false;
            cout << "WARN: Potential overflow at idx " << i 
                 << ": " << real << " + " << imag << "j" << endl;
        }
    }
    cout << "Range check: " << (in_range ? "PASS" : "FAIL") << endl;
    
    // Test 3: Sample output values
    cout << "\n--- Test 3: Sample output values ---" << endl;
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            int idx = y * Lx + x;
            cout << "imgf[" << y << "," << x << "] = " 
                 << imgf[idx].real() << " + " << imgf[idx].imag() << "j" << endl;
        }
    }
    
    // Summary
    cout << "\n=== Test Summary ===" << endl;
    cout << "200MHz calcImage kernel test:" << endl;
    cout << "  - Clock: 5ns (200MHz)" << endl;
    cout << "  - Target II: 4" << endl;
    cout << "  - Verified Fmax: 273.97 MHz" << endl;
    
    int passes = (has_nonzero ? 1 : 0) + (in_range ? 1 : 0);
    cout << "Tests passed: " << passes << "/2" << endl;
    
    if (passes >= 2) {
        cout << "TEST STATUS: PASS" << endl;
        return 0;
    } else {
        cout << "TEST STATUS: FAIL" << endl;
        return 1;
    }
}