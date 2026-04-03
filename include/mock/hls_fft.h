// Mock hls_fft for standalone compilation
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
