// Mock hls_stream for standalone compilation
#ifndef HLS_STREAM_H
#define HLS_STREAM_H

template<typename T>
class hls_stream {
public:
    void write(T val) {}
    T read() { return T(); }
};

#endif
