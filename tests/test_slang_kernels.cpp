#include <gtest/gtest.h>

#include <algorithm>
#include <complex>
#include <vector>

#include "Common/Constants.h"
#include "Geometry/GeometryBase.h"
#include "Reconstruction/FilterUtils.h"
#include "Utils/ImageUtils.h"

// Execute code compiled from the actual Slang source, including its parameter layouts.
// A separate prelude namespace also permits builds with CUDA's native vector types.
#define SLANG_PRELUDE_NAMESPACE libcbct_slang_cpu
#include "FeldkampKernels.cpu.cpp"

namespace {

namespace generated = libcbct_slang_cpu;

constexpr float kGuard = -12345.0f;
constexpr size_t kGuardCount = 32;

template <typename Parameters>
void dispatch(generated::ComputeFunc kernel, Parameters &params, int width, int height, int depth = 1) {
    generated::ComputeVaryingInput groups{};
    groups.endGroupID = generated::uint3((width + 15) / 16, (height + 15) / 16, depth);
    kernel(&groups, &params, nullptr);
}

void checkWeightAndPad(int width, int height) {
    const int nPad = static_cast<int>(paddedFFTLength(width));
    const size_t nPixels = static_cast<size_t>(width) * height;
    const size_t nPadded = static_cast<size_t>(nPad) * height;
    const Geometry geometry(vec2i(width, height), vec2f(0.3f, 0.6f), vec3i(17, 19, 3), 100.0f, 400.0f);
    std::vector<float> projection(nPixels);
    auto weights = makeCosineWeights(geometry, width, height);
    std::vector<float> padded(nPadded + kGuardCount, kGuard);
    for (size_t i = 0; i < nPixels; ++i) {
        projection[i] = (static_cast<int>(i % 13) - 6) * 0.125f;
    }

    EntryPointParams_0 params{{{projection.data(), projection.size()}, {weights.data(), weights.size()},
                             {padded.data(), padded.size()}, width, height, nPad}};
    dispatch(weightAndPadKernel, params, nPad, height);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < nPad; ++x) {
            const float expected = x < width ? projection[y * width + x] * weights[y * width + x] : 0.0f;
            EXPECT_FLOAT_EQ(padded[y * nPad + x], expected) << "x=" << x << " y=" << y;
        }
    }
    for (size_t i = nPadded; i < padded.size(); ++i) {
        EXPECT_FLOAT_EQ(padded[i], kGuard);
    }
}

}  // namespace

TEST(SlangKernels, WeightsAndPadsPartialDetectorTiles) {
    checkWeightAndPad(17, 19);
}

TEST(SlangKernels, GuardsPartialPaddingTiles) {
    checkWeightAndPad(3, 2);
}

TEST(SlangKernels, FiltersBothComplexComponentsWithoutExtraNormalization) {
    constexpr int nPad = 32;
    constexpr int nFreq = nPad / 2 + 1;
    constexpr int height = 5;
    constexpr size_t count = nFreq * height;
    auto response = makeRampResponse(nPad, 0.25, RampFilter::SheppLogan);
    for (float &value : response) {
        value /= nPad;
    }
    std::vector<generated::float2> spectrum(count + kGuardCount, generated::float2(kGuard, kGuard));
    std::vector<std::complex<float>> expected(count);
    for (int y = 0; y < height; ++y) {
        for (int k = 0; k < nFreq; ++k) {
            const int index = y * nFreq + k;
            expected[index] = std::complex<float>(k - 0.3f * y, 0.2f * k - 4.0f * y);
            spectrum[index] = generated::float2(expected[index].real(), expected[index].imag());
            expected[index] *= response[k];
        }
    }

    EntryPointParams_1 params{{{spectrum.data(), spectrum.size()}, {response.data(), response.size()},
                             nFreq, height}};
    dispatch(applyFilterKernel, params, nFreq, height);
    for (size_t i = 0; i < count; ++i) {
        EXPECT_FLOAT_EQ(spectrum[i].x, expected[i].real());
        EXPECT_FLOAT_EQ(spectrum[i].y, expected[i].imag());
    }
    for (size_t i = count; i < spectrum.size(); ++i) {
        EXPECT_FLOAT_EQ(spectrum[i].x, kGuard);
        EXPECT_FLOAT_EQ(spectrum[i].y, kGuard);
    }
}

TEST(SlangKernels, BackProjectionMatchesCPUAtBordersAndAcrossProjections) {
    // Keep geometry dimensions distinct from buffer dimensions, as in the CUDA signature.
    constexpr int width = 19;
    constexpr int height = 11;
    const Geometry geometry(vec2i(21, 13), vec2f(0.8f, 1.1f), vec3i(17, 19, 19), 100.0f, 400.0f);
    const vec3i size = geometry.volSize;
    const size_t count = static_cast<size_t>(size.x) * size.y * size.z;
    std::vector<float> filtered(width * height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            filtered[y * width + x] = (x * x + 2.0f * y + 0.125f * x * y) * 0.1f;
        }
    }
    std::vector<float> volume(count + kGuardCount, kGuard);
    std::fill_n(volume.begin(), count, 0.25f);
    std::vector<float> expected(count, 0.25f);
    const FdkGeometry_0 slangGeometry{generated::int2(geometry.detSize.x, geometry.detSize.y),
                                    generated::float2(geometry.pixSize.x, geometry.pixSize.y),
                                    generated::int3(size.x, size.y, size.z), geometry.sod, geometry.sdd};
    size_t outside = 0;
    size_t horizontalBorder = 0;
    size_t verticalBorder = 0;
    for (float theta : {0.0f, 0.7f, 0.5f * static_cast<float>(libcbct::kPi)}) {
        const float scale = static_cast<float>(libcbct::kPi) / 7.0f;
        EntryPointParams_2 params{{{filtered.data(), filtered.size()}, {volume.data(), volume.size()},
                                 width, height, slangGeometry, theta, scale}};
        // An extra z group checks the volume-depth guard as well as partial x/y tiles.
        dispatch(backProjectionKernel, params, size.x, size.y, size.z + 1);
        for (int z = 0; z < size.z; ++z) {
            for (int y = 0; y < size.y; ++y) {
                for (int x = 0; x < size.x; ++x) {
                    const size_t index = (static_cast<size_t>(z) * size.y + y) * size.x + x;
                    const vec3f uvw = vox2pix(vec3i(x, y, z), theta, geometry);
                    if (uvw.x >= 0.0f && uvw.y >= 0.0f && uvw.x < width && uvw.y < height) {
                        horizontalBorder += uvw.x < 0.5f || uvw.x > width - 0.5f;
                        verticalBorder += uvw.y < 0.5f || uvw.y > height - 0.5f;
                        expected[index] += bilerp(filtered.data(), width, height, uvw.x - 0.5f, uvw.y - 0.5f) *
                                           uvw.z * scale;
                    } else {
                        ++outside;
                    }
                    EXPECT_NEAR(volume[index], expected[index], 2e-5f * std::max(1.0f, std::abs(expected[index])))
                        << "x=" << x << " y=" << y << " z=" << z << " theta=" << theta;
                }
            }
        }
        for (size_t i = count; i < volume.size(); ++i) {
            EXPECT_FLOAT_EQ(volume[i], kGuard);
        }
    }
    EXPECT_GT(outside, 0u);
    EXPECT_GT(horizontalBorder, 0u);
    EXPECT_GT(verticalBorder, 0u);
}
