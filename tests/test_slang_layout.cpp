#include <gtest/gtest.h>

#include "Reconstruction/Slang/FdkCudaParameters.h"

namespace libcbct::tests {

namespace {

nlohmann::json reflectionFor(const std::string &entry) {
    std::ifstream input(std::filesystem::path(LIBCBCT_SLANG_KERNEL_DIR) / (entry + ".cuda.json"));
    if (!input) throw std::runtime_error("Missing generated CUDA reflection for " + entry);
    return nlohmann::json::parse(input);
}

TEST(SlangCudaLayout, MatchesGeneratedLayoutsForAllFdkKernels) {
    for (const std::string entry : { "weightAndPadKernel", "applyFilterKernel", "backProjectionKernel" }) {
        SCOPED_TRACE(entry);
        EXPECT_NO_THROW(libcbct::detail::verifyFdkCudaLayout(reflectionFor(entry), entry));
    }
}

TEST(SlangCudaLayout, RejectsChangedBufferAndGeometryOffsets) {
    auto weighting = reflectionFor("weightAndPadKernel");
    weighting["entryPoints"][0]["parameters"][1]["type"]["fields"][1]["binding"]["offset"] = 8;
    EXPECT_THROW(libcbct::detail::verifyFdkCudaLayout(weighting, "weightAndPadKernel"), libcbct::InvalidArgument);
    auto back = reflectionFor("backProjectionKernel");
    back["entryPoints"][0]["parameters"][1]["type"]["fields"][4]["type"]["fields"][4]["binding"]["offset"] = 28;
    EXPECT_THROW(libcbct::detail::verifyFdkCudaLayout(back, "backProjectionKernel"), libcbct::InvalidArgument);
}

TEST(SlangCudaLayout, RejectsChangedDispatchSizeAndGlobalBindings) {
    auto reflection = reflectionFor("applyFilterKernel");
    reflection["entryPoints"][0]["threadGroupSize"] = { 8, 8, 1 };
    EXPECT_THROW(libcbct::detail::verifyFdkCudaLayout(reflection, "applyFilterKernel"), libcbct::InvalidArgument);
    reflection = reflectionFor("applyFilterKernel");
    reflection["parameters"].push_back({ { "name", "global" } });
    EXPECT_THROW(libcbct::detail::verifyFdkCudaLayout(reflection, "applyFilterKernel"), libcbct::InvalidArgument);
}

}  // namespace

}  // namespace libcbct::tests
