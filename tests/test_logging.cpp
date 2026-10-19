#include <gtest/gtest.h>

#include <array>
#include <bit>
#include <cstdint>
#include <source_location>
#include <string>

#include "Common/Constants.h"
#include "Common/Logging.h"

namespace libcbct::tests {
namespace {

TEST(Constants, PreservesCompileTimeFloatAndDoubleValues) {
    // Baseline values from the former M_PI, M_PI_2 and M_1_PI definitions.
    constexpr std::array expected = { 0x1.921fb54442d18p+1, 0x1.921fb54442d18p+0, 0x1.921fb54442d18p+2,
                                      0x1.921fb54442d18p+3, 0x1.45f306dc9c883p-2, 0x1.45f306dc9c883p-3,
                                      0x1.45f306dc9c883p-4 };
    constexpr std::array doubles = { static_cast<double>(kPi),           static_cast<double>(kHalfPi),
                                     static_cast<double>(kTwoPi),        static_cast<double>(kFourPi),
                                     static_cast<double>(kOneOverPi),    static_cast<double>(kOneOverTwoPi),
                                     static_cast<double>(kOneOverFourPi) };
    constexpr std::array floats = { static_cast<float>(kPi),           static_cast<float>(kHalfPi),
                                    static_cast<float>(kTwoPi),        static_cast<float>(kFourPi),
                                    static_cast<float>(kOneOverPi),    static_cast<float>(kOneOverTwoPi),
                                    static_cast<float>(kOneOverFourPi) };
    static_assert(doubles == expected);
    for (std::size_t i = 0; i < expected.size(); ++i) {
        EXPECT_EQ(std::bit_cast<std::uint64_t>(doubles[i]), std::bit_cast<std::uint64_t>(expected[i]));
        EXPECT_EQ(std::bit_cast<std::uint32_t>(floats[i]),
                  std::bit_cast<std::uint32_t>(static_cast<float>(expected[i])));
    }
}

TEST(Logging, EachLevelReportsTheOriginalCallSite) {
    ::testing::internal::CaptureStdout();
    const auto infoLine = std::source_location::current().line() + 1;
    LIBCBCT_INFO("value={}", 7);
    const auto debugLine = std::source_location::current().line() + 1;
    LIBCBCT_DEBUG("debug message");
    const auto warningLine = std::source_location::current().line() + 1;
    LIBCBCT_WARN("warning={}", "text");
    const auto output = ::testing::internal::GetCapturedStdout();
    EXPECT_NE(output.find("value=7"), std::string::npos);
    EXPECT_NE(output.find("debug message"), std::string::npos);
    EXPECT_NE(output.find("warning=text"), std::string::npos);
    for (const auto line : { infoLine, debugLine, warningLine }) {
        EXPECT_NE(output.find(std::format("test_logging.cpp:{}", line)), std::string::npos);
    }
    EXPECT_NE(output.find(" in "), std::string::npos);
}

TEST(Logging, TypedExceptionReportsTheOriginalCallSite) {
    std::string message;
    const auto line = std::source_location::current().line() + 2;
    try {
        LIBCBCT_THROW<InvalidArgument>("invalid value={}", 7);
    } catch (const InvalidArgument &error) {
        message = error.what();
    }
    EXPECT_NE(message.find("invalid value=7"), std::string::npos);
    EXPECT_NE(message.find(std::format("test_logging.cpp:{}", line)), std::string::npos);
}

TEST(Logging, ErrorForwardingPreservesTheOriginalCallSite) {
    std::string message;
    const auto line = std::source_location::current().line() + 2;
    try {
        LIBCBCT_ERROR("error={}", 9);
    } catch (const Error &error) {
        message = error.what();
    }
    EXPECT_NE(message.find("error=9"), std::string::npos);
    EXPECT_NE(message.find(std::format("test_logging.cpp:{}", line)), std::string::npos);
}

TEST(Logging, AssertionLabelReportsTheOriginalCallSite) {
    const auto line = std::source_location::current().line() + 1;
    const auto label = ASRT_LABEL("bad input");
    EXPECT_NE(label.find("Assertion failed: bad input"), std::string::npos);
    EXPECT_NE(label.find(std::format("test_logging.cpp:{}", line)), std::string::npos);
}

TEST(Logging, AssertionFunctionEvaluatesItsConditionOnce) {
    int calls = 0;
    LIBCBCT_ASSERT(++calls == 1, "Condition was evaluated once");
    EXPECT_EQ(calls, 1);
    LIBCBCT_ASSERT(true);
}

TEST(LoggingDeathTest, FormattedAssertionReportsTheOriginalCallSite) {
    const auto line = std::source_location::current().line() + 2;
    const auto pattern = std::format("Assertion failed: bad value=7 .*test_logging.cpp:{}", line);
    EXPECT_DEATH(LIBCBCT_ASSERT(false, "bad value={}", 7), pattern);
}

TEST(LoggingDeathTest, AssertionWithoutAMessageAborts) {
    EXPECT_DEATH(LIBCBCT_ASSERT(false), "Assertion failed: Condition is false");
}

}  // namespace
}  // namespace libcbct::tests
