#include <gtest/gtest.h>

#include <complex>
#include <optional>

#include "Phantom.h"
#include "Reconstruction/FeldkampCPU.h"
#include "Reconstruction/FeldkampVulkan.h"
#include "Utils/SlangSpirvKernel.h"
#include "Utils/VulkanFft.h"

#if defined(LIBCBCT_WITH_CUDA)
#include <cuda_runtime.h>
#include "Reconstruction/FeldkampCUDA.h"
#endif

namespace libcbct::tests {

using namespace testing_utils;

namespace {

class VulkanKernelTest : public ::testing::Test {
protected:
    std::optional<VulkanComputeContext> context;
    void SetUp() override {
        try {
            context.emplace();
        } catch (const VulkanUnavailable &error) {
            GTEST_SKIP() << error.what();
        }
    }
};

TEST_F(VulkanKernelTest, BatchedFftMatchesDirectDftAndUnnormalizedInverse) {
    for (const int length : { 2, 16, 64, 256 }) {
        SCOPED_TRACE(length);
        constexpr int rows = 3;
        std::vector<float> values(length * rows);
        for (std::size_t i = 0; i < values.size(); ++i) values[i] = (static_cast<int>(i % 17) - 8) * 0.025f;
        const auto realBytes = values.size() * sizeof(float), complexBytes = realBytes * 2;
        const auto upload = context->createBuffer(realBytes, VulkanBufferMemory::Upload);
        const auto input = context->createBuffer(realBytes);
        const auto spectrum = context->createBuffer(complexBytes);
        const auto inverse = context->createBuffer(complexBytes);
        const auto forwardReadback = context->createBuffer(complexBytes, VulkanBufferMemory::Readback);
        const auto inverseReadback = context->createBuffer(complexBytes, VulkanBufferMemory::Readback);
        upload.write(std::as_bytes(std::span(values)));
        VulkanFftPlan fft(*context, LIBCBCT_SLANG_KERNEL_DIR, length, rows);
        auto batch = context->createBatch();
        batch.copy(upload, input, realBytes);
        fft.forward(batch, input, spectrum);
        batch.copy(spectrum, forwardReadback, complexBytes);
        fft.inverse(batch, spectrum, inverse);
        batch.copy(inverse, inverseReadback, complexBytes);
        batch.submitAndWait();
        std::vector<std::complex<float>> actual(values.size()), recovered(values.size());
        static_assert(sizeof(std::complex<float>) == 2 * sizeof(float));
        forwardReadback.read(std::as_writable_bytes(std::span(actual)));
        inverseReadback.read(std::as_writable_bytes(std::span(recovered)));
        for (int row = 0; row < rows; ++row) {
            for (int k = 0; k < length; ++k) {
                std::complex<double> expected{};
                for (int j = 0; j < length; ++j) {
                    const double theta = -static_cast<double>(libcbct::kTwoPi) * k * j / length;
                    expected += static_cast<double>(values[row * length + j])
                                * std::complex<double>(std::cos(theta), std::sin(theta));
                }
                EXPECT_NEAR(actual[row * length + k].real(), expected.real(), 1e-5);
                EXPECT_NEAR(actual[row * length + k].imag(), expected.imag(), 1e-5);
                EXPECT_NEAR(recovered[row * length + k].real() / length, values[row * length + k], 1e-6);
                EXPECT_NEAR(recovered[row * length + k].imag() / length, 0.0f, 1e-6);
            }
        }
    }
}

TEST_F(VulkanKernelTest, ReflectedBindingsRejectUnknownOrUnassignedArguments) {
    SlangSpirvKernel kernel(*context, LIBCBCT_SLANG_KERNEL_DIR, "weightAndPadKernel");
    const auto buffer = context->createBuffer(16);
    EXPECT_THROW(kernel.setBuffer("missing", buffer), InvalidArgument);
    EXPECT_THROW(kernel.setUniform("detWidth", 1.0), InvalidArgument);
    auto batch = context->createBatch();
    EXPECT_THROW(kernel.dispatch(batch, { 1, 1, 1 }), InvalidArgument);
}

class FeldkampVulkanTest : public ::testing::TestWithParam<RampFilter> {
protected:
    void SetUp() override {
        try {
            if (VulkanComputeContext::availableDevices().empty()) GTEST_SKIP() << "No Vulkan compute device";
        } catch (const VulkanUnavailable &error) {
            GTEST_SKIP() << error.what();
        }
    }
    void compare(const VolumeF32 &sinogram, const Geometry &geometry) {
        const auto cpu = FeldkampCPU(GetParam()).reconstruct(sinogram, geometry);
        const auto vulkan = FeldkampVulkan(LIBCBCT_SLANG_KERNEL_DIR, GetParam()).reconstruct(sinogram, geometry);
        EXPECT_LT(maxAbsDiff(cpu, vulkan), 1e-6);
#if defined(LIBCBCT_WITH_CUDA)
        int count = 0;
        if (cudaGetDeviceCount(&count) == cudaSuccess && count > 0) {
            const auto cuda = FeldkampCUDA(GetParam()).reconstruct(sinogram, geometry);
            EXPECT_LT(maxAbsDiff(cuda, vulkan), 1e-6);
        }
#endif
    }
};

TEST_P(FeldkampVulkanTest, MatchesCpuForSpherePhantom) {
    const PhantomSetup setup;
    compare(makeSphereSinogram(setup, { { 2.0, 1.0, 0.0, 6.0, 0.02 }, { 0.0, 0.0, 7.0, 3.0, 0.02 } }),
            setup.geometry());
}

TEST_P(FeldkampVulkanTest, MatchesCpuForPartialTilesAndRectangularDetector) {
    VolumeF32 sinogram(17, 19, 7);
    for (int p = 0; p < 7; ++p) {
        for (int y = 0; y < 19; ++y) {
            for (int x = 0; x < 17; ++x) sinogram(x, y, p) = 0.01f * (1 + (x + 3 * y + p) % 13);
        }
    }
    compare(sinogram, Geometry(vec2i(17, 19), vec2f(0.8f, 1.1f), vec3i(17, 19, 5), 100.0f, 400.0f));
}

INSTANTIATE_TEST_SUITE_P(Filters, FeldkampVulkanTest, ::testing::Values(RampFilter::RamLak, RampFilter::SheppLogan),
                         [](const auto &info) { return info.param == RampFilter::RamLak ? "RamLak" : "SheppLogan"; });

TEST(FeldkampVulkanInput, RejectsInvalidInputBeforeLoadingSpirvOrInitializingVulkan) {
    const FeldkampVulkan fdk("missing-kernels");
    const PhantomSetup setup;
    EXPECT_THROW(fdk.reconstruct(VolumeF32(), setup.geometry()), InvalidArgument);
    EXPECT_THROW(fdk.reconstruct(VolumeF32(1, 2, 1), setup.geometry()), InvalidArgument);
    auto geometry = setup.geometry();
    geometry.sod = 0.0f;
    EXPECT_THROW(fdk.reconstruct(VolumeF32(2, 2, 1), geometry), InvalidArgument);
}

}  // namespace

}  // namespace libcbct::tests
