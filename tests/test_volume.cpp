#include <gtest/gtest.h>

#include <algorithm>

#include "Utils/Volume.h"

namespace libcbct::tests {

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

}  // namespace libcbct::tests
