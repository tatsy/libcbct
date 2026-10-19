#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <complex>
#include <vector>

#include "FdkReference.h"
#include "Reconstruction/FeldkampCpu.h"
#include "Reconstruction/FilterUtils.h"
#include "Utils/ImageUtils.h"

namespace libcbct::tests {
namespace {

namespace reference = testing_utils::fdk_reference;

// Compare every voxel, including zeros, and reject NaNs explicitly. A reduction
// using std::max alone can silently discard a NaN difference.
template <typename T>
void expectVolumeNear(const VolumeF32 &actual, const Volume<T> &expected, double tolerance = 1e-6) {
    ASSERT_EQ(actual.size<0>(), expected.template size<0>());
    ASSERT_EQ(actual.size<1>(), expected.template size<1>());
    ASSERT_EQ(actual.size<2>(), expected.template size<2>());
    double maximum = 0.0;
    for (std::size_t i = 0; i < actual.numel(); ++i) {
        ASSERT_TRUE(std::isfinite(actual.ptr()[i])) << "voxel=" << i;
        ASSERT_TRUE(std::isfinite(expected.ptr()[i])) << "voxel=" << i;
        maximum = std::max(maximum, std::abs(static_cast<double>(actual.ptr()[i]) - expected.ptr()[i]));
    }
    EXPECT_LT(maximum, tolerance);
}

VolumeF32 asymmetricInput(int width, int height, int count) {
    VolumeF32 input(width, height, count);
    for (int p = 0; p < count; ++p) {
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                input(x, y, p) = 0.01f * ((3 * x + 5 * y + 7 * p + x * p) % 23 - 11);
            }
        }
    }
    return input;
}

class FdkContracts : public ::testing::TestWithParam<RampFilter> {};

TEST_P(FdkContracts, RampResponseMatchesDirectDftWithoutInverseNormalization) {
    for (const int length : { 4, 16, 64 }) {
        for (const double pitch : { 0.2, 0.75 }) {
            const auto actual = makeRampResponse(length, pitch, GetParam());
            ASSERT_EQ(actual.size(), static_cast<std::size_t>(length / 2 + 1));
            for (int k = 0; k <= length / 2; ++k) {
                std::complex<double> expected{};
                for (int j = 0; j < length; ++j) {
                    const int offset = j <= length / 2 ? j : j - length;
                    const double angle = -static_cast<double>(kTwoPi) * j * k / length;
                    expected += reference::rampKernel(offset, pitch, GetParam())
                                * std::complex<double>(std::cos(angle), std::sin(angle));
                }
                EXPECT_NEAR(expected.imag(), 0.0, 1e-12);
                EXPECT_NEAR(actual[k], expected.real(), 2e-7)
                    << "length=" << length << " pitch=" << pitch << " frequency=" << k;
            }
        }
    }
}

TEST(FdkSamplingContracts, CosineWeightsUsePixelCentersAndBothPixelPitches) {
    const Geometry geometry(vec2i(3, 4), vec2f(0.8f, 1.7f), vec3i(3, 3, 3), 2.0f, 4.0f);
    const auto actual = makeCosineWeights(geometry, 3, 4);
    ASSERT_EQ(actual.size(), 12u);
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 3; ++x) {
            EXPECT_NEAR(actual[y * 3 + x], reference::cosineWeight(geometry, x, y), 1e-7);
        }
    }
}

TEST(FdkSamplingContracts, BilinearInterpolationPreservesHalfPixelExtrapolation) {
    constexpr int width = 4, height = 3;
    // A bilinear polynomial distinguishes axes and exercises all four samples.
    const auto value = [](float x, float y) { return 1.0f + 2.0f * x - 3.0f * y + 0.5f * x * y; };
    std::vector<float> input(width * height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) input[y * width + x] = value(x, y);
    }
    for (const float y : { -0.5f, -0.25f, 0.0f, 0.75f, 2.0f, 2.25f, 2.5f }) {
        for (const float x : { -0.5f, -0.25f, 0.0f, 1.25f, 3.0f, 3.25f, 3.5f }) {
            EXPECT_NEAR(bilerp(input.data(), width, height, x, y), value(x, y), 1e-6)
                << "x=" << x << " y=" << y;
        }
    }
}

TEST_P(FdkContracts, RectangularDetectorMatchesDirectConvolutionReference) {
    const Geometry geometry(vec2i(17, 19), vec2f(0.8f, 1.1f), vec3i(17, 19, 5), 100.0f, 400.0f);
    const auto input = asymmetricInput(17, 19, 7);
    const auto original = input;
    const auto expected = reference::reconstruct(input, geometry, GetParam());
    const auto actual = FeldkampCpu(GetParam()).reconstruct(input, geometry);
    expectVolumeNear(actual, expected);
    EXPECT_TRUE(std::equal(input.elements().begin(), input.elements().end(), original.elements().begin()));
}

TEST_P(FdkContracts, EdgeImpulsesPreserveZeroExtensionAndRowIsolation) {
    const Geometry geometry(vec2i(5, 3), vec2f(1.0f, 0.7f), vec3i(5, 5, 3), 100.0f, 400.0f);
    for (const int column : { 0, 4 }) {
        SCOPED_TRACE(column);
        VolumeF32 input(5, 3, 1);
        input(column, 0, 0) = 0.2f;
        input(4 - column, 2, 0) = -0.1f;
        expectVolumeNear(FeldkampCpu(GetParam()).reconstruct(input, geometry),
                         reference::reconstruct(input, geometry, GetParam()));
    }
}

