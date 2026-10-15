#include <gtest/gtest.h>

#include <type_traits>

// Simulate consumer types declared before the public library header.
struct Geometry {};
template <typename T>
struct Volume {};
enum class RampFilter { ConsumerFilter };

#include "libcbct.h"

namespace consumer {

namespace cbct = libcbct;

TEST(Namespace, PublicTypesCoexistWithConsumerTypes) {
    static_assert(!std::is_same_v<::Geometry, cbct::Geometry>);
    static_assert(!std::is_same_v<::Volume<float>, cbct::Volume<float>>);
    static_assert(!std::is_same_v<::RampFilter, cbct::RampFilter>);
    const cbct::Geometry geometry(cbct::vec2i(8, 8), cbct::vec2f(1.0f, 1.0f),
                                  cbct::vec3i(4, 4, 4), 100.0f, 400.0f);
    const cbct::VolumeF32 volume(4, 4, 4);
    const cbct::FeldkampCPU reconstruction(cbct::RampFilter::RamLak);
    EXPECT_EQ(geometry.volSize.x, volume.size<0>());
}

TEST(Namespace, VectorOperatorsAndMathWorkWithoutUsingDirectives) {
    const cbct::vec3f a(1.0f, 4.0f, 9.0f);
    const auto sum = a + cbct::vec3f(1.0f, 2.0f, 3.0f);
    EXPECT_FLOAT_EQ(sum.z, 12.0f);
    EXPECT_FLOAT_EQ(cbct::sqrt(a).y, 2.0f);
    EXPECT_FLOAT_EQ(cbct::cos(cbct::vec3f(0.0f)).x, 1.0f);
    EXPECT_FLOAT_EQ(cbct::sin(cbct::vec3f(0.0f)).y, 0.0f);
    EXPECT_FLOAT_EQ(cbct::abs(cbct::vec3f(-1.0f)).z, 1.0f);
    EXPECT_FLOAT_EQ(cbct::min(a, cbct::vec3f(2.0f)).z, 2.0f);
    EXPECT_FLOAT_EQ(cbct::max(a, cbct::vec3f(2.0f)).x, 2.0f);
    EXPECT_FLOAT_EQ(cbct::clamp(3.0f, 0.0f, 2.0f), 2.0f);
    EXPECT_FLOAT_EQ(cbct::clamp(a, cbct::vec3f(0.0f), cbct::vec3f(2.0f)).z, 2.0f);
}

TEST(Namespace, LoggingAndCheckMacrosWorkInConsumerScope) {
    EXPECT_EQ(cbct::STR_FMT("value={}", 7), "value=7");
    EXPECT_THROW(cbct::LIBCBCT_THROW<cbct::InvalidArgument>("invalid value"), cbct::InvalidArgument);
    LIBCBCT_ASSERT(true, "A consumer can invoke the assertion macro");
#if defined(LIBCBCT_WITH_CUDA)
    EXPECT_NO_THROW(CUDA_CHECK(cudaSuccess));
    EXPECT_NO_THROW(CUFFT_CHECK(CUFFT_SUCCESS));
#endif
}

}  // namespace consumer
