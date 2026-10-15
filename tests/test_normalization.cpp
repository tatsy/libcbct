#include <gtest/gtest.h>

#include <fstream>
#include <limits>
#include <vector>

#include "IO/RawVolumeExporter.h"
#include "TestDirectory.h"
#include "Utils/Normalization.h"

namespace libcbct::tests {
namespace {

VolumeF32 volumeOf(const std::vector<float> &values) {
    VolumeF32 volume(static_cast<int>(values.size()), 1, 1);
    std::copy(values.begin(), values.end(), volume.ptr());
    return volume;
}

template <typename T>
std::vector<T> readRaw(const std::filesystem::path &path, size_t count) {
    EXPECT_EQ(std::filesystem::file_size(path), count * sizeof(T));
    std::vector<T> values(count);
    std::ifstream input(path, std::ios::binary);
    input.read(reinterpret_cast<char *>(values.data()), values.size() * sizeof(T));
    EXPECT_TRUE(input.good());
    return values;
}

TEST(Normalization, PositiveMaximumHandlesZeroConstantNegativeAndOrdinaryValues) {
    const std::vector<std::pair<std::vector<float>, std::vector<float>>> cases = {
        { { 0, 0, 0 }, { 0, 0, 0 } },
        { { 5, 5, 5 }, { 1, 1, 1 } },
        { { -3, -1, -2 }, { 0, 0, 0 } },
        { { -3, 0, 1, 2, 4 }, { 0, 0, 0.25f, 0.5f, 1 } },
    };
    for (const auto &[input, expected] : cases) {
        auto volume = volumeOf(input);
        normalizePositiveMaximum(volume);
        for (size_t i = 0; i < expected.size(); ++i) EXPECT_EQ(volume.ptr()[i], expected[i]);
    }
}

TEST(Normalization, PositiveMaximumPreservesTheOriginalFormulaForOrdinaryData) {
    const std::vector<float> input = { -0.3f, 0.0123f, 0.53f, 0.9876f, 1.2345f };
    auto volume = volumeOf(input);
    normalizePositiveMaximum(volume);
    for (size_t i = 0; i < input.size(); ++i) EXPECT_EQ(volume.ptr()[i], std::max(0.0f, input[i]) / input.back());
}

TEST(Normalization, RejectsNonfiniteValuesBeforeModifyingAnyPixel) {
    for (float invalid : { std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(),
                           -std::numeric_limits<float>::infinity() }) {
        for (int position : { 0, 2 }) {
            auto volume = volumeOf({ 2, 4, 6 });
            volume.ptr()[position] = invalid;
            EXPECT_THROW(normalizePositiveMaximum(volume), InvalidArgument);
            EXPECT_EQ(volume.ptr()[1], 4.0f);
        }
    }
    VolumeF32 empty;
    EXPECT_THROW(normalizePositiveMaximum(empty), InvalidArgument);
}

template <typename T>
void checkIntegerOutput(VolumeType type, float upper) {
    testing_utils::TestDirectory directory;
    const auto path = directory.path / "volume.raw";
    for (float constant : { 0.0f, 5.0f, -3.0f }) {
        RawVolumeExporter().write(path.string(), volumeOf({ constant, constant, constant }), type);
        EXPECT_EQ(readRaw<T>(path, 3), (std::vector<T>{ 0, 0, 0 }));
    }
    for (const std::vector<float> input : { std::vector<float>{ -5, -3, -1 }, { -0.3f, 0, 0.0123f, 0.53f, 1.2345f } }) {
        RawVolumeExporter().write(path.string(), volumeOf(input), type);
        const auto actual = readRaw<T>(path, input.size());
        for (size_t i = 0; i < input.size(); ++i) {
            const float normalized = (input[i] - input.front()) / (input.back() - input.front());
            EXPECT_EQ(actual[i], static_cast<T>(0.0f + normalized * upper));
        }
    }
    const float largest = std::numeric_limits<float>::max();
    RawVolumeExporter().write(path.string(), volumeOf({ -largest, 0, largest }), type);
    EXPECT_EQ(readRaw<T>(path, 3), (std::vector<T>{ 0, static_cast<T>(upper * 0.5f), static_cast<T>(upper) }));
}

TEST(RawNormalization, Uint8KeepsRangeAndOrdinaryOutput) { checkIntegerOutput<uint8_t>(VolumeType::Uint8, 255.0f); }
TEST(RawNormalization, Uint16KeepsRangeAndOrdinaryOutput) { checkIntegerOutput<uint16_t>(VolumeType::Uint16, 50000.0f); }
TEST(RawNormalization, Uint32KeepsRangeAndOrdinaryOutput) { checkIntegerOutput<uint32_t>(VolumeType::Uint32, 50000.0f); }

TEST(RawNormalization, RejectsNonfiniteInputBeforeTruncatingTheDestination) {
    testing_utils::TestDirectory directory;
    const auto path = directory.path / "volume.raw";
    for (auto type : { VolumeType::Uint8, VolumeType::Uint16, VolumeType::Uint32 }) {
        for (float invalid : { std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(),
                               -std::numeric_limits<float>::infinity() }) {
            for (int position : { 0, 2 }) {
                auto volume = volumeOf({ 1, 2, 3 });
                volume.ptr()[position] = invalid;
                std::ofstream(path, std::ios::binary) << "keep";
                EXPECT_THROW(RawVolumeExporter().write(path.string(), volume, type), InvalidArgument);
                EXPECT_EQ(readRaw<char>(path, 4), (std::vector<char>{ 'k', 'e', 'e', 'p' }));
            }
        }
    }
}

TEST(RawNormalization, FloatingPointOutputPreservesConstantValues) {
    testing_utils::TestDirectory directory;
    const auto path = directory.path / "volume.raw";
    for (float value : { 0.0f, 5.0f, -3.0f }) {
        const auto volume = volumeOf({ value, value, value });
        RawVolumeExporter().write(path.string(), volume, VolumeType::Float32);
        EXPECT_EQ(readRaw<float>(path, 3), (std::vector<float>{ value, value, value }));
        RawVolumeExporter().write(path.string(), volume, VolumeType::Float64);
        EXPECT_EQ(readRaw<double>(path, 3), (std::vector<double>{ value, value, value }));
    }
}

}  // namespace
}  // namespace libcbct::tests
