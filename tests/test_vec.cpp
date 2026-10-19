#include "Utils/Vec.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <type_traits>

namespace libcbct::tests {

constexpr vec3i v(1, 2, 3);
static_assert(v[0] == 1);
static_assert(v[2] == 3);

constexpr int changed = [] {
    vec3i value(1, 2, 3);
    value[1] = 10;
    return value[1];
}();

static_assert(changed == 10);

template <typename V>
class Vec : public ::testing::Test {};
using VectorTypes = ::testing::Types<vec2i, vec3i, vec4i, vec2f, vec3f, vec4f,
                                    vec2d, vec3d, vec4d>;
TYPED_TEST_SUITE(Vec, VectorTypes);

TYPED_TEST(Vec, ConstructorsAssignmentAndIndexing) {
    using V = TypeParam;
    using T = typename V::scalar_t;
    V zero;
    V broadcast(T{3});
    V indexed;
    for (int i = 0; i < V::dims; ++i) {
        EXPECT_EQ(zero[i], T{0});
        EXPECT_EQ(broadcast[i], T{3});
        indexed[i] = static_cast<T>(i + 1);
    }
    V assigned;
    EXPECT_EQ(&(assigned = indexed), &assigned);
    const V copied(assigned);
    for (int i = 0; i < V::dims; ++i) {
        EXPECT_EQ(copied[i], static_cast<T>(i + 1));
    }
}

TYPED_TEST(Vec, ComponentArithmetic) {
    using V = TypeParam;
    using T = typename V::scalar_t;
    V a;
    V b(T{2});
    for (int i = 0; i < V::dims; ++i) a[i] = static_cast<T>(2 * (i + 1));
    const auto sum = a + b;
    const auto difference = a - b;
    const auto product = a * b;
    const auto quotient = a / b;
    const auto negative = -a;
    for (int i = 0; i < V::dims; ++i) {
        EXPECT_EQ(sum[i], static_cast<T>(2 * (i + 1) + 2));
        EXPECT_EQ(difference[i], static_cast<T>(2 * i));
        EXPECT_EQ(product[i], static_cast<T>(4 * (i + 1)));
        EXPECT_EQ(quotient[i], static_cast<T>(i + 1));
        EXPECT_EQ(negative[i], static_cast<T>(-2 * (i + 1)));
    }
}

TYPED_TEST(Vec, RightScalarPromotesResult) {
    using V = TypeParam;
    using R = vec_t<double, V::dims>;
    const V a(typename V::scalar_t{4});
    static_assert(std::is_same_v<decltype(a + 0.5), R>);
    static_assert(std::is_same_v<decltype(a - 0.5), R>);
    static_assert(std::is_same_v<decltype(a * 0.5), R>);
    static_assert(std::is_same_v<decltype(a / 0.5), R>);
    const auto sum = a + 0.5;
    const auto difference = a - 0.5;
    const auto product = a * 0.5;
    const auto quotient = a / 0.5;
    for (int i = 0; i < V::dims; ++i) {
        EXPECT_EQ(sum[i], 4.5);
        EXPECT_EQ(difference[i], 3.5);
        EXPECT_EQ(product[i], 2.0);
        EXPECT_EQ(quotient[i], 8.0);
    }
}

TYPED_TEST(Vec, CompoundVectorArithmeticReturnsReference) {
    using V = TypeParam;
    using T = typename V::scalar_t;
    V a(T{8});
    const V b(T{2});
    EXPECT_EQ(&(a += b), &a);
    for (int i = 0; i < V::dims; ++i) EXPECT_EQ(a[i], T{10});
    EXPECT_EQ(&(a -= b), &a);
    for (int i = 0; i < V::dims; ++i) EXPECT_EQ(a[i], T{8});
    EXPECT_EQ(&(a *= b), &a);
    for (int i = 0; i < V::dims; ++i) EXPECT_EQ(a[i], T{16});
    EXPECT_EQ(&(a /= b), &a);
    for (int i = 0; i < V::dims; ++i) EXPECT_EQ(a[i], T{8});
}

TYPED_TEST(Vec, EqualityChecksEveryComponent) {
    using V = TypeParam;
    using T = typename V::scalar_t;
    const V a(T{1});
    EXPECT_TRUE(a == a);
    EXPECT_FALSE(a != a);
    for (int i = 0; i < V::dims; ++i) {
        V b = a;
        b[i] = T{2};
        EXPECT_FALSE(a == b);
        EXPECT_TRUE(a != b);
    }
}

template <typename T>
class DivRoundUp : public ::testing::Test {};
using IntegerTypes = ::testing::Types<std::int32_t, std::uint32_t,
                                      std::int64_t, std::uint64_t>;
TYPED_TEST_SUITE(DivRoundUp, IntegerTypes);

TYPED_TEST(DivRoundUp, NonnegativeInputsIncludingMaximum) {
    using T = TypeParam;
    EXPECT_EQ(divRoundUp(T{0}, T{3}), T{0});
    EXPECT_EQ(divRoundUp(T{6}, T{3}), T{2});
    EXPECT_EQ(divRoundUp(T{7}, T{3}), T{3});
    // Volatile prevents folding the overflow-prone implementation at the call site.
    volatile T maximum = std::numeric_limits<T>::max();
    const T value = maximum;
    EXPECT_EQ(divRoundUp(value, T{2}), value / T{2} + value % T{2});
    EXPECT_EQ(divRoundUp(value, value), T{1});
    EXPECT_EQ(divRoundUp(value, T{1}), value);
}

}  // namespace libcbct::tests
