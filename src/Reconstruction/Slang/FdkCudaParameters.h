#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <string>

#include <nlohmann/json.hpp>

#include "Common/Exception.h"

namespace libcbct::detail {

// CUDA structured buffers carry a device address and an element count.
struct BufferBinding {
    void *data;
    std::uint64_t count;
};

// CUDA int2/float2 have eight-byte alignment, including the tail padding here.
struct alignas(8) FdkCudaGeometry {
    std::int32_t detSize[2];
    float pixSize[2];
    std::int32_t volSize[3];
    float sod;
    float sdd;
    std::uint32_t padding = 0;
};

struct WeightAndPadArgs {
    BufferBinding projection;
    BufferBinding weights;
    BufferBinding padded;
    std::int32_t detWidth;
    std::int32_t detHeight;
    std::int32_t nPad;
};

struct FilterArgs {
    BufferBinding spectrum;
    BufferBinding response;
    std::int32_t nFreq;
    std::int32_t detHeight;
};

struct BackProjectionArgs {
    BufferBinding filtered;
    BufferBinding volume;
    std::int32_t detWidth;
    std::int32_t detHeight;
    FdkCudaGeometry geometry;
    float theta;
    float scale;
};

static_assert(sizeof(void *) == 8 && sizeof(BufferBinding) == 16);
static_assert(sizeof(FdkCudaGeometry) == 40);
static_assert(sizeof(WeightAndPadArgs) == 64);
static_assert(sizeof(FilterArgs) == 40);
static_assert(sizeof(BackProjectionArgs) == 88);

struct UniformField {
    const char *name;
    std::size_t offset;
    std::size_t size;
};

inline void verifyUniform(const nlohmann::json &field, std::size_t offset, std::size_t size) {
    const auto &binding = field.at("binding");
    if (binding.at("kind") != "uniform" || binding.at("offset") != offset || binding.at("size") != size) {
        throw InvalidArgument("Slang CUDA ABI mismatch for " + field.at("name").get<std::string>());
    }
}

inline void verifyFields(const nlohmann::json &type, std::initializer_list<UniformField> expected) {
    if (type.at("kind") != "struct" || type.at("fields").size() != expected.size()) {
        throw InvalidArgument("Slang CUDA ABI requires the expected parameter structure");
    }
    std::size_t index = 0;
    for (const auto &layout : expected) {
        const auto &field = type.at("fields").at(index++);
        if (field.at("name") != layout.name) {
            throw InvalidArgument("Slang CUDA ABI field mismatch: expected " + std::string(layout.name));
        }
        verifyUniform(field, layout.offset, layout.size);
    }
}

// Validate reflection from the same compilation as the PTX before launching it.
inline void verifyFdkCudaLayout(const nlohmann::json &reflection, const std::string &entryName) {
    if (!reflection.at("parameters").empty() || reflection.at("entryPoints").size() != 1) {
        throw InvalidArgument("FDK PTX requires one entry point and no global parameters");
    }
    const auto &entry = reflection.at("entryPoints").at(0);
    if (entry.at("name") != entryName || entry.at("stage") != "compute"
        || entry.at("threadGroupSize") != nlohmann::json::array({ 16, 16, 1 }) || entry.at("parameters").size() != 2) {
        throw InvalidArgument("FDK PTX entry point or thread group does not match " + entryName);
    }
    const auto &parameters = entry.at("parameters");
    if (parameters.at(0).at("semanticName") != "SV_DISPATCHTHREADID" || parameters.at(1).at("name") != "params") {
        throw InvalidArgument("FDK PTX requires dispatch IDs and one uniform parameter structure");
    }
    const auto &params = parameters.at(1);
    const auto &type = params.at("type");
#define FDK_FIELD(T, member)                            \
    UniformField {                                      \
        #member, offsetof(T, member), sizeof(T::member) \
    }
    if (entryName == "weightAndPadKernel") {
        verifyUniform(params, 0, sizeof(WeightAndPadArgs));
        verifyFields(type, { FDK_FIELD(WeightAndPadArgs, projection), FDK_FIELD(WeightAndPadArgs, weights),
                             FDK_FIELD(WeightAndPadArgs, padded), FDK_FIELD(WeightAndPadArgs, detWidth),
                             FDK_FIELD(WeightAndPadArgs, detHeight), FDK_FIELD(WeightAndPadArgs, nPad) });
    } else if (entryName == "applyFilterKernel") {
        verifyUniform(params, 0, sizeof(FilterArgs));
        verifyFields(type, { FDK_FIELD(FilterArgs, spectrum), FDK_FIELD(FilterArgs, response),
                             FDK_FIELD(FilterArgs, nFreq), FDK_FIELD(FilterArgs, detHeight) });
    } else if (entryName == "backProjectionKernel") {
        verifyUniform(params, 0, sizeof(BackProjectionArgs));
        verifyFields(type, { FDK_FIELD(BackProjectionArgs, filtered), FDK_FIELD(BackProjectionArgs, volume),
                             FDK_FIELD(BackProjectionArgs, detWidth), FDK_FIELD(BackProjectionArgs, detHeight),
                             FDK_FIELD(BackProjectionArgs, geometry), FDK_FIELD(BackProjectionArgs, theta),
                             FDK_FIELD(BackProjectionArgs, scale) });
        verifyFields(
            type.at("fields").at(4).at("type"),
            { FDK_FIELD(FdkCudaGeometry, detSize), FDK_FIELD(FdkCudaGeometry, pixSize),
              FDK_FIELD(FdkCudaGeometry, volSize), FDK_FIELD(FdkCudaGeometry, sod), FDK_FIELD(FdkCudaGeometry, sdd) });
    } else {
        throw InvalidArgument("Unknown FDK PTX entry point: " + entryName);
    }
#undef FDK_FIELD
}

inline void verifyFdkCudaLayoutFile(const std::filesystem::path &path, const std::string &entryName) {
    std::ifstream input(path);
    if (!input) {
        throw IOError("Cannot open PTX reflection file: " + path.string());
    }
    try {
        const auto reflection = nlohmann::json::parse(input);
        verifyFdkCudaLayout(reflection, entryName);
    } catch (const nlohmann::json::exception &error) {
        throw InvalidArgument("Invalid PTX reflection in " + path.string() + ": " + error.what());
    }
}

}  // namespace libcbct::detail
