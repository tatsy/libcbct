#include <gtest/gtest.h>

#include <limits>
#include <thread>
#include <vector>

#include "Common/ProgressBar.h"

namespace libcbct::tests {
namespace {

void expectFiniteDisplay(const std::string &output) {
    EXPECT_EQ(output.find("nan"), std::string::npos);
    EXPECT_EQ(output.find("inf"), std::string::npos);
}

TEST(ProgressBar, DefaultAndZeroTotalCompleteOnceWithoutDivisionErrors) {
    for (bool useDefault : { false, true }) {
        ProgressBar bar;
        if (!useDefault) bar.reset(0);
        ::testing::internal::CaptureStdout();
        bar.step(0);
        bar.step();
        bar.finish();
        const auto output = ::testing::internal::GetCapturedStdout();
        EXPECT_NE(output.find("100%"), std::string::npos);
        EXPECT_NE(output.find("0/0"), std::string::npos);
        EXPECT_EQ(std::count(output.begin(), output.end(), '\n'), 1);
        expectFiniteDisplay(output);
    }
}

TEST(ProgressBar, PreservesTheEntireDescriptionAndHandlesImmediateZeroProgress) {
    ProgressBar bar(2);
    ::testing::internal::CaptureStdout();
    bar.setDescription("");
    bar.step(0);
    bar.setDescription("%s-%d!", "label", 123);
    bar.setWidth(1);
    bar.step();
    bar.finish();
    const auto output = ::testing::internal::GetCapturedStdout();
    EXPECT_NE(output.find("label-123!"), std::string::npos);
    EXPECT_NE(output.find("0/2"), std::string::npos);
    EXPECT_NE(output.find("2/2"), std::string::npos);
    expectFiniteDisplay(output);
}

TEST(ProgressBar, RejectsInvalidArgumentsWithoutChangingState) {
    EXPECT_THROW(ProgressBar(-1), InvalidArgument);
    ProgressBar bar(2);
    EXPECT_THROW(bar.setWidth(0), InvalidArgument);
    EXPECT_THROW(bar.setWidth(-1), InvalidArgument);
    EXPECT_THROW(bar.step(-1), InvalidArgument);
    EXPECT_THROW(bar.reset(-1), InvalidArgument);
    ::testing::internal::CaptureStdout();
    bar.finish();
    const auto output = ::testing::internal::GetCapturedStdout();
    EXPECT_NE(output.find("2/2"), std::string::npos);
}

TEST(ProgressBar, CapsOvershootAndPrintsCompletionOnlyOnce) {
    ProgressBar bar(2);
    ::testing::internal::CaptureStdout();
    bar.step();
    bar.step(std::numeric_limits<int>::max());
    bar.step(std::numeric_limits<int>::max());
    bar.finish();
    const auto output = ::testing::internal::GetCapturedStdout();
    EXPECT_NE(output.find("2/2"), std::string::npos);
    EXPECT_EQ(std::count(output.begin(), output.end(), '\n'), 1);
    expectFiniteDisplay(output);
}

TEST(ProgressBar, ResetRetainsOrReplacesTotalIncludingZero) {
    ProgressBar bar(2);
    ::testing::internal::CaptureStdout();
    bar.finish();
    bar.reset();
    bar.finish();
    bar.reset(3);
    bar.finish();
    bar.reset(0);
    bar.finish();
    const auto output = ::testing::internal::GetCapturedStdout();
    EXPECT_EQ(std::count(output.begin(), output.end(), '\n'), 4);
    EXPECT_NE(output.find("3/3"), std::string::npos);
    EXPECT_NE(output.find("0/0"), std::string::npos);
}

TEST(ProgressBar, IntegerPercentagesAndLargeTotalsDoNotRoundDownOrOverflow) {
    ProgressBar bar(100);
    ::testing::internal::CaptureStdout();
    bar.step(29);
    bar.reset(std::numeric_limits<int>::max());
    bar.setWidth(3);
    bar.step(std::numeric_limits<int>::max() - 1);
    bar.finish();
    const auto output = ::testing::internal::GetCapturedStdout();
    EXPECT_NE(output.find("29%"), std::string::npos);
    EXPECT_NE(output.find("2147483647/2147483647"), std::string::npos);
    EXPECT_NE(output.find("|===|"), std::string::npos);
    expectFiniteDisplay(output);
}

TEST(ProgressBar, ConcurrentUpdatesAndFinishRemainConsistent) {
    ProgressBar bar(100);
    ::testing::internal::CaptureStdout();
    std::vector<std::thread> workers;
    for (int i = 0; i < 4; ++i) {
        workers.emplace_back([&bar] {
            for (int j = 0; j < 50; ++j) {
                bar.setDescription("work-%d", j);
                bar.setWidth(8);
                bar.step();
            }
            bar.finish();
        });
    }
    for (auto &worker : workers) worker.join();
    const auto output = ::testing::internal::GetCapturedStdout();
    EXPECT_NE(output.find("100/100"), std::string::npos);
    EXPECT_EQ(std::count(output.begin(), output.end(), '\n'), 1);
    expectFiniteDisplay(output);
}

}  // namespace
}  // namespace libcbct::tests
