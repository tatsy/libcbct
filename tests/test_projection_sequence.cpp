#include <gtest/gtest.h>

#include <format>
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>

#include "IO/ImageSequenceImporter.h"
#include "Phantom.h"
#include "Reconstruction/FeldkampCpu.h"
#include "TestDirectory.h"

#if defined(LIBCBCT_WITH_CUDA)
#include <cuda_runtime.h>
#include "Reconstruction/FeldkampCuda.h"
#endif
#if defined(LIBCBCT_WITH_VULKAN)
#include "Reconstruction/FeldkampVulkan.h"
#include "Utils/VulkanCompute.h"
#endif

namespace libcbct::tests {
namespace {

void writeIndexedImages(const std::filesystem::path &folder, int count) {
    for (int i = 0; i < count; ++i) {
        const auto path = folder / std::format("proj_{:04d}.tif", i);
        ASSERT_TRUE(cv::imwrite(path.string(), cv::Mat(2, 3, CV_16UC1, cv::Scalar(i + 100))));
    }
}

TEST(ProjectionSequence, Reads180FramesInEitherDirectionWithoutMovingAngleZero) {
    testing_utils::TestDirectory directory;
    writeIndexedImages(directory.path, 180);
    for (bool clockwise : { false, true }) {
        const ProjectionSequenceOptions options{ 180, clockwise, false };
        const auto volume = ImageSequenceImporter(directory.path.string(), ".tif", options).read();
        ASSERT_EQ(volume.size<2>(), 180);
        for (int p = 0; p < 180; ++p) {
            const int source = clockwise && p != 0 ? 180 - p : p;
            EXPECT_EQ(volume(0, 0, p), source + 100);
        }
    }
    // The generic image reader's existing full-reversal API remains available.
    const auto reversed = ImageSequenceImporter(directory.path.string(), ".tif", true).read();
    EXPECT_EQ(reversed(0, 0, 0), 279);
}

TEST(ProjectionSequence, ExcludesThe181stFrameOnlyWhenExplicitlyConfigured) {
    testing_utils::TestDirectory directory;
    writeIndexedImages(directory.path, 181);
    for (bool clockwise : { false, true }) {
        const ProjectionSequenceOptions options{ 180, clockwise, true };
        const auto volume = ImageSequenceImporter(directory.path.string(), ".tif", options).read();
        ASSERT_EQ(volume.size<2>(), 180);
        for (int p = 0; p < 180; ++p) {
            const int source = clockwise && p != 0 ? 180 - p : p;
            EXPECT_EQ(volume(0, 0, p), source + 100);
        }
    }
    EXPECT_THROW((ImageSequenceImporter(directory.path.string(), ".tif", ProjectionSequenceOptions{ 180 }).read()),
                 InvalidArgument);
}

TEST(ProjectionSequence, RejectsMissingAndExtraFramesForBothEndpointPolicies) {
    for (int count : { 179, 180, 181, 182 }) {
        testing_utils::TestDirectory directory;
        writeIndexedImages(directory.path, count);
        for (bool duplicate : { false, true }) {
            if (count == 180 + static_cast<int>(duplicate)) continue;
            SCOPED_TRACE(std::format("count={}, duplicate={}", count, duplicate));
            const ProjectionSequenceOptions options{ 180, false, duplicate };
            EXPECT_THROW(ImageSequenceImporter(directory.path.string(), ".tif", options).read(), InvalidArgument);
        }
    }
}

TEST(ProjectionSequence, RejectsNonpositiveCountsAndHandlesOneEffectiveFrame) {
    testing_utils::TestDirectory directory;
    writeIndexedImages(directory.path, 2);
    for (int count : { 0, -1 }) {
        const ProjectionSequenceOptions options{ count, true, true };
        EXPECT_THROW(ImageSequenceImporter(directory.path.string(), ".tif", options).read(), InvalidArgument);
    }
    const ProjectionSequenceOptions options{ 1, true, true };
    const auto volume = ImageSequenceImporter(directory.path.string(), ".tif", options).read();
    ASSERT_EQ(volume.size<2>(), 1);
    EXPECT_EQ(volume(0, 0, 0), 100);
}

class ProjectionPhantomTest : public ::testing::Test {
protected:
    testing_utils::TestDirectory directory;
    testing_utils::PhantomSetup setup;
    VolumeF32 transmission;

    void SetUp() override {
        setup.detSize = 32;
        setup.volSize = 24;
        const std::vector<testing_utils::Sphere> spheres = {
            { 1.0, -1.2, 0.0, 0.9, 0.02 },
            { -1.3, 0.8, 0.9, 0.55, 0.03 },
        };
        transmission = testing_utils::makeSphereSinogram(setup, spheres);
        // Simulate 16-bit transmission TIFFs with the CLI's free-ray convention.
        transmission.forEach([](float integral) { return std::round(65535.0f * std::exp(-integral) - 1.0f); });
    }

