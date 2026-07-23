// Unit tests for resampleBilinear() in image.h / image.cpp
#include "image.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>

static int tests_run = 0;
static int tests_passed = 0;

#define ASSERT(expr, msg) \
    do { \
        ++tests_run; \
        if (!(expr)) { \
            std::fprintf(stderr, "  FAIL [line %d]: %s\n", __LINE__, msg); \
            return 1; \
        } \
        ++tests_passed; \
    } while(0)

#define ASSERT_EQ(a, b, msg) \
    do { \
        ++tests_run; \
        if ((a) != (b)) { \
            std::fprintf(stderr, "  FAIL [line %d]: %s (got %d, expected %d)\n", \
                         __LINE__, msg, (int)(a), (int)(b)); \
            return 1; \
        } \
        ++tests_passed; \
    } while(0)

#define ASSERT_NEAR(a, b, tol, msg) \
    do { \
        ++tests_run; \
        if (std::abs((double)(a) - (double)(b)) > (tol)) { \
            std::fprintf(stderr, "  FAIL [line %d]: %s (got %d, expected ~%d, tol %d)\n", \
                         __LINE__, msg, (int)(a), (int)(b), (int)(tol)); \
            return 1; \
        } \
        ++tests_passed; \
    } while(0)

// Build a solid-color 1x1 RGBA image.
static ImageBuf makeSolid(int w, int h, uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
    ImageBuf buf;
    buf.w = w; buf.h = h;
    buf.data = new uint8_t[w * h * 4];
    for (int i = 0; i < w * h; ++i) {
        buf.data[i*4+0] = r;
        buf.data[i*4+1] = g;
        buf.data[i*4+2] = b;
        buf.data[i*4+3] = a;
    }
    return buf;
}

// Build a 2x2 checkerboard: top-left and bottom-right = c0, others = c1.
static ImageBuf make2x2(uint8_t r0, uint8_t g0, uint8_t b0,
                        uint8_t r1, uint8_t g1, uint8_t b1) {
    ImageBuf buf;
    buf.w = 2; buf.h = 2;
    buf.data = new uint8_t[2 * 2 * 4];
    uint8_t px[4][4] = {
        {r0,g0,b0,255}, {r1,g1,b1,255},
        {r1,g1,b1,255}, {r0,g0,b0,255}
    };
    for (int i = 0; i < 4; ++i)
        std::memcpy(buf.data + i*4, px[i], 4);
    return buf;
}

// ---------------------------------------------------------------------------

static int test_invalid_input() {
    std::fprintf(stderr, "test_invalid_input...\n");
    ImageBuf src = makeSolid(4, 4, 128, 128, 128);
    ImageBuf r1 = resampleBilinear(src, 0, 4);
    ASSERT(r1.data == nullptr, "zero dstW should return null");
    ImageBuf r2 = resampleBilinear(src, 4, 0);
    ASSERT(r2.data == nullptr, "zero dstH should return null");
    delete[] src.data;
    return 0;
}

static int test_same_size() {
    std::fprintf(stderr, "test_same_size...\n");
    ImageBuf src = makeSolid(3, 3, 100, 150, 200);
    ImageBuf dst = resampleBilinear(src, 3, 3);
    ASSERT(dst.data != nullptr, "result must not be null");
    ASSERT_EQ(dst.w, 3, "width");
    ASSERT_EQ(dst.h, 3, "height");
    // All pixels should equal the source color exactly
    for (int i = 0; i < 9; ++i) {
        ASSERT_NEAR(dst.data[i*4+0], 100, 1, "R channel preserved");
        ASSERT_NEAR(dst.data[i*4+1], 150, 1, "G channel preserved");
        ASSERT_NEAR(dst.data[i*4+2], 200, 1, "B channel preserved");
    }
    delete[] src.data;
    delete[] dst.data;
    return 0;
}

static int test_upscale_solid_color() {
    std::fprintf(stderr, "test_upscale_solid_color...\n");
    // A solid-color image upscaled should stay the same color everywhere.
    ImageBuf src = makeSolid(2, 2, 80, 160, 240);
    ImageBuf dst = resampleBilinear(src, 8, 8);
    ASSERT(dst.data != nullptr, "result not null");
    ASSERT_EQ(dst.w, 8, "width");
    ASSERT_EQ(dst.h, 8, "height");
    for (int i = 0; i < 64; ++i) {
        ASSERT_NEAR(dst.data[i*4+0], 80,  1, "R uniform");
        ASSERT_NEAR(dst.data[i*4+1], 160, 1, "G uniform");
        ASSERT_NEAR(dst.data[i*4+2], 240, 1, "B uniform");
    }
    delete[] src.data;
    delete[] dst.data;
    return 0;
}

