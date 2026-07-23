#include "image.h"
#include <algorithm>
#include <cmath>
#include <cstring>

ImageRect fitImageToArea(int srcW, int srcH,
                         int areaX, int areaY, int areaW, int areaH) {
    if (srcW <= 0 || srcH <= 0 || areaW <= 0 || areaH <= 0)
        return {};

    float scaleX = static_cast<float>(areaW) / static_cast<float>(srcW);
    float scaleY = static_cast<float>(areaH) / static_cast<float>(srcH);
    float scale = std::min(scaleX, scaleY);

    int width = std::max(1, std::min(areaW, static_cast<int>(srcW * scale)));
    int height = std::max(1, std::min(areaH, static_cast<int>(srcH * scale)));
    return {
        areaX + (areaW - width) / 2,
        areaY + areaH - height,
        width,
        height
    };
}

// Single bilinear pass: resample src into a freshly-allocated dst of (dstW x dstH).
static ImageBuf bilinearPass(const ImageBuf& src, int dstW, int dstH) {
    ImageBuf dst;
    dst.w = dstW;
    dst.h = dstH;
    dst.data = new uint8_t[dstW * dstH * 4];

    float sx = static_cast<float>(src.w) / static_cast<float>(dstW);
    float sy = static_cast<float>(src.h) / static_cast<float>(dstH);

    for (int dy = 0; dy < dstH; ++dy) {
        // Map dst pixel center to src space: (dy+0.5)*sy - 0.5
        float fy = (dy + 0.5f) * sy - 0.5f;
        int y0 = static_cast<int>(std::floor(fy));
        int y1 = y0 + 1;
        float ty = fy - static_cast<float>(y0);
        y0 = std::max(y0, 0);
        y1 = std::min(y1, src.h - 1);
        ty = std::max(0.0f, std::min(1.0f, ty));

        for (int dx = 0; dx < dstW; ++dx) {
            float fx = (dx + 0.5f) * sx - 0.5f;
            int x0 = static_cast<int>(std::floor(fx));
            int x1 = x0 + 1;
            float tx = fx - static_cast<float>(x0);
            x0 = std::max(x0, 0);
            x1 = std::min(x1, src.w - 1);
            tx = std::max(0.0f, std::min(1.0f, tx));

            const uint8_t* p00 = src.data + (y0 * src.w + x0) * 4;
            const uint8_t* p10 = src.data + (y0 * src.w + x1) * 4;
            const uint8_t* p01 = src.data + (y1 * src.w + x0) * 4;
            const uint8_t* p11 = src.data + (y1 * src.w + x1) * 4;

            uint8_t* out = dst.data + (dy * dstW + dx) * 4;
            for (int c = 0; c < 4; ++c) {
                float v = (1.0f - ty) * ((1.0f - tx) * p00[c] + tx * p10[c])
                        +          ty  * ((1.0f - tx) * p01[c] + tx * p11[c]);
                out[c] = static_cast<uint8_t>(v + 0.5f);
            }
        }
    }
    return dst;
}

ImageBuf resampleBilinear(const ImageBuf& src, int dstW, int dstH) {
    if (src.w <= 0 || src.h <= 0 || dstW <= 0 || dstH <= 0)
        return {nullptr, 0, 0};

    // Multi-step downscaling: halve iteratively while any dimension is still >2x
    // the target. This avoids aliasing that a single large-ratio bilinear pass
    // would introduce by sampling too few source pixels per output pixel.
    ImageBuf cur;
    cur.w = src.w;
    cur.h = src.h;
    cur.data = new uint8_t[src.w * src.h * 4];
    std::memcpy(cur.data, src.data, src.w * src.h * 4);

    while (cur.w / 2 >= dstW * 2 || cur.h / 2 >= dstH * 2) {
        int stepW = std::max(cur.w / 2, dstW);
        int stepH = std::max(cur.h / 2, dstH);
        ImageBuf next = bilinearPass(cur, stepW, stepH);
        delete[] cur.data;
        cur = next;
        if (cur.w == dstW && cur.h == dstH) return cur;
    }

    // Final pass to exact size
    if (cur.w == dstW && cur.h == dstH) return cur;
    ImageBuf result = bilinearPass(cur, dstW, dstH);
    delete[] cur.data;
    return result;
}