    VolumeF32 canonicalIntegrals() const {
        VolumeF32 integrals(transmission);
        integrals.forEach([](float value) { return -std::log((value + 1.0f) / 65535.0f); });
        return integrals;
    }

    VolumeF32 readAcquisition(bool clockwise, bool duplicate, const std::string &name) const {
        const auto folder = directory.path / name;
        std::filesystem::create_directory(folder);
        const int count = setup.nProj + static_cast<int>(duplicate);
        for (int p = 0; p < count; ++p) {
            const int source = clockwise ? (setup.nProj - p) % setup.nProj : p % setup.nProj;
            cv::Mat image(setup.detSize, setup.detSize, CV_16UC1);
            for (int y = 0; y < setup.detSize; ++y) {
                for (int x = 0; x < setup.detSize; ++x) image.at<uint16_t>(y, x) = transmission(x, y, source);
            }
            if (!cv::imwrite((folder / std::format("proj_{:04d}.tif", p)).string(), image)) {
                throw std::runtime_error("Failed to write phantom TIFF");
            }
        }
        const ProjectionSequenceOptions options{ setup.nProj, clockwise, duplicate };
        auto integrals = ImageSequenceImporter(folder.string(), ".tif", options).read();
        integrals.forEach([](float value) { return -std::log((value + 1.0f) / 65535.0f); });
        return integrals;
    }
};

TEST_F(ProjectionPhantomTest, BothDirectionsAndEndpointPoliciesReconstructTheSameAsymmetricPhantom) {
    const auto canonical = canonicalIntegrals();
    const auto expected = FeldkampCpu().reconstruct(canonical, setup.geometry());
    for (bool clockwise : { false, true }) {
        for (bool duplicate : { false, true }) {
            const auto actual = readAcquisition(clockwise, duplicate, std::format("{}-{}", clockwise, duplicate));
            ASSERT_EQ(actual.size<2>(), setup.nProj);
            EXPECT_EQ(testing_utils::maxAbsDiff(canonical, actual), 0.0);
            EXPECT_EQ(testing_utils::maxAbsDiff(expected, FeldkampCpu().reconstruct(actual, setup.geometry())), 0.0);
        }
    }
    // Ensure this phantom detects a direction error, rather than merely being rotationally symmetric.
    VolumeF32 wrongDirection(canonical);
    const size_t pixelsPerFrame = setup.detSize * setup.detSize;
    for (int p = 1; p < setup.nProj; ++p) {
        std::copy_n(canonical.ptr() + (setup.nProj - p) * pixelsPerFrame, pixelsPerFrame,
                    wrongDirection.ptr() + p * pixelsPerFrame);
    }
    EXPECT_GT(testing_utils::maxAbsDiff(expected, FeldkampCpu().reconstruct(wrongDirection, setup.geometry())), 1e-3);
}

#if defined(LIBCBCT_WITH_CUDA)
TEST_F(ProjectionPhantomTest, CudaUsesTheSame180AnglesAfterRemovingTheEndpoint) {
    int count = 0;
    if (cudaGetDeviceCount(&count) != cudaSuccess || count == 0) GTEST_SKIP() << "No CUDA device";
    const auto actual = readAcquisition(true, true, "cuda");
    ASSERT_EQ(actual.size<2>(), 180);
    const auto expected = FeldkampCpu().reconstruct(canonicalIntegrals(), setup.geometry());
    const auto gpu = FeldkampCuda(LIBCBCT_SLANG_KERNEL_DIR).reconstruct(actual, setup.geometry());
    EXPECT_LT(testing_utils::maxAbsDiff(expected, gpu), 1e-6);
}
#endif

#if defined(LIBCBCT_WITH_VULKAN)
TEST_F(ProjectionPhantomTest, VulkanUsesTheSame180AnglesAfterRemovingTheEndpoint) {
    try {
        if (VulkanComputeContext::availableDevices().empty()) GTEST_SKIP() << "No Vulkan device";
        const auto actual = readAcquisition(true, true, "vulkan");
        ASSERT_EQ(actual.size<2>(), 180);
        const auto expected = FeldkampCpu().reconstruct(canonicalIntegrals(), setup.geometry());
        const auto gpu = FeldkampVulkan(LIBCBCT_SLANG_KERNEL_DIR).reconstruct(actual, setup.geometry());
        EXPECT_LT(testing_utils::maxAbsDiff(expected, gpu), 1e-6);
    } catch (const VulkanUnavailable &error) {
        GTEST_SKIP() << error.what();
    }
}
#endif

}  // namespace
}  // namespace libcbct::tests