static int test_downscale_solid_color() {
    std::fprintf(stderr, "test_downscale_solid_color...\n");
    // Downscaling a solid-color image should also stay the same color.
    ImageBuf src = makeSolid(16, 16, 200, 100, 50);
    ImageBuf dst = resampleBilinear(src, 4, 4);
    ASSERT(dst.data != nullptr, "result not null");
    ASSERT_EQ(dst.w, 4, "width");
    ASSERT_EQ(dst.h, 4, "height");
    for (int i = 0; i < 16; ++i) {
        ASSERT_NEAR(dst.data[i*4+0], 200, 1, "R uniform");
        ASSERT_NEAR(dst.data[i*4+1], 100, 1, "G uniform");
        ASSERT_NEAR(dst.data[i*4+2],  50, 1, "B uniform");
    }
    delete[] src.data;
    delete[] dst.data;
    return 0;
}

static int test_upscale_gradient_center() {
    std::fprintf(stderr, "test_upscale_gradient_center...\n");
    // 1x2 image: top=0, bottom=255. Upscaled to 1x4.
    // The center interpolated values should be monotonically increasing.
    ImageBuf src;
    src.w = 1; src.h = 2;
    src.data = new uint8_t[8];
    src.data[0]=0; src.data[1]=0; src.data[2]=0; src.data[3]=255; // row 0: black
    src.data[4]=255; src.data[5]=255; src.data[6]=255; src.data[7]=255; // row 1: white
    ImageBuf dst = resampleBilinear(src, 1, 4);
    ASSERT(dst.data != nullptr, "not null");
    ASSERT_EQ(dst.w, 1, "width");
    ASSERT_EQ(dst.h, 4, "height");
    // Pixels should go from dark to light (stride = 1 pixel * 4 bytes = 4)
    ASSERT(dst.data[0]  < dst.data[4],  "row0 < row1");
    ASSERT(dst.data[4]  < dst.data[8],  "row1 < row2");
    ASSERT(dst.data[8]  < dst.data[12], "row2 < row3");
    delete[] src.data;
    delete[] dst.data;
    return 0;
}

static int test_downscale_2x_averages() {
    std::fprintf(stderr, "test_downscale_2x_averages...\n");
    // 2x1 image: left=0, right=200. Downscale to 1x1 should give ~100.
    ImageBuf src;
    src.w = 2; src.h = 1;
    src.data = new uint8_t[8];
    src.data[0]=0;   src.data[1]=0;   src.data[2]=0;   src.data[3]=255;
    src.data[4]=200; src.data[5]=200; src.data[6]=200; src.data[7]=255;
    ImageBuf dst = resampleBilinear(src, 1, 1);
    ASSERT(dst.data != nullptr, "not null");
    ASSERT_NEAR(dst.data[0], 100, 5, "R is ~average");
    ASSERT_NEAR(dst.data[1], 100, 5, "G is ~average");
    ASSERT_NEAR(dst.data[2], 100, 5, "B is ~average");
    delete[] src.data;
    delete[] dst.data;
    return 0;
}

static int test_multistep_downscale() {
    std::fprintf(stderr, "test_multistep_downscale...\n");
    // Large solid-color downscale (>2x multiple times) should still give correct color.
    ImageBuf src = makeSolid(256, 256, 123, 45, 67);
    ImageBuf dst = resampleBilinear(src, 10, 10);
    ASSERT(dst.data != nullptr, "not null");
    ASSERT_EQ(dst.w, 10, "width");
    ASSERT_EQ(dst.h, 10, "height");
    for (int i = 0; i < 100; ++i) {
        ASSERT_NEAR(dst.data[i*4+0], 123, 2, "R preserved through multi-step");
        ASSERT_NEAR(dst.data[i*4+1],  45, 2, "G preserved through multi-step");
        ASSERT_NEAR(dst.data[i*4+2],  67, 2, "B preserved through multi-step");
    }
    delete[] src.data;
    delete[] dst.data;
    return 0;
}

