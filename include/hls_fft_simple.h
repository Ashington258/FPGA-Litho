/*
 * K-Litho HLS Simplified FFT Header
 */

#ifndef HLS_FFT_SIMPLE_H
#define HLS_FFT_SIMPLE_H

#include "../include/hls_types.h"
#include <hls_stream.h>

void hls_fft_forward_simple(
    hls::stream<realFloat> &real_in,
    hls::stream<cmpxFloat> &cmplx_out,
    int total_size
);

void hls_fft_inverse_simple(
    hls::stream<cmpxFloat> &cmplx_in,
    hls::stream<realFloat> &real_out,
    int total_size
);

void hls_top_simple(
    hls::stream<realFloat> &data_in,
    hls::stream<realFloat> &data_out,
    int sizeX,
    int sizeY
);

#endif // HLS_FFT_SIMPLE_H