#include "Utils/Vec.h"

#include <gtest/gtest.h>
#include <type_traits>

namespace libcbct::tests {
template <typename V>
class ScalarLeft : public ::testing::Test {};
using Types = ::testing::Types<vec2i, vec3i, vec4i, vec2f, vec3f, vec4f,
                              vec2d, vec3d, vec4d>;
TYPED_TEST_SUITE(ScalarLeft, Types);

TYPED_TEST(ScalarLeft, PromotesAndPreservesOperandOrder) {
    using V = TypeParam;
    using R = vec_t<double, V::dims>;
    V a;
    for (int i = 0; i < V::dims; ++i) a[i] = static_cast<typename V::scalar_t>(i + 1);
    static_assert(std::is_same_v<decltype(12.0 + a), R>);
    static_assert(std::is_same_v<decltype(12.0 - a), R>);
    static_assert(std::is_same_v<decltype(12.0 * a), R>);
    static_assert(std::is_same_v<decltype(12.0 / a), R>);
    const auto sum = 12.0 + a;
    const auto difference = 12.0 - a;
    const auto product = 12.0 * a;
    const auto quotient = 12.0 / a;
    for (int i = 0; i < V::dims; ++i) {
        EXPECT_DOUBLE_EQ(sum[i], 13.0 + i);
        EXPECT_DOUBLE_EQ(difference[i], 11.0 - i);
        EXPECT_DOUBLE_EQ(product[i], 12.0 * (i + 1));
        EXPECT_DOUBLE_EQ(quotient[i], 12.0 / (i + 1));
    }
}
}  // namespace libcbct::tests