static int test_alpha_preserved() {
    std::fprintf(stderr, "test_alpha_preserved...\n");
    // Semi-transparent solid image: alpha should be preserved.
    ImageBuf src = makeSolid(4, 4, 255, 0, 0, 128);
    ImageBuf dst = resampleBilinear(src, 8, 8);
    ASSERT(dst.data != nullptr, "not null");
    for (int i = 0; i < 64; ++i) {
        ASSERT_NEAR(dst.data[i*4+3], 128, 1, "alpha preserved");
    }
    delete[] src.data;
    delete[] dst.data;
    return 0;
}

static int test_asymmetric_scale() {
    std::fprintf(stderr, "test_asymmetric_scale...\n");
    // Scale a solid image with different X/Y ratios.
    ImageBuf src = makeSolid(4, 8, 60, 120, 180);
    ImageBuf dst = resampleBilinear(src, 12, 3);
    ASSERT(dst.data != nullptr, "not null");
    ASSERT_EQ(dst.w, 12, "width");
    ASSERT_EQ(dst.h,  3, "height");
    for (int i = 0; i < 36; ++i) {
        ASSERT_NEAR(dst.data[i*4+0],  60, 1, "R");
        ASSERT_NEAR(dst.data[i*4+1], 120, 1, "G");
        ASSERT_NEAR(dst.data[i*4+2], 180, 1, "B");
    }
    delete[] src.data;
    delete[] dst.data;
    return 0;
}

static int test_fit_placement_uses_shared_bottom_edge() {
    std::fprintf(stderr, "test_fit_placement_uses_shared_bottom_edge...\n");
    ImageRect landscape = fitImageToArea(1600, 900, 100, 50, 400, 400);
    ImageRect portrait = fitImageToArea(900, 1600, 100, 50, 400, 400);

    int landscapeLeft = landscape.x - 100;
    int landscapeRight = 500 - (landscape.x + landscape.w);
    int portraitLeft = portrait.x - 100;
    int portraitRight = 500 - (portrait.x + portrait.w);
    ASSERT(std::abs(landscapeLeft - landscapeRight) <= 1, "landscape centered horizontally");
    ASSERT(std::abs(portraitLeft - portraitRight) <= 1, "portrait centered horizontally");
    ASSERT_EQ(landscape.y + landscape.h, 450, "landscape bottom aligned");
    ASSERT_EQ(portrait.y + portrait.h, 450, "portrait bottom aligned");
    ASSERT(landscape.w <= 400 && landscape.h <= 400, "landscape contained");
    ASSERT(portrait.w <= 400 && portrait.h <= 400, "portrait contained");
    return 0;
}

// ---------------------------------------------------------------------------

int main() {
    int failures = 0;
    int results[] = {
        test_invalid_input(),
        test_same_size(),
        test_upscale_solid_color(),
        test_downscale_solid_color(),
        test_upscale_gradient_center(),
        test_downscale_2x_averages(),
        test_multistep_downscale(),
        test_alpha_preserved(),
        test_asymmetric_scale(),
        test_fit_placement_uses_shared_bottom_edge(),
    };
    const char* names[] = {
        "invalid_input",
        "same_size",
        "upscale_solid_color",
        "downscale_solid_color",
        "upscale_gradient_center",
        "downscale_2x_averages",
        "multistep_downscale",
        "alpha_preserved",
        "asymmetric_scale",
        "fit_placement_uses_shared_bottom_edge",
    };
    int n = sizeof(results)/sizeof(results[0]);
    for (int i = 0; i < n; ++i) {
        if (results[i] != 0) {
            ++failures;
            std::fprintf(stderr, "FAILED: %s\n", names[i]);
        } else {
            std::fprintf(stderr, "PASSED: %s\n", names[i]);
        }
    }
    std::fprintf(stderr, "\n========================================\n");
    std::fprintf(stderr, "Results: %d/%d assertions passed", tests_passed, tests_run);
    if (failures > 0)
        std::fprintf(stderr, ", %d test(s) FAILED\n", failures);
    else
        std::fprintf(stderr, ", all tests PASSED\n");
    std::fprintf(stderr, "========================================\n");
    return failures > 0 ? 1 : 0;
}
