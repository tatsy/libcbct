#include "Utils/Vec.h"

#include <gtest/gtest.h>
#include <type_traits>

namespace libcbct::tests {
template <typename V>
class MixedCompound : public ::testing::Test {};
using Types = ::testing::Types<vec2i, vec3i, vec4i, vec2f, vec3f, vec4f,
                              vec2d, vec3d, vec4d>;
TYPED_TEST_SUITE(MixedCompound, Types);

TYPED_TEST(MixedCompound, RetainsLeftTypeAndReturnsReference) {
    using V = TypeParam;
    using T = typename V::scalar_t;
    V a(T{4});
    static_assert(std::is_same_v<decltype(a += 0.5), V &>);
    static_assert(std::is_same_v<decltype(a -= 0.5), V &>);
    static_assert(std::is_same_v<decltype(a *= 1.5), V &>);
    static_assert(std::is_same_v<decltype(a /= 2.5), V &>);
    EXPECT_EQ(&(a += 0.5), &a);
    for (int i = 0; i < V::dims; ++i) EXPECT_EQ(a[i], static_cast<T>(4.5));
    a = V(T{4});
    EXPECT_EQ(&(a -= 0.5), &a);
    for (int i = 0; i < V::dims; ++i) EXPECT_EQ(a[i], static_cast<T>(3.5));
    a = V(T{4});
    EXPECT_EQ(&(a *= 1.5), &a);
    for (int i = 0; i < V::dims; ++i) EXPECT_EQ(a[i], T{6});
    a = V(T{4});
    EXPECT_EQ(&(a /= 2.5), &a);
    for (int i = 0; i < V::dims; ++i) EXPECT_EQ(a[i], static_cast<T>(1.6));
}
}  // namespace libcbct::tests
