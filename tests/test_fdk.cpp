#include <gtest/gtest.h>

#include "Phantom.h"
#include "Reconstruction/FeldkampCpu.h"
#include "Reconstruction/Feldkamp.h"

#if defined(LIBCBCT_WITH_CUDA)
#include <cuda_runtime.h>
#include "Reconstruction/FeldkampCuda.h"
#endif

namespace libcbct::tests {

using namespace testing_utils;

namespace {

const std::vector<Sphere> kSpheres = {
    { 2.0, 1.0, 0.0, 6.0, 0.02 },  // near the central plane
    { 0.0, 0.0, 7.0, 3.0, 0.02 },  // off the central plane (cone-beam approximation error)
};

class FeldkampCpuTest : public ::testing::TestWithParam<RampFilter> {};

}  // namespace

// Quantitative check: reconstructed attenuation inside each sphere matches the ground truth
TEST_P(FeldkampCpuTest, ReconstructsSphereAttenuation) {
    const PhantomSetup setup;
    const VolumeF32 sino = makeSphereSinogram(setup, kSpheres);
    const VolumeF32 vol = FeldkampCpu(GetParam()).reconstruct(sino, setup.geometry());

    EXPECT_NEAR(meanInsideSphere(vol, setup, kSpheres[0]), 0.02, 0.02 * 0.01);  // 1% on the central plane
    EXPECT_NEAR(meanInsideSphere(vol, setup, kSpheres[1]), 0.02, 0.02 * 0.05);  // 5% off the central plane
}

TEST_P(FeldkampCpuTest, OperatorRecipeMatchesLegacySphereReconstruction) {
    const PhantomSetup setup;
    const auto sino = makeSphereSinogram(setup, kSpheres);
    const auto legacy = FeldkampCpu(GetParam()).reconstruct(sino, setup.geometry());
    const auto actual = Feldkamp(Backend::Cpu, GetParam()).reconstruct(sino, setup.geometry());
    ASSERT_EQ(actual.numel(), legacy.numel());
    double maximum = 0.0;
    for (std::size_t i = 0; i < actual.numel(); ++i) {
        ASSERT_TRUE(std::isfinite(actual.ptr()[i]));
        maximum = std::max(maximum, std::abs(static_cast<double>(actual.ptr()[i]) - legacy.ptr()[i]));
    }
    EXPECT_LT(maximum, 1e-6);
    EXPECT_NEAR(meanInsideSphere(actual, setup, kSpheres[0]), 0.02, 0.02 * 0.01);
    EXPECT_NEAR(meanInsideSphere(actual, setup, kSpheres[1]), 0.02, 0.02 * 0.05);
}

INSTANTIATE_TEST_SUITE_P(Filters, FeldkampCpuTest, ::testing::Values(RampFilter::RamLak, RampFilter::SheppLogan),
                         [](const auto &info) { return info.param == RampFilter::RamLak ? "RamLak" : "SheppLogan"; });

#if defined(LIBCBCT_WITH_CUDA)
// Built with CUDA, but the machine may still have no GPU (e.g. CI runners): skip instead of failing
TEST(FeldkampCuda, DefaultKernelDirectoryMatchesCPU) {
    int deviceCount = 0;
    if (cudaGetDeviceCount(&deviceCount) != cudaSuccess || deviceCount == 0) {
        GTEST_SKIP() << "No CUDA device available";
    }
    const PhantomSetup setup;
    const VolumeF32 sino = makeSphereSinogram(setup, kSpheres);
    const VolumeF32 cpu = FeldkampCpu(RampFilter::SheppLogan).reconstruct(sino, setup.geometry());
    const VolumeF32 gpu = FeldkampCuda(RampFilter::SheppLogan).reconstruct(sino, setup.geometry());

    EXPECT_LT(maxAbsDiff(cpu, gpu), 1e-6);
}
#endif

}  // namespace libcbct::tests
