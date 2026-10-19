#include "Utils/Vec.h"

#include <gtest/gtest.h>
#include <type_traits>

namespace libcbct::tests {
template <typename V>
class VectorGeometry : public ::testing::Test {};
using Types = ::testing::Types<vec2f, vec3f, vec4f, vec2d, vec3d, vec4d>;
TYPED_TEST_SUITE(VectorGeometry, Types);

TYPED_TEST(VectorGeometry, DeducesDimensionAndUsesEveryComponent) {
    using V = TypeParam;
    using T = typename V::scalar_t;
    V a;
    V b(T{2});
    for (int i = 0; i < V::dims; ++i) a[i] = static_cast<T>(i + 1);
    static_assert(std::is_same_v<decltype(dot(a, b)), T>);
    EXPECT_EQ(dot(a, b), static_cast<T>(V::dims * (V::dims + 1)));
    const double expectedLength = V::dims == 2 ? 2.2360679774997896964 :
                                  V::dims == 3 ? 3.7416573867739413856 :
                                                 5.4772255750516611346;
    const double tolerance = std::is_same_v<T, float> ? 1e-6 : 1e-14;
    EXPECT_NEAR(length(a), expectedLength, tolerance);
    EXPECT_EQ(length(V(T{0})), T{0});
    const auto unit = normalize(a);
    for (int i = 0; i < V::dims; ++i) {
        EXPECT_NEAR(unit[i], (i + 1) / expectedLength, tolerance);
    }
    EXPECT_NEAR(length(unit), 1.0, tolerance);
}

TEST(VectorGeometry, IntegerDotDeducesAllDimensions) {
    EXPECT_EQ(dot(vec2i(1, 2), vec2i(3, 4)), 11);
    EXPECT_EQ(dot(vec3i(1, 2, 3), vec3i(4, 5, 6)), 32);
    EXPECT_EQ(dot(vec4i(1, 2, 3, 4), vec4i(5, 6, 7, 8)), 70);
}

TEST(VectorGeometry, DoubleLengthKeepsDoublePrecision) {
    EXPECT_NEAR(length(vec2d(3.0000000001, 4.0)), 5.00000000006, 1e-13);
}
}  // namespace libcbct::tests
