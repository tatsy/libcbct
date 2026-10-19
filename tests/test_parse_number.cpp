#include "Utils/ParseNumber.h"
#include "Utils/CheckedSize.h"

#include <gtest/gtest.h>

#include <limits>
#include <string>
#include <string_view>

namespace libcbct::tests {
namespace {
template <typename T>
concept ParsesNumber = requires(std::string_view text) { detail::parseNumber<T>(text); };
static_assert(ParsesNumber<int> && ParsesNumber<float>);
static_assert(!ParsesNumber<bool> && !ParsesNumber<double>);
}  // namespace

TEST(ParseNumber, AcceptsIntegersAndBoundaries) {
    EXPECT_EQ(detail::parseNumber<int>("0"), 0);
    EXPECT_EQ(detail::parseNumber<int>("-42"), -42);
    EXPECT_EQ(detail::parseNumber<int>(std::to_string(std::numeric_limits<int>::min())),
              std::numeric_limits<int>::min());
    EXPECT_EQ(detail::parseNumber<int>(std::to_string(std::numeric_limits<int>::max())),
              std::numeric_limits<int>::max());
}

TEST(ParseNumber, AcceptsFloatAndScientificNotation) {
    EXPECT_FLOAT_EQ(detail::parseNumber<float>("-1.25"), -1.25f);
    EXPECT_FLOAT_EQ(detail::parseNumber<float>("2.5e2"), 250.0f);
    EXPECT_FLOAT_EQ(detail::parseNumber<float>("0"), 0.0f);
}

TEST(ParseNumber, ConsumesExactStringViewIncludingEmbeddedNul) {
    const std::string text = "123junk";
    EXPECT_EQ(detail::parseNumber<int>(std::string_view(text.data(), 3)), 123);
    EXPECT_THROW(detail::parseNumber<int>(text), InvalidArgument);
    EXPECT_THROW(detail::parseNumber<int>(std::string_view("12\0junk", 7)), InvalidArgument);
    EXPECT_THROW(detail::parseNumber<float>(std::string_view("1.5\0junk", 8)), InvalidArgument);
}

TEST(ParseNumber, RejectsEmptyMalformedAndOutOfRangeNumbers) {
    for (const auto text : {"", "abc", "12abc", "1.5", "9999999999999999999999999"}) {
        SCOPED_TRACE(text);
        EXPECT_THROW(detail::parseNumber<int>(text), InvalidArgument);
    }
    for (const auto text : {"", "abc", "1.5abc", "1e", "1e1000", "1e-1000"}) {
        SCOPED_TRACE(text);
        EXPECT_THROW(detail::parseNumber<float>(text), InvalidArgument);
    }
}
}  // namespace libcbct::tests
