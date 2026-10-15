#include <gtest/gtest.h>

#include <cuda_runtime.h>

#include "Phantom.h"
#include "Reconstruction/FeldkampCPU.h"
#include "Reconstruction/FeldkampCUDA.h"
#include "Reconstruction/FeldkampSlangCUDA.h"

namespace libcbct::tests {

using namespace testing_utils;

namespace {

class FeldkampCUDATest : public ::testing::TestWithParam<RampFilter> {
protected:
    void SetUp() override {
        int count = 0;
        if (cudaGetDeviceCount(&count) != cudaSuccess || count == 0) {
            GTEST_SKIP() << "No CUDA device available";
        }
        CUDA_CHECK(cudaSetDevice(0));
    }

    void compare(const VolumeF32 &sinogram, const Geometry &geometry) {
        const auto cpu = FeldkampCPU(GetParam()).reconstruct(sinogram, geometry);
        const auto cuda = FeldkampCUDA(LIBCBCT_SLANG_KERNEL_DIR, GetParam()).reconstruct(sinogram, geometry);
        EXPECT_LT(maxAbsDiff(cpu, cuda), 1e-6);
    }
};

TEST_P(FeldkampCUDATest, MatchesCpuForSpherePhantom) {
    const PhantomSetup setup;
    const std::vector<Sphere> spheres = { { 2.0, 1.0, 0.0, 6.0, 0.02 }, { 0.0, 0.0, 7.0, 3.0, 0.02 } };
    compare(makeSphereSinogram(setup, spheres), setup.geometry());
}

TEST_P(FeldkampCUDATest, MatchesCpuForPartialTilesAndRectangularDetector) {
    VolumeF32 sinogram(17, 19, 7);
    for (int p = 0; p < 7; ++p) {
        for (int y = 0; y < 19; ++y) {
            for (int x = 0; x < 17; ++x) sinogram(x, y, p) = 0.01f * (1 + (x + 3 * y + p) % 13);
        }
    }
    compare(sinogram, Geometry(vec2i(17, 19), vec2f(0.8f, 1.1f), vec3i(17, 19, 5), 100.0f, 400.0f));
}

INSTANTIATE_TEST_SUITE_P(Filters, FeldkampCUDATest, ::testing::Values(RampFilter::RamLak, RampFilter::SheppLogan),
                         [](const auto &info) { return info.param == RampFilter::RamLak ? "RamLak" : "SheppLogan"; });

// The former class name remains usable with an explicit PTX directory.
TEST(FeldkampCUDAInput, LegacyClassRejectsInvalidInputBeforeLoadingPtxOrInitializingCuda) {
    const FeldkampSlangCUDA fdk("missing-kernels");
    const PhantomSetup setup;
    EXPECT_THROW(fdk.reconstruct(VolumeF32(), setup.geometry()), libcbct::InvalidArgument);
    EXPECT_THROW(fdk.reconstruct(VolumeF32(1, 2, 1), setup.geometry()), libcbct::InvalidArgument);
    Geometry invalid = setup.geometry();
    invalid.sod = 0.0f;
    EXPECT_THROW(fdk.reconstruct(VolumeF32(2, 2, 1), invalid), libcbct::InvalidArgument);
}

}  // namespace

}  // namespace libcbct::tests
