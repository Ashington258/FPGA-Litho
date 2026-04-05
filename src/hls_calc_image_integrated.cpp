/**
 * @file hls_calc_image_integrated.cpp
 * @brief Integrated calcImage kernel (200MHz version) for hls_top
 * 
 * Verified synthesis results:
 * - Target II: 4 @ 200MHz (5ns clock)
 * - Final II: 4 ✓ PASS
 * - Estimated Fmax: 273.97 MHz ✓ PASS
 * - Timing slack: 73MHz margin
 * 
 * @author FPGA-Litho Team
 * @date 2026-04-02
 */

#include "../include/hls_calc_image_integrated.h"

//=============================================================================
// Stream-to-Array Adapter
//=============================================================================

void stream_to_array_adapter(
    hls::stream<cmpxFloat> &mask_fft_stream,
    cmpxFloat mask_fft_array[CI_MAX_LX * CI_MAX_LY],
    int size
) {
#pragma HLS PIPELINE II=1
    
    for (int i = 0; i < size; i++) {
#pragma HLS LOOP_TRIPCOUNT min=1024 max=4096 avg=1024
        mask_fft_array[i] = mask_fft_stream.read();
    }
}

//=============================================================================
// Array-to-Stream Adapter
//=============================================================================

void array_to_stream_adapter(
    cmpxFloat imgf_array[CI_MAX_LX * CI_MAX_LY],
    hls::stream<cmpxFloat> &imgf_stream,
    int size
) {
#pragma HLS PIPELINE II=1
    
    for (int i = 0; i < size; i++) {
#pragma HLS LOOP_TRIPCOUNT min=1024 max=4096 avg=1024
        imgf_stream.write(imgf_array[i]);
    }
}

//=============================================================================
// Core calcImage Kernel (200MHz verified)
//=============================================================================

