#include <gtest/gtest.h>

#include <cmath>

#include "Common/Constants.h"
#include "Reconstruction/FilterUtils.h"

TEST(FilterUtils, PaddedLengthIsSmallestPowerOfTwoAboveTwiceWidth) {
    for (size_t width : { 1, 100, 128, 300, 896 }) {
        const size_t n = paddedFFTLength(width);
        EXPECT_GE(n, 2 * width) << "width=" << width;
        EXPECT_EQ(n & (n - 1), 0u) << "width=" << width;
        EXPECT_LT(n / 2, 2 * width) << "not the smallest power of two, width=" << width;
    }
}

// Ram-Lak should follow |f| (f in cycles per unit length at the isocenter pitch)
TEST(FilterUtils, RamLakResponseFollowsRamp) {
    const size_t nPad = 512;
    const double dp = 0.2;
    const auto H = makeRampResponse(nPad, dp, RampFilter::RamLak);
    ASSERT_EQ(H.size(), nPad / 2 + 1);

    for (size_t k = 8; k < H.size(); k++) {
        const double f = k / (nPad * dp);
        EXPECT_NEAR(H[k], f, 2e-3 * f) << "k=" << k;
    }
    EXPECT_NEAR(H.back(), 1.0 / (2.0 * dp), 1e-3 / dp);  // Nyquist
    EXPECT_GT(H[0], 0.0);                                // band-limited kernel has a small positive DC
    EXPECT_LT(H[0], 0.01 * H.back());
}

// Shepp-Logan should follow |f| * sinc(f * dp), i.e. 1 / (pi * dp) at Nyquist
TEST(FilterUtils, SheppLoganResponseIsApodizedRamp) {
    const size_t nPad = 512;
    const double dp = 0.2;
    const double pi = (double)libcbct::kPi;
    const auto H = makeRampResponse(nPad, dp, RampFilter::SheppLogan);

    for (size_t k = 8; k < H.size(); k++) {
        const double f = k / (nPad * dp);
        const double ideal = std::abs(std::sin(pi * f * dp)) / (pi * dp);
        EXPECT_NEAR(H[k], ideal, 2e-3 * ideal) << "k=" << k;
    }
    EXPECT_NEAR(H.back(), 1.0 / (pi * dp), 1e-3 / dp);
}

TEST(FilterUtils, CosineWeightsAreSymmetricAndBounded) {
    const int w = 64, h = 48;
    const Geometry geom(vec2i(w, h), vec2f(0.5f, 0.5f), vec3i(32, 32, 32), 100.0f, 400.0f);
    const auto weights = makeCosineWeights(geom, w, h);
    ASSERT_EQ(weights.size(), (size_t)w * h);

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            const float c = weights[y * w + x];
            EXPECT_GT(c, 0.0f);
            EXPECT_LE(c, 1.0f);
            EXPECT_FLOAT_EQ(c, weights[(h - 1 - y) * w + (w - 1 - x)]) << "x=" << x << " y=" << y;
        }
    }

    // Corner pixel center: u = -(w/2 - 0.5) * du, v = -(h/2 - 0.5) * dv
    const double u = (0.5 - 0.5 * w) * 0.5, v = (0.5 - 0.5 * h) * 0.5;
    EXPECT_NEAR(weights[0], 400.0 / std::sqrt(400.0 * 400.0 + u * u + v * v), 1e-6);
}
