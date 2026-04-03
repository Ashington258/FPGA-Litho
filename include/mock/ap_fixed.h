// Mock ap_fixed for standalone compilation
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
