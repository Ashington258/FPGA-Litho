// Mock hls_math for standalone compilation
#ifndef HLS_MATH_H
#define HLS_MATH_H

#include <cmath>

namespace hls {
    inline float sqrt(float x) { return std::sqrt(x); }
    inline float exp(float x) { return std::exp(x); }
}

#endif
