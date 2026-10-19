#include <gtest/gtest.h>

#include <algorithm>
#include <limits>
#include <span>
#include <type_traits>

#include "Utils/Volume.h"

namespace libcbct::tests {

static_assert(std::is_same_v<decltype(std::declval<VolumeF32 &>().elements()), std::span<float>>);
static_assert(std::is_same_v<decltype(std::declval<const VolumeF32 &>().elements()), std::span<const float>>);
static_assert(std::is_same_v<decltype(std::declval<const VolumeF32 &>().ptr()), const float *>);

TEST(Volume, DefaultConstructor) {
    VolumeF32 vol;
    EXPECT_EQ(vol.size<0>(), 0);
    EXPECT_EQ(vol.size<1>(), 0);
    EXPECT_EQ(vol.size<2>(), 0);
    EXPECT_EQ(vol.ptr(), nullptr);
}

TEST(Volume, ConstructorWithSize) {
    VolumeF32 vol(4, 3, 2);
    EXPECT_EQ(vol.size<0>(), 4);
    EXPECT_EQ(vol.size<1>(), 3);
    EXPECT_EQ(vol.size<2>(), 2);
    EXPECT_NE(vol.ptr(), nullptr);
}

TEST(Volume, CopyConstructor) {
    VolumeF32 vol1(4, 3, 2);
    vol1(1, 1, 1) = 42.0f;

    VolumeF32 vol2(vol1);
    EXPECT_EQ(vol2.size<0>(), 4);
    EXPECT_EQ(vol2.size<1>(), 3);
    EXPECT_EQ(vol2.size<2>(), 2);
    EXPECT_NE(vol2.ptr(), nullptr);
    EXPECT_EQ(vol2(1, 1, 1), 42.0f);
}

TEST(Volume, MoveConstructor) {
    VolumeF32 vol1(4, 3, 2);
    vol1(1, 1, 1) = 42.0f;

    VolumeF32 vol2(std::move(vol1));
    EXPECT_EQ(vol2.size<0>(), 4);
    EXPECT_EQ(vol2.size<1>(), 3);
    EXPECT_EQ(vol2.size<2>(), 2);
    EXPECT_NE(vol2.ptr(), nullptr);
    EXPECT_EQ(vol2(1, 1, 1), 42.0f);

    EXPECT_EQ(vol1.size<0>(), 0);
    EXPECT_EQ(vol1.size<1>(), 0);
    EXPECT_EQ(vol1.size<2>(), 0);
    EXPECT_EQ(vol1.ptr(), nullptr);
}

TEST(Volume, SelfAssignment) {
    VolumeF32 vol(4, 3, 2);
    vol(1, 1, 1) = 42.0f;

    vol = vol;  // Self-assignment
    EXPECT_EQ(vol.size<0>(), 4);
    EXPECT_EQ(vol.size<1>(), 3);
    EXPECT_EQ(vol.size<2>(), 2);
    EXPECT_NE(vol.ptr(), nullptr);
    EXPECT_EQ(vol(1, 1, 1), 42.0f);
}

TEST(Volume, GetMinFloat) {
    VolumeF32 vol(4, 3, 2);
    vol(0, 0, 0) = 5.0f;
    vol(1, 1, 1) = -3.0f;
    vol(2, 2, 1) = 2.0f;

    const float minVal = vol.getMin();
    EXPECT_EQ(minVal, -3.0f);
}

TEST(Volume, GetMaxFloat) {
    VolumeF32 vol(4, 3, 2);
    vol(0, 0, 0) = 5.0f;
    vol(1, 1, 1) = -3.0f;
    vol(2, 2, 1) = 2.0f;

    const float maxVal = vol.getMax();
    EXPECT_EQ(maxVal, 5.0f);
}

TEST(Volume, GetMinUint16) {
    Volume<uint16_t> vol(4, 3, 2);
    std::fill_n(vol.ptr(), vol.numel(), 42);

    const uint16_t minVal = vol.getMin();
    EXPECT_EQ(minVal, 42);
}

TEST(Volume, GetMaxUint16) {
    Volume<uint16_t> vol(4, 3, 2);
    vol(0, 0, 0) = 5;
    vol(1, 1, 1) = 3;
    vol(2, 2, 1) = 2;

    const uint16_t maxVal = vol.getMax();
    EXPECT_EQ(maxVal, 5) << "Expected maximum value to be 5, but got " << maxVal;
}

TEST(Volume, ElementsAreWritableContiguousAndZeroInitialized) {
    VolumeF32 vol(2, 3, 2);
    ASSERT_EQ(vol.elements().size(), 12u);
    EXPECT_EQ(vol.elements().data(), vol.ptr());
    for (const auto value : vol.elements()) EXPECT_EQ(value, 0.0f);
    vol.elements()[7] = 42.0f;
    EXPECT_EQ(vol(1, 0, 1), 42.0f);
    const auto &constant = vol;
    EXPECT_EQ(constant.elements()[7], 42.0f);
}

TEST(Volume, ZeroExtentReleasesStorage) {
    VolumeF32 vol(2, 2, 2);
    vol.resize(3, 0, 4);
    EXPECT_EQ(vol.size<0>(), 3u);
    EXPECT_EQ(vol.size<1>(), 0u);
    EXPECT_EQ(vol.size<2>(), 4u);
    EXPECT_TRUE(vol.empty());
    EXPECT_TRUE(vol.elements().empty());
    EXPECT_EQ(vol.ptr(), nullptr);
    vol.resize(2, 2, 2);
    for (const auto value : vol.elements()) EXPECT_EQ(value, 0.0f);
}

TEST(Volume, OverflowingResizePreservesShapeAndData) {
    VolumeF32 vol(2, 2, 2);
    vol(1, 1, 1) = 42.0f;
    const auto original = vol.ptr();
    const auto maximum = std::numeric_limits<std::size_t>::max();
    EXPECT_THROW(vol.resize(maximum, 2, 1), InvalidArgument);
    // Element count fits, but the number of bytes does not.
    EXPECT_THROW(vol.resize(maximum / sizeof(float) + 1, 1, 1), InvalidArgument);
    EXPECT_EQ(vol.size<0>(), 2u);
    EXPECT_EQ(vol.size<1>(), 2u);
    EXPECT_EQ(vol.size<2>(), 2u);
    EXPECT_EQ(vol.ptr(), original);
    EXPECT_EQ(vol(1, 1, 1), 42.0f);
}

TEST(Volume, RejectsNegativeDimensionsEvenWithZeroExtent) {
    // A zero extent makes this safe even if validation is missing: no large allocation.
    EXPECT_THROW((VolumeU8(-1, 0, 0)), InvalidArgument);
    EXPECT_THROW((VolumeU8(0, -1, 0)), InvalidArgument);
    EXPECT_THROW((VolumeU8(0, 0, -1)), InvalidArgument);
    VolumeU8 vol(2, 2, 2);
    vol(1, 1, 1) = 42;
    const auto original = vol.ptr();
    EXPECT_THROW(vol.resize(-1, 0, 0), InvalidArgument);
    EXPECT_THROW(vol.resize(0, -1, 0), InvalidArgument);
    EXPECT_THROW(vol.resize(0, 0, -1), InvalidArgument);
    EXPECT_EQ(vol.ptr(), original);
    EXPECT_EQ(vol.size<0>(), 2u);
    EXPECT_EQ(vol.size<1>(), 2u);
    EXPECT_EQ(vol.size<2>(), 2u);
    // Do not dereference storage after a failed invariant check.
    if (vol.ptr() == original && vol.numel() == 8) EXPECT_EQ(vol(1, 1, 1), 42);
}

TEST(Volume, CopyAssignmentOwnsIndependentStorage) {
    VolumeF32 original(2, 2, 2);
    original(1, 1, 1) = 42.0f;
    VolumeF32 copy;
    copy = original;
    EXPECT_NE(copy.ptr(), original.ptr());
    EXPECT_EQ(copy(1, 1, 1), 42.0f);
    copy(1, 1, 1) = 7.0f;
    EXPECT_EQ(original(1, 1, 1), 42.0f);
}

}  // namespace libcbct::tests
