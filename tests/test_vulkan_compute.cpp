#include <gtest/gtest.h>

#include <array>
#include <optional>

#include "Utils/VulkanCompute.h"

namespace libcbct::tests {

namespace {

class VulkanComputeTest : public ::testing::Test {
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

TEST_F(VulkanComputeTest, CopiesStagingDataAndSerializesFillAndReadback) {
    const std::array<std::uint32_t, 4> expected{ 1, 5, 17, 12345 };
    const auto upload = context->createBuffer(sizeof(expected), VulkanBufferMemory::Upload);
    const auto device = context->createBuffer(sizeof(expected));
    const auto download = context->createBuffer(sizeof(expected), VulkanBufferMemory::Readback);
    upload.write(std::as_bytes(std::span(expected)));
    auto batch = context->createBatch();
    batch.fill(device, 0xffffffff);
    batch.copy(upload, device, sizeof(expected));
    batch.copy(device, download, sizeof(expected));
    batch.submitAndWait();
    std::array<std::uint32_t, 4> actual{};
    download.read(std::as_writable_bytes(std::span(actual)));
    EXPECT_EQ(actual, expected);
    EXPECT_THROW(batch.submitAndWait(), InvalidArgument);
    EXPECT_THROW(batch.fill(device), InvalidArgument);
}

TEST_F(VulkanComputeTest, RejectsInvalidBufferRangesAndUnmappedAccess) {
    EXPECT_THROW(context->createBuffer(0), InvalidArgument);
    const auto upload = context->createBuffer(16, VulkanBufferMemory::Upload);
    const auto device = context->createBuffer(16);
    const std::array<std::uint32_t, 2> input{};
    EXPECT_THROW(upload.write(std::as_bytes(std::span(input)), 12), InvalidArgument);
    EXPECT_THROW(device.write(std::as_bytes(std::span(input))), InvalidArgument);
    auto batch = context->createBatch();
    EXPECT_THROW(batch.copy(upload, device, 20), InvalidArgument);
    EXPECT_THROW(batch.copy(device, device, 16), InvalidArgument);
    EXPECT_THROW(context->createKernel("missing.spv", 1, 4), IOError);
    EXPECT_THROW(context->createKernel("missing.spv", 1, 4, { 0, 16, 1 }), InvalidArgument);
}

TEST_F(VulkanComputeTest, BatchRetainsBuffersAndDeviceAfterOriginalOwnersAreReleased) {
    const std::array<std::uint32_t, 4> expected{ 7, 11, 13, 17 };
    auto recorded = [&] {
        const auto upload = context->createBuffer(sizeof(expected), VulkanBufferMemory::Upload);
        const auto download = context->createBuffer(sizeof(expected), VulkanBufferMemory::Readback);
        upload.write(std::as_bytes(std::span(expected)));
        auto batch = context->createBatch();
        batch.copy(upload, download, sizeof(expected));
        return std::pair(std::move(batch), download);
    }();
    context.reset();
    recorded.first.submitAndWait();
    std::array<std::uint32_t, 4> actual{};
    recorded.second.read(std::as_writable_bytes(std::span(actual)));
    EXPECT_EQ(actual, expected);
}

}  // namespace

}  // namespace libcbct::tests
