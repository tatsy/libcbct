#include "Utils/Volume.h"

#include <gtest/gtest.h>

namespace libcbct::tests {
TEST(VolumeForEach, AcceptsValueAndRvalueCallbacks) {
    VolumeF32 volume(2, 2, 2);
    volume.forEach([](float value) { return value + 1.0f; });
    for (const auto value : volume.elements()) EXPECT_FLOAT_EQ(value, 1.0f);
    // The public constraint accepts callables invocable with a value of T.
    volume.forEach([](float &&value) { return value + 2.0f; });
    for (const auto value : volume.elements()) EXPECT_FLOAT_EQ(value, 3.0f);
}

TEST(VolumeForEach, EmptyVolumeDoesNotInvokeCallback) {
    VolumeF32 volume;
    int calls = 0;
    volume.forEach([&calls](float) { ++calls; return 1.0f; });
    EXPECT_EQ(calls, 0);
}
}  // namespace libcbct::tests
