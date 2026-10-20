#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <complex>
#include <limits>
#include <type_traits>

#include "FdkReference.h"
#include "Reconstruction/CpuOperators.h"
#include "Reconstruction/Feldkamp.h"

namespace libcbct::tests {
namespace {

namespace reference = testing_utils::fdk_reference;

const Geometry kGeometry(vec2i(5, 3), vec2f(0.8f, 1.1f), vec3i(5, 7, 3), 100.0f, 400.0f);

template <typename T, std::size_t Rank>
void fill(TensorView<T, Rank> view, T value) {
    std::fill(view.hostElements().begin(), view.hostElements().end(), value);
}

TEST(CpuExecution, ShapeTypeSlicesAndOwnershipAreExplicit) {
    auto view = [] {
        CpuExecutionSession session;
        auto owned = session.allocate<float>(std::array<std::size_t, 3>{ 3, 2, 4 });
        owned.hostElements()[12] = 7.0f;
        return owned;
    }();
    EXPECT_EQ(view.backend(), Backend::Cpu);
    EXPECT_EQ(view.layout(), BufferLayout::XContiguous);
    EXPECT_EQ(view.scalar(), BufferScalar::Float32);
    EXPECT_EQ(view.slice(2).hostElements()[0], 7.0f);
    EXPECT_EQ(view.slice(2).shape(), (std::array<std::size_t, 2>{ 3, 2 }));
    EXPECT_THROW(view.slice(4), InvalidArgument);
    ConstVolumeView readOnly = view;
    static_assert(std::is_same_v<decltype(readOnly.hostElements()), std::span<const float>>);
    EXPECT_EQ(readOnly.context(), view.context());
}

TEST(CpuExecution, BorrowDoesNotCopyAndRejectsInconsistentOrOverflowingShapes) {
    CpuExecutionSession session;
    VolumeF32 volume(3, 2, 1);
    const auto view = session.makeView(volume);
    EXPECT_EQ(view.hostElements().data(), volume.ptr());
    EXPECT_EQ(session.statistics().bufferAllocations, 0u);
    EXPECT_EQ(session.statistics().transferBytes, 0u);
    EXPECT_THROW(session.makeView(volume.elements(), std::array<std::size_t, 2>{ 2, 2 }), InvalidArgument);
    EXPECT_THROW(session.allocate<float>(std::array<std::size_t, 2>{ 0, 2 }), InvalidArgument);
    EXPECT_THROW(session.allocate<float>(std::array{ std::numeric_limits<std::size_t>::max(), std::size_t{ 2 } }),
                 InvalidArgument);
}

TEST(CpuWeightAndPad, OverwritesPixelsAndPaddingAndReadsUpdatedInput) {
    CpuExecutionSession session;
    CpuWeightAndPad weight(session, kGeometry);
    VolumeF32 input(5, 3, 1);
    for (std::size_t i = 0; i < input.numel(); ++i) {
        input.ptr()[i] = 0.1f * (i + 1);
    }
    auto padded = session.allocate<float>(std::array{ weight.paddedWidth(), std::size_t{ 3 } });
    fill(padded, -999.0f);
    weight.apply(session.makeView(input).slice(0), padded);
    for (std::size_t y = 0; y < 3; ++y) {
        for (std::size_t x = 0; x < weight.paddedWidth(); ++x) {
            const double expected = x < 5 ? input.ptr()[y * 5 + x] * reference::cosineWeight(kGeometry, x, y) : 0;
            EXPECT_NEAR(padded.hostElements()[y * weight.paddedWidth() + x], expected, 2e-7);
        }
    }
    std::fill(input.elements().begin(), input.elements().end(), 0.0f);
    weight.apply(session.makeView(input).slice(0), padded);
    for (float value : padded.hostElements()) {
        EXPECT_EQ(value, 0.0f);
    }
}

TEST(CpuWeightAndPad, RejectsShapeContextAndPartialOverlapBeforeWriting) {
    CpuExecutionSession session, other;
    CpuWeightAndPad weight(session, kGeometry);
    auto padded = session.allocate<float>(std::array<std::size_t, 2>{ 16, 3 });
    fill(padded, -9.0f);
    auto wrongShape = session.allocate<float>(std::array<std::size_t, 2>{ 3, 5 });
    auto wrongContext = other.allocate<float>(std::array<std::size_t, 2>{ 5, 3 });
    auto overlap = session.makeView(padded.hostElements().subspan(1, 15), std::array<std::size_t, 2>{ 5, 3 });
    EXPECT_THROW(weight.apply(wrongShape, padded), InvalidArgument);
    EXPECT_THROW(weight.apply(wrongContext, padded), InvalidArgument);
    EXPECT_THROW(weight.apply(overlap, padded), InvalidArgument);
    for (float value : padded.hostElements()) {
        EXPECT_EQ(value, -9.0f);
    }
}

TEST(CpuRealFft, MatchesDirectDftAndNormalizedRoundTripAcrossRows) {
    CpuExecutionSession session;
    for (const std::size_t length : { 2u, 8u, 64u }) {
        SCOPED_TRACE(length);
        // Includes both SIMD groups and a scalar tail on SSE/AVX/AVX-512.
        constexpr std::size_t rows = 19;
        CpuRealFft fft(session, length, rows);
        auto input = session.allocate<float>(std::array{ length, rows });
        auto spectrum = session.allocate<std::complex<float>>(std::array{ length / 2 + 1, rows });
        auto output = session.allocate<float>(std::array{ length, rows });
        for (std::size_t i = 0; i < input.hostElements().size(); ++i) {
            input.hostElements()[i] = 0.025f * (static_cast<int>((i * 7) % 19) - 9);
        }
        const std::vector<float> original(input.hostElements().begin(), input.hostElements().end());
        fft.forward(input, spectrum);
        for (std::size_t y = 0; y < rows; ++y) {
            for (std::size_t k = 0; k <= length / 2; ++k) {
                std::complex<double> expected{};
                for (std::size_t j = 0; j < length; ++j) {
                    const double angle = -static_cast<double>(kTwoPi) * j * k / length;
                    expected += static_cast<double>(original[y * length + j])
                                * std::complex<double>(std::cos(angle), std::sin(angle));
                }
                const auto actual = spectrum.hostElements()[y * (length / 2 + 1) + k];
                EXPECT_NEAR(actual.real(), expected.real(), 2e-6);
                EXPECT_NEAR(actual.imag(), expected.imag(), 2e-6);
            }
        }
        const std::vector<std::complex<float>> originalSpectrum(spectrum.hostElements().begin(),
                                                                spectrum.hostElements().end());
        fft.inverse(spectrum, output);
        for (std::size_t i = 0; i < original.size(); ++i) {
            EXPECT_NEAR(output.hostElements()[i], original[i], 1e-6);
            EXPECT_EQ(input.hostElements()[i], original[i]);
        }
        EXPECT_TRUE(std::equal(originalSpectrum.begin(), originalSpectrum.end(), spectrum.hostElements().begin()));
    }
}

TEST(CpuRealFft, RejectsInvalidPlansAndBuffersBeforeWriting) {
    CpuExecutionSession session, other;
    EXPECT_THROW(CpuRealFft(session, 0, 2), InvalidArgument);
    EXPECT_THROW(CpuRealFft(session, 3, 2), InvalidArgument);
    EXPECT_THROW(CpuRealFft(session, 8, 0), InvalidArgument);
    EXPECT_THROW(CpuRealFft(session, 8, std::numeric_limits<std::size_t>::max()), InvalidArgument);
    CpuRealFft fft(session, 8, 2);
    auto input = session.allocate<float>(std::array<std::size_t, 2>{ 8, 2 });
    auto output = session.allocate<std::complex<float>>(std::array<std::size_t, 2>{ 5, 2 });
    auto foreign = other.allocate<float>(std::array<std::size_t, 2>{ 8, 2 });
    auto wrong = session.allocate<std::complex<float>>(std::array<std::size_t, 2>{ 2, 5 });
    fill(output, std::complex<float>(7, 9));
    fill(input, 8.0f);
    EXPECT_THROW(fft.forward(foreign, output), InvalidArgument);
    EXPECT_THROW(fft.forward(input, wrong), InvalidArgument);
    EXPECT_THROW(fft.inverse(wrong, input), InvalidArgument);
    for (auto value : output.hostElements()) {
        EXPECT_EQ(value, std::complex<float>(7, 9));
    }
    for (auto value : input.hostElements()) {
        EXPECT_EQ(value, 8.0f);
    }
}

TEST(CpuSpectralMultiply, MultipliesBothComponentsAndOwnsImmutableResponse) {
    CpuExecutionSession session;
    std::vector<float> response{ 0.0f, -2.0f, 3.0f };
    CpuSpectralMultiply multiply(session, response);
    std::fill(response.begin(), response.end(), 99.0f);
    auto spectrum = session.allocate<std::complex<float>>(std::array<std::size_t, 2>{ 3, 2 });
    fill(spectrum, std::complex<float>(1, -4));
    multiply.apply(spectrum);
    for (std::size_t y = 0; y < 2; ++y) {
        EXPECT_EQ(spectrum.hostElements()[y * 3], std::complex<float>(0, 0));
        EXPECT_EQ(spectrum.hostElements()[y * 3 + 1], std::complex<float>(-2, 8));
        EXPECT_EQ(spectrum.hostElements()[y * 3 + 2], std::complex<float>(3, -12));
    }
    const std::array<float, 1> invalid{ std::numeric_limits<float>::infinity() };
    EXPECT_THROW(CpuSpectralMultiply(session, invalid), InvalidArgument);
}

TEST(CpuCrop, CopiesLeftColumnsAndRejectsAliasingBeforeWriting) {
    CpuExecutionSession session;
    CpuCrop crop(session);
    auto input = session.allocate<float>(std::array<std::size_t, 2>{ 8, 3 });
    auto output = session.allocate<float>(std::array<std::size_t, 2>{ 3, 3 });
    for (std::size_t i = 0; i < input.hostElements().size(); ++i) {
        input.hostElements()[i] = i;
    }
    crop.apply(input, output);
    for (std::size_t y = 0; y < 3; ++y) {
        for (std::size_t x = 0; x < 3; ++x) {
            EXPECT_EQ(output.hostElements()[y * 3 + x], y * 8 + x);
        }
    }
    auto alias = session.makeView(input.hostElements().subspan(1, 9), std::array<std::size_t, 2>{ 3, 3 });
    EXPECT_THROW(crop.apply(input, alias), InvalidArgument);
    for (std::size_t i = 0; i < input.hostElements().size(); ++i) {
        EXPECT_EQ(input.hostElements()[i], i);
    }
}

class CpuProjectionFilterTest : public ::testing::TestWithParam<RampFilter> {};

TEST_P(CpuProjectionFilterTest, MatchesDirectConvolutionAndReusesBuffers) {
    CpuExecutionSession session;
    CpuWeightAndPad weight(session, kGeometry);
    CpuProjectionFilter filter(session, kGeometry, GetParam());
    auto padded = session.allocate<float>(std::array{ weight.paddedWidth(), std::size_t{ 3 } });
    auto filtered = session.allocate<float>(std::array<std::size_t, 2>{ 5, 3 });
    VolumeF32 input(5, 3, 1);
    const auto before = session.statistics();
    const auto *paddedAddress = padded.hostElements().data();
    for (int iteration = 0; iteration < 3; ++iteration) {
        for (std::size_t i = 0; i < input.numel(); ++i) {
            input.ptr()[i] = iteration == 2 ? 0.0f : 0.01f * (static_cast<int>((i * 3 + iteration) % 11) - 5);
        }
        weight.apply(session.makeView(input).slice(0), padded);
        filter.apply(padded, filtered);
        const auto expected = reference::filteredProjection(input, kGeometry, 0, GetParam());
        for (std::size_t i = 0; i < expected.size(); ++i) {
            EXPECT_NEAR(filtered.hostElements()[i], expected[i], 1e-6);
        }
    }
    EXPECT_EQ(paddedAddress, padded.hostElements().data());
    EXPECT_EQ(session.statistics().bufferAllocations, before.bufferAllocations);
    EXPECT_EQ(session.statistics().bufferBytes, before.bufferBytes);
    EXPECT_EQ(session.statistics().transferBytes, 0u);
}

TEST_P(CpuProjectionFilterTest, CompositeValidationPrecedesDestructiveInverse) {
    CpuExecutionSession session, other;
    CpuProjectionFilter filter(session, kGeometry, GetParam());
    auto padded = session.allocate<float>(std::array<std::size_t, 2>{ 16, 3 });
    auto foreign = other.allocate<float>(std::array<std::size_t, 2>{ 5, 3 });
    auto wrong = session.allocate<float>(std::array<std::size_t, 2>{ 3, 5 });
    auto overlap = session.makeView(padded.hostElements().first(15), std::array<std::size_t, 2>{ 5, 3 });
    fill(padded, 4.0f);
    fill(foreign, 8.0f);
    fill(wrong, 9.0f);
    EXPECT_THROW(filter.apply(padded, foreign), InvalidArgument);
    EXPECT_THROW(filter.apply(padded, wrong), InvalidArgument);
    EXPECT_THROW(filter.apply(padded, overlap), InvalidArgument);
    for (float value : padded.hostElements()) {
        EXPECT_EQ(value, 4.0f);
    }
    for (float value : foreign.hostElements()) {
        EXPECT_EQ(value, 8.0f);
    }
    for (float value : wrong.hostElements()) {
        EXPECT_EQ(value, 9.0f);
    }
}

INSTANTIATE_TEST_SUITE_P(Filters, CpuProjectionFilterTest,
                         ::testing::Values(RampFilter::RamLak, RampFilter::SheppLogan));

TEST(CpuFdkBackprojection, AddsToNonzeroVolumeWithExplicitScaleAndPreservesInput) {
    CpuExecutionSession session;
    const Geometry geometry(vec2i(2, 2), vec2f(1.0f, 1.0f), vec3i(1, 1, 1), 100.0f, 400.0f);
    CpuFdkBackprojection back(session, geometry);
    auto projection = session.allocate<float>(std::array<std::size_t, 2>{ 2, 2 });
    auto volume = session.allocate<float>(std::array<std::size_t, 3>{ 1, 1, 1 });
    fill(projection, 2.0f);
    fill(volume, 3.0f);
    back.accumulate(projection, volume, 0.0f, 0.25f);
    EXPECT_FLOAT_EQ(volume.hostElements()[0], 3.5f);
    back.accumulate(projection, volume, 0.7f, 0.25f);
    EXPECT_FLOAT_EQ(volume.hostElements()[0], 4.0f);
    back.accumulate(projection, volume, 0.7f, -0.5f);
    EXPECT_FLOAT_EQ(volume.hostElements()[0], 3.0f);
    back.accumulate(projection, volume, 0.7f, 0.0f);
    EXPECT_FLOAT_EQ(volume.hostElements()[0], 3.0f);
    for (float value : projection.hostElements()) {
        EXPECT_EQ(value, 2.0f);
    }
}

TEST(CpuFdkBackprojection, MatchesIndependentGeometryIncludingDetectorEdges) {
    for (const auto geometry :
         { kGeometry, Geometry(vec2i(2, 2), vec2f(1.0f, 2.0f), vec3i(1, 1, 5), 100.0f, 400.0f) }) {
        CpuExecutionSession session;
        CpuFdkBackprojection back(session, geometry);
        const auto w = static_cast<std::size_t>(geometry.detSize.x), h = static_cast<std::size_t>(geometry.detSize.y);
        auto projection = session.allocate<float>(std::array{ w, h });
        std::vector<double> referenceProjection(w * h);
        for (std::size_t i = 0; i < w * h; ++i) {
            referenceProjection[i] = projection.hostElements()[i] = 0.01f * i;
        }
        VolumeF32 volume(geometry.volSize.x, geometry.volSize.y, geometry.volSize.z);
        VolumeF64 expected(geometry.volSize.x, geometry.volSize.y, geometry.volSize.z);
        for (const float angle : { 0.0f, 0.7f, 2.3f }) {
            back.accumulate(projection, session.makeView(volume), angle, 0.2f);
            reference::backprojectAdd(referenceProjection, geometry, angle, 0.2f, expected);
        }
        for (std::size_t i = 0; i < volume.numel(); ++i) {
            EXPECT_NEAR(volume.ptr()[i], expected.ptr()[i], 1e-6);
        }
    }
}

TEST(CpuFdkBackprojection, RejectsInvalidArgumentsBeforeAccumulation) {
    CpuExecutionSession session, other;
    CpuFdkBackprojection back(session, kGeometry);
    auto projection = session.allocate<float>(std::array<std::size_t, 2>{ 5, 3 });
    auto foreign = other.allocate<float>(std::array<std::size_t, 2>{ 5, 3 });
    auto wrong = session.allocate<float>(std::array<std::size_t, 2>{ 3, 5 });
    auto volume = session.allocate<float>(std::array<std::size_t, 3>{ 5, 7, 3 });
    auto overlap = session.makeView(volume.hostElements().subspan(1, 15), std::array<std::size_t, 2>{ 5, 3 });
    fill(volume, 9.0f);
    EXPECT_THROW(back.accumulate(foreign, volume, 0, 1), InvalidArgument);
    EXPECT_THROW(back.accumulate(wrong, volume, 0, 1), InvalidArgument);
    EXPECT_THROW(back.accumulate(overlap, volume, 0, 1), InvalidArgument);
    EXPECT_THROW(back.accumulate(projection, volume, std::numeric_limits<float>::quiet_NaN(), 1), InvalidArgument);
    EXPECT_THROW(back.accumulate(projection, volume, 0, std::numeric_limits<float>::infinity()), InvalidArgument);
    for (float value : volume.hostElements()) {
        EXPECT_EQ(value, 9.0f);
    }
}

TEST(FeldkampInput, RejectsInvalidShapesGeometryFiltersAndUnavailableOperators) {
    const Feldkamp fdk;
    EXPECT_THROW(fdk.reconstruct(VolumeF32(), kGeometry), InvalidArgument);
    EXPECT_THROW(fdk.reconstruct(VolumeF32(3, 5, 1), kGeometry), InvalidArgument);
    EXPECT_THROW(fdk.reconstruct(VolumeF32(5, 3, 0), kGeometry), InvalidArgument);
    for (const float invalid :
         { 0.0f, -1.0f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN() }) {
        auto geometry = kGeometry;
        geometry.sod = invalid;
        EXPECT_THROW(fdk.reconstruct(VolumeF32(5, 3, 1), geometry), InvalidArgument);
    }
    auto geometry = kGeometry;
    geometry.volSize.x = -1;
    EXPECT_THROW(fdk.reconstruct(VolumeF32(5, 3, 1), geometry), InvalidArgument);
    geometry = kGeometry;
    geometry.detSize = vec2i(std::numeric_limits<int>::max(), 2);
    EXPECT_THROW(fdk.reconstruct(VolumeF32(), geometry), InvalidArgument);
    EXPECT_THROW(Feldkamp(Backend::Cpu, static_cast<RampFilter>(99)), InvalidArgument);
    EXPECT_THROW(Feldkamp(static_cast<Backend>(99)), InvalidArgument);
    EXPECT_THROW((void)Feldkamp{ Backend::Cuda }, InvalidArgument);
    EXPECT_THROW((void)Feldkamp{ Backend::Vulkan }, InvalidArgument);
    FeldkampOptions options;
    options.filterBackend = Backend::Cuda;
    EXPECT_THROW(Feldkamp{ options }, InvalidArgument);
}

TEST(FeldkampInput, OperatorOverridesTakePrecedenceOverDefaultBackend) {
    FeldkampOptions options;
    options.backend = Backend::Vulkan;
    options.weightBackend = options.filterBackend = options.backprojectionBackend = Backend::Cpu;
    const auto volume = Feldkamp(options).reconstruct(VolumeF32(5, 3, 1), kGeometry);
    for (float value : volume.elements()) {
        EXPECT_EQ(value, 0.0f);
    }
}

}  // namespace
}  // namespace libcbct::tests
