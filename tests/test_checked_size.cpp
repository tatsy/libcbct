#include "Utils/CheckedSize.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <type_traits>

namespace libcbct::tests {
namespace {
template <typename... Args>
concept HasCheckedProduct = requires(Args... args) { detail::checkedProduct(args...); };
static_assert(HasCheckedProduct<int, unsigned, std::size_t>);
static_assert(!HasCheckedProduct<>);
static_assert(!HasCheckedProduct<bool>);
static_assert(!HasCheckedProduct<int, bool>);
static_assert(!HasCheckedProduct<double>);
static_assert(!HasCheckedProduct<int, float>);
static_assert(std::is_same_v<decltype(detail::checkedProduct(2, 3u)), std::size_t>);
static_assert(detail::checkedProduct(7) == 7);
static_assert(detail::checkedProduct(2, 3u, std::uint64_t{4}, short{5}) == 120);
static_assert(detail::checkedProduct(0, 100) == 0);
static_assert(detail::checkedSize(42) == 42);
}  // namespace

TEST(CheckedSize, RejectsNegativeValues) {
    EXPECT_THROW(detail::checkedSize(-1), InvalidArgument);
    EXPECT_THROW(detail::checkedSize(std::numeric_limits<std::int64_t>::min()), InvalidArgument);
    EXPECT_THROW(detail::checkedProduct(2, -3, 4), InvalidArgument);
    EXPECT_THROW(detail::checkedProduct(-1), InvalidArgument);
}

TEST(CheckedSize, ValidatesAllArgumentsEvenWithZero) {
    EXPECT_THROW(detail::checkedProduct(0, -1), InvalidArgument);
    EXPECT_THROW(detail::checkedProduct(-1, 0), InvalidArgument);
    EXPECT_THROW(detail::checkedProduct(2, 0, -1), InvalidArgument);
    EXPECT_EQ(detail::checkedProduct(2, 0, 3), 0u);
}

TEST(CheckedSize, ChecksEveryIntermediateProduct) {
    constexpr auto maximum = std::numeric_limits<std::size_t>::max();
    EXPECT_EQ(detail::checkedProduct(maximum, 1), maximum);
    EXPECT_EQ(detail::checkedProduct(maximum, 0), 0u);
    EXPECT_EQ(detail::checkedProduct(maximum / 2, 2), (maximum / 2) * 2);
    EXPECT_THROW(detail::checkedProduct(maximum, 2), InvalidArgument);
    EXPECT_THROW(detail::checkedProduct(1, maximum, 2), InvalidArgument);
    // The contract checks a left fold, including overflow before a later zero.
    EXPECT_THROW(detail::checkedProduct(maximum, 2, 0), InvalidArgument);
    EXPECT_EQ(detail::checkedProduct(0, maximum, 2), 0u);
}

TEST(CheckedSize, ChecksSizeTRepresentability) {
    if constexpr (std::numeric_limits<std::uint64_t>::digits >
                  std::numeric_limits<std::size_t>::digits) {
        EXPECT_THROW(detail::checkedSize(std::numeric_limits<std::uint64_t>::max()), InvalidArgument);
    } else {
        EXPECT_EQ(detail::checkedSize(std::numeric_limits<std::size_t>::max()),
                  std::numeric_limits<std::size_t>::max());
    }
}
}  // namespace libcbct::tests