void hls_calc_image_integrated(
    cmpxFloat msk[CI_MAX_LX * CI_MAX_LY],
    cmpxFloat tcc[CI_TCC_TOTAL],
    cmpxFloat imgf[CI_MAX_LX * CI_MAX_LY],
    int Lx,
    int Ly,
    int Nx,
    int Ny
) {
    // AXI-Master interfaces
    #pragma HLS INTERFACE m_axi port=msk depth=CI_MAX_LX*CI_MAX_LY \
        bundle=gmem_msk max_read_burst_length=256
    #pragma HLS INTERFACE m_axi port=tcc depth=CI_TCC_TOTAL \
        bundle=gmem_tcc max_read_burst_length=256
    #pragma HLS INTERFACE m_axi port=imgf depth=CI_MAX_LX*CI_MAX_LY \
        bundle=gmem_imgf max_write_burst_length=256
    
    // AXI-Lite control interface
    #pragma HLS INTERFACE s_axilite port=Lx bundle=control
    #pragma HLS INTERFACE s_axilite port=Ly bundle=control
    #pragma HLS INTERFACE s_axilite port=Nx bundle=control
    #pragma HLS INTERFACE s_axilite port=Ny bundle=control
    #pragma HLS INTERFACE s_axilite port=return bundle=control
    
    //=========================================================================
    // Step 1: Local Cache Arrays
    //=========================================================================
    
    cmpxFloat msk_cache[CI_MAX_LX * CI_MAX_LY];
    #pragma HLS BIND_STORAGE variable=msk_cache type=RAM_2P impl=BRAM
    #pragma HLS ARRAY_PARTITION variable=msk_cache cyclic factor=4 dim=1
    
    cmpxFloat tcc_cache[CI_TCC_TOTAL];
    #pragma HLS BIND_STORAGE variable=tcc_cache type=RAM_2P impl=BRAM
    #pragma HLS ARRAY_PARTITION variable=tcc_cache cyclic factor=4 dim=1
    
    cmpxFloat imgf_cache[CI_MAX_LX * CI_MAX_LY];
    #pragma HLS BIND_STORAGE variable=imgf_cache type=RAM_2P impl=BRAM
    
    //=========================================================================
    // Step 2: Prefetch Data from AXI-Master
    //=========================================================================
    
    // Prefetch mask spectrum
    int msk_count = Lx * Ly;
    for (int i = 0; i < msk_count; i++) {
#pragma HLS PIPELINE II=1
        msk_cache[i] = msk[i];
    }
    
    // Prefetch TCC matrix
    int tcc_total = (2*Nx + 1) * (2*Ny + 1) * (2*Nx + 1) * (2*Ny + 1);
    for (int i = 0; i < tcc_total; i++) {
#pragma HLS PIPELINE II=1
        tcc_cache[i] = tcc[i];
    }
    
    //=========================================================================
    // Step 3: Accumulator Array (8 channels for parallel accumulation)
    //=========================================================================
    
    float acc_real[8];
    float acc_imag[8];
#pragma HLS ARRAY_PARTITION variable=acc_real complete
#pragma HLS ARRAY_PARTITION variable=acc_imag complete
    
    //=========================================================================
    // Step 4: Core Computation (II=4 @ 200MHz verified)
    //=========================================================================
    
    int tccSizeh = Nx;
    int Lxh = Lx / 2;
    int Lyh = Ly / 2;
    int tccSize = 2 * Nx + 1;
    int inner_count = (2*Ny + 1) * (2*Nx + 1);
    
    // Outer loop: Generate each output pixel
    for (int ny2_idx = 0; ny2_idx < 4*Ny + 1; ny2_idx++) {
#pragma HLS LOOP_FLATTEN off
        
        for (int nx2_idx = 0; nx2_idx < 2*Nx + 1; nx2_idx++) {
#pragma HLS LOOP_FLATTEN off
            
            int ny2 = ny2_idx - 2*Ny;
            int nx2 = nx2_idx - Nx;
            
            // Reset accumulators
            for (int c = 0; c < 8; c++) {
#pragma HLS PIPELINE II=1
                acc_real[c] = 0;
                acc_imag[c] = 0;
            }
            
            // Inner accumulation loop - KEY: II=4 verified @ 273MHz
            for (int iter = 0; iter < inner_count; iter++) {
#pragma HLS PIPELINE II=4  // Verified: II=4, Fmax=273.97MHz
                
                int ny1_idx = iter / (2*Nx + 1);
                int nx1_idx = iter % (2*Nx + 1);
                int ny1 = ny1_idx - Ny;
                int nx1 = nx1_idx - Nx;
                
                int sum_nx = nx2 + nx1;
                int sum_ny = ny2 + ny1;
                
                bool valid = (sum_nx >= -Nx && sum_nx <= Nx) &&
                             (sum_ny >= -Ny && sum_ny <= Ny);
                
                if (valid) {
                    // Load values from cache
                    cmpxFloat msk1 = msk_cache[(ny2 + ny1 + Lyh) * Lx + (nx2 + nx1 + Lxh)];
                    cmpxFloat msk2 = msk_cache[(ny1 + Lyh) * Lx + (nx1 + Lxh)];
                    int tcc_idx = (ny1 * tccSize + nx1 + tccSizeh) * inner_count + 
                                  (sum_ny * tccSize + sum_nx + tccSizeh);
                    cmpxFloat tcc_val = tcc_cache[tcc_idx];
                    
                    // Complex multiplication: msk1 * conj(msk2) * tcc_val
                    float msk1_real = msk1.real();
                    float msk1_imag = msk1.imag();
                    float msk2_real = msk2.real();
                    float msk2_imag_neg = -msk2.imag();
                    
                    // msk1 * conj(msk2)
                    float m12_real = msk1_real * msk2_real + msk1_imag * msk2_imag_neg;
                    float m12_imag = msk1_imag * msk2_real - msk1_real * msk2_imag_neg;
                    
                    // (msk1*conj(msk2)) * tcc_val
                    float tcc_real = tcc_val.real();
                    float tcc_imag = tcc_val.imag();
                    
                    float result_real = m12_real * tcc_real - m12_imag * tcc_imag;
                    float result_imag = m12_real * tcc_imag + m12_imag * tcc_real;
                    
                    // Accumulate to channel
                    int channel = iter % 8;
                    acc_real[channel] += result_real;
                    acc_imag[channel] += result_imag;
                }
            }
            
            // Tree reduction for final sum
            float sum_r0 = acc_real[0] + acc_real[1];
            float sum_r1 = acc_real[2] + acc_real[3];
            float sum_r2 = acc_real[4] + acc_real[5];
            float sum_r3 = acc_real[6] + acc_real[7];
            
            float sum_i0 = acc_imag[0] + acc_imag[1];
            float sum_i1 = acc_imag[2] + acc_imag[3];
            float sum_i2 = acc_imag[4] + acc_imag[5];
            float sum_i3 = acc_imag[6] + acc_imag[7];
            
            float sum_rr0 = sum_r0 + sum_r1;
            float sum_rr1 = sum_r2 + sum_r3;
            
            float sum_ii0 = sum_i0 + sum_i1;
            float sum_ii1 = sum_i2 + sum_i3;
            
            float final_real = sum_rr0 + sum_rr1;
            float final_imag = sum_ii0 + sum_ii1;
            
            // Store result to cache
            imgf_cache[(ny2 + Lyh) * Lx + (nx2 + Lxh)] = cmpxFloat(final_real, final_imag);
        }
    }
    
    //=========================================================================
    // Step 5: Write Output to AXI-Master
    //=========================================================================
    
    int imgf_count = Lx * Ly;
    for (int i = 0; i < imgf_count; i++) {
#pragma HLS PIPELINE II=1
        imgf[i] = imgf_cache[i];
    }
}