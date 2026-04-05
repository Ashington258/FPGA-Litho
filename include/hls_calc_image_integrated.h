/**
 * @file hls_calc_image_integrated.h
 * @brief Integrated calcImage kernel for hls_top
 * 
 * Uses AXI-Master interface with 200MHz timing
 * II=4 @ 200MHz (Fmax: 273MHz verified)
 * 
 * @author FPGA-Litho Team
 * @date 2026-04-02
 */

#ifndef HLS_CALC_IMAGE_INTEGRATED_H
#define HLS_CALC_IMAGE_INTEGRATED_H

#include "hls_types.h"

//=============================================================================
// Configuration Constants (matching hls_top parameters)
//=============================================================================

#ifndef CI_MAX_LX
#define CI_MAX_LX       64
#endif

#ifndef CI_MAX_LY
#define CI_MAX_LY       64
#endif

#ifndef CI_MAX_NX
#define CI_MAX_NX       7
#endif

#ifndef CI_MAX_NY
#define CI_MAX_NY       7
#endif

// TCC matrix size
#define CI_TCC_SIZE     (2*CI_MAX_NX+1) * (2*CI_MAX_NY+1)
#define CI_TCC_TOTAL    CI_TCC_SIZE * CI_TCC_SIZE

//=============================================================================
// Function Declarations
//=============================================================================

/**
 * @brief Integrated calcImage kernel (200MHz version)
 * 
 * Verified synthesis results:
 * - Target II: 4
 * - Final II: 4 ✓
 * - Estimated Fmax: 273.97 MHz ✓
 * 
 * @param msk   Input mask spectrum [Lx * Ly]
 * @param tcc   TCC matrix [(2Nx+1)*(2Ny+1) squared]
 * @param imgf  Output image spectrum [Lx * Ly]
 * @param Lx    Mask width (max 64)
 * @param Ly    Mask height (max 64)
 * @param Nx    TCC half-width (max 7)
 * @param Ny    TCC half-height (max 7)
 */
void hls_calc_image_integrated(
    cmpxFloat msk[CI_MAX_LX * CI_MAX_LY],
    cmpxFloat tcc[CI_TCC_TOTAL],
    cmpxFloat imgf[CI_MAX_LX * CI_MAX_LY],
    int Lx,
    int Ly,
    int Nx,
    int Ny
);

/**
 * @brief Stream-to-Array adapter for calcImage
 * Converts AXI-Stream input to AXI-Master array format
 * 
 * @param mask_fft_stream  Input mask FFT stream
 * @param mask_fft_array   Output mask FFT array
 * @param size             Total size (Lx * Ly)
 */
void stream_to_array_adapter(
    hls::stream<cmpxFloat> &mask_fft_stream,
    cmpxFloat mask_fft_array[CI_MAX_LX * CI_MAX_LY],
    int size
);

/**
 * @brief Array-to-Stream adapter for calcImage output
 * Converts AXI-Master array output to AXI-Stream format
 * 
 * @param imgf_array   Input image spectrum array
 * @param imgf_stream  Output image spectrum stream
 * @param size         Total size (Lx * Ly)
 */
void array_to_stream_adapter(
    cmpxFloat imgf_array[CI_MAX_LX * CI_MAX_LY],
    hls::stream<cmpxFloat> &imgf_stream,
    int size
);

#endif // HLS_CALC_IMAGE_INTEGRATED_H