TEST_P(FdkContracts, IsocenterHasAnalyticScaleIndependentOfProjectionCount) {
    const Geometry geometry(vec2i(2, 2), vec2f(1.0f, 1.0f), vec3i(1, 1, 1), 100.0f, 400.0f);
    const double pi = static_cast<double>(kPi);
    const double weight = 400.0 / std::sqrt(400.0 * 400.0 + 0.5);
    // dp=1/4. Each two-pixel row contributes h[0]+h[1] at the isocenter.
    const double kernelSum = GetParam() == RampFilter::RamLak ? 1.0 - 4.0 / (pi * pi)
                                                             : 16.0 / (3.0 * pi * pi);
    for (const int count : { 1, 2, 7 }) {
        SCOPED_TRACE(count);
        VolumeF32 input(2, 2, count);
        std::fill(input.elements().begin(), input.elements().end(), 0.02f);
        const auto actual = FeldkampCpu(GetParam()).reconstruct(input, geometry);
        EXPECT_NEAR(actual(0, 0, 0), static_cast<double>(0.02f) * weight * kernelSum * pi, 1e-6);
    }
}

TEST_P(FdkContracts, BackprojectionIncludesLowerEdgeAndExcludesUpperEdge) {
    // With x=y=0, z centers project to v=-1,0,1,2,3 exactly. The upper
    // detector edge v=2 must be excluded; v=0 uses half-pixel extrapolation.
    const Geometry geometry(vec2i(2, 2), vec2f(1.0f, 2.0f), vec3i(1, 1, 5), 100.0f, 400.0f);
    VolumeF32 input(2, 2, 1);
    input(0, 0, 0) = input(1, 0, 0) = 0.02f;
    input(0, 1, 0) = input(1, 1, 0) = 0.06f;
    const auto actual = FeldkampCpu(GetParam()).reconstruct(input, geometry);
    const auto filtered = reference::filteredProjection(input, geometry, 0, GetParam());
    const double lowerRow = (filtered[0] + filtered[1]) * 0.5;
    const double upperRow = (filtered[2] + filtered[3]) * 0.5;
    const double pi = static_cast<double>(kPi);
    ASSERT_GT(std::abs(lowerRow - upperRow), 1e-4);
    EXPECT_NEAR(actual(0, 0, 1), (1.5 * lowerRow - 0.5 * upperRow) * pi, 1e-6);
    EXPECT_NEAR(actual(0, 0, 2), (lowerRow + upperRow) * 0.5 * pi, 1e-6);
    EXPECT_EQ(actual(0, 0, 0), 0.0f);
    EXPECT_EQ(actual(0, 0, 3), 0.0f);
    EXPECT_EQ(actual(0, 0, 4), 0.0f);
}

TEST_P(FdkContracts, ProjectionContributionsAccumulateAtTheirOriginalAngles) {
    const Geometry geometry(vec2i(5, 3), vec2f(0.9f, 1.2f), vec3i(5, 7, 3), 100.0f, 400.0f);
    const auto input = asymmetricInput(5, 3, 7);
    const FeldkampCpu fdk(GetParam());
    VolumeF32 sum(5, 7, 3);
    // Keep all seven slots: a one-frame sinogram would change theta and pi/N.
    for (int p = 0; p < 7; ++p) {
        VolumeF32 isolated(5, 3, 7);
        for (int y = 0; y < 3; ++y) {
            for (int x = 0; x < 5; ++x) isolated(x, y, p) = input(x, y, p);
        }
        const auto contribution = fdk.reconstruct(isolated, geometry);
        for (std::size_t i = 0; i < sum.numel(); ++i) sum.ptr()[i] += contribution.ptr()[i];
    }
    const auto actual = fdk.reconstruct(input, geometry);
    expectVolumeNear(actual, sum);
    expectVolumeNear(actual, reference::reconstruct(input, geometry, GetParam()));
}

TEST_P(FdkContracts, RepeatedCallsAndZeroFramesDoNotReuseStaleData) {
    const FeldkampCpu fdk(GetParam());
    const Geometry geometry(vec2i(5, 3), vec2f(1.0f, 0.7f), vec3i(5, 5, 3), 100.0f, 400.0f);
    const auto input = asymmetricInput(5, 3, 7);
    const auto first = fdk.reconstruct(input, geometry);
    expectVolumeNear(first, reference::reconstruct(input, geometry, GetParam()));
    const auto zero = fdk.reconstruct(VolumeF32(5, 3, 7), geometry);
    for (const float value : zero.elements()) EXPECT_EQ(value, 0.0f);

    // Change both geometry and buffer shapes before returning to the first case.
    const Geometry other(vec2i(2, 4), vec2f(0.7f, 1.4f), vec3i(3, 3, 5), 80.0f, 300.0f);
    const auto otherInput = asymmetricInput(2, 4, 3);
    expectVolumeNear(fdk.reconstruct(otherInput, other), reference::reconstruct(otherInput, other, GetParam()));
    expectVolumeNear(fdk.reconstruct(input, geometry), first);
}

INSTANTIATE_TEST_SUITE_P(Filters, FdkContracts, ::testing::Values(RampFilter::RamLak, RampFilter::SheppLogan),
                         [](const auto &info) { return info.param == RampFilter::RamLak ? "RamLak" : "SheppLogan"; });

}  // namespace
}  // namespace libcbct::tests
