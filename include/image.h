#pragma once
#include <cstdint>

// RGBA pixel buffer (packed R,G,B,A bytes, row-major).
struct ImageBuf {
    uint8_t* data = nullptr;
    int w = 0, h = 0;
};

// Resample src into a new heap-allocated ImageBuf of size (dstW x dstH).
// Uses bilinear interpolation; for downscaling by more than 2x each step
// the image is halved iteratively (mipmap-style) until the ratio is <=2,
// then a final bilinear pass reaches the exact target size.
// Caller must free result.data with delete[].
ImageBuf resampleBilinear(const ImageBuf& src, int dstW, int dstH);
