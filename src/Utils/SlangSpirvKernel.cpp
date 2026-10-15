#include "SlangSpirvKernel.h"

#include <cstring>
#include <fstream>
#include <map>
#include <optional>
#include <set>
#include <utility>

#include <nlohmann/json.hpp>

namespace libcbct {
namespace {

struct UniformLocation {
    std::size_t offset;
    std::size_t size;
};

void collectFields(const nlohmann::json &type, std::size_t base, const std::string &prefix, std::size_t pushSize,
                   std::map<std::string, UniformLocation, std::less<>> &uniforms,
                   std::map<std::string, std::uint32_t, std::less<>> &buffers) {
    if (type.at("kind") != "struct") throw InvalidArgument("Slang SPIR-V parameters must be a structure");
    for (const auto &field : type.at("fields")) {
        const auto name = prefix + field.at("name").get<std::string>();
        const auto &binding = field.at("binding");
        const auto &fieldType = field.at("type");
        if (binding.at("kind") == "descriptorTableSlot") {
            if (fieldType.at("kind") != "resource" || fieldType.at("baseShape") != "structuredBuffer"
                || binding.value("space", 0) != 0 || binding.value("count", 1) != 1
                || !buffers.emplace(name, binding.at("index").get<std::uint32_t>()).second) {
                throw InvalidArgument("Unsupported Slang SPIR-V resource layout: " + name);
            }
        } else if (binding.at("kind") == "uniform") {
            const auto offset = binding.at("offset").get<std::size_t>();
            const auto size = binding.at("size").get<std::size_t>();
            if (base > pushSize || offset > pushSize - base || size > pushSize - base - offset) {
                throw InvalidArgument("Slang SPIR-V uniform is outside the push-constant block: " + name);
            }
            if (fieldType.at("kind") == "struct") {
                collectFields(fieldType, base + offset, name + ".", pushSize, uniforms, buffers);
            } else if (!uniforms.emplace(name, UniformLocation{ base + offset, size }).second) {
                throw InvalidArgument("Duplicate Slang SPIR-V uniform: " + name);
            }
        } else {
            throw InvalidArgument("Unsupported Slang SPIR-V binding: " + name);
        }
    }
}

}  // namespace

struct SlangSpirvKernel::State {
    VulkanComputeKernel kernel;
    std::vector<std::byte> constants;
    std::map<std::string, UniformLocation, std::less<>> uniforms;
    std::map<std::string, std::uint32_t, std::less<>> bufferIndices;
    std::vector<std::optional<VulkanComputeBuffer>> buffers;
    std::set<std::string, std::less<>> assignedUniforms;

    explicit State(VulkanComputeKernel value)
        : kernel(std::move(value)) {
    }
};

SlangSpirvKernel::SlangSpirvKernel(const VulkanComputeContext &context, const std::filesystem::path &directory,
                                   const std::string &entryPoint) {
    const auto path = directory / (entryPoint + ".spirv.json");
    std::ifstream input(path);
    if (!input) throw IOError("Cannot open SPIR-V reflection file: " + path.string());
    try {
        const auto reflection = nlohmann::json::parse(input);
        if (!reflection.at("parameters").empty() || reflection.at("entryPoints").size() != 1) {
            throw InvalidArgument("Slang SPIR-V adapter requires one entry point and no global parameters");
        }
        const auto &entry = reflection.at("entryPoints").at(0);
        if (entry.at("name") != entryPoint || entry.at("stage") != "compute"
            || entry.at("threadGroupSize") != nlohmann::json::array({ 16, 16, 1 }) || entry.at("parameters").size() != 2
            || !entry.at("bindings").empty()) {
            throw InvalidArgument("Unexpected Slang SPIR-V entry point or thread-group layout: " + entryPoint);
        }
        const auto &parameters = entry.at("parameters");
        if (parameters.at(0).at("semanticName") != "SV_DISPATCHTHREADID" || parameters.at(1).at("name") != "params") {
            throw InvalidArgument("Slang SPIR-V adapter requires dispatch IDs and uniform params");
        }
        const auto &params = parameters.at(1);
        std::uint32_t pushSize = 0, bufferCount = 0;
        for (const auto &binding : params.at("bindings")) {
            if (binding.at("kind") == "uniform" && binding.at("offset") == 0) {
                pushSize = binding.at("size").get<std::uint32_t>();
            } else if (binding.at("kind") == "descriptorTableSlot" && binding.at("index") == 0
                       && binding.value("space", 0) == 0) {
                bufferCount = binding.value("count", std::uint32_t{ 1 });
            } else {
                throw InvalidArgument("Unsupported Slang SPIR-V parameter block: " + entryPoint);
            }
        }
        if (!pushSize || !bufferCount)
            throw InvalidArgument("Slang SPIR-V requires storage buffers and push constants");
        auto state =
            std::make_unique<State>(context.createKernel(directory / (entryPoint + ".spv"), bufferCount, pushSize));
        state->constants.resize(pushSize);
        state->buffers.resize(bufferCount);
        collectFields(params.at("type"), 0, "", pushSize, state->uniforms, state->bufferIndices);
        std::set<std::uint32_t> indices;
        for (const auto &[name, index] : state->bufferIndices) indices.insert(index);
        if (state->bufferIndices.size() != bufferCount || indices.size() != bufferCount
            || *indices.rbegin() >= bufferCount) {
            throw InvalidArgument("Slang SPIR-V storage bindings must be consecutive from zero");
        }
        state_ = std::move(state);
    } catch (const nlohmann::json::exception &error) {
        throw InvalidArgument("Invalid SPIR-V reflection in " + path.string() + ": " + error.what());
    }
}

SlangSpirvKernel::~SlangSpirvKernel() = default;
SlangSpirvKernel::SlangSpirvKernel(SlangSpirvKernel &&) noexcept = default;
SlangSpirvKernel &SlangSpirvKernel::operator=(SlangSpirvKernel &&) noexcept = default;

void SlangSpirvKernel::setBuffer(std::string_view name, const VulkanComputeBuffer &buffer) {
    if (!state_) throw InvalidArgument("Moved-from Slang SPIR-V kernel");
    const auto found = state_->bufferIndices.find(name);
    if (found == state_->bufferIndices.end())
        throw InvalidArgument("Unknown Slang SPIR-V buffer: " + std::string(name));
    state_->buffers[found->second] = buffer;
}

void SlangSpirvKernel::setUniformBytes(std::string_view name, std::span<const std::byte> data) {
    if (!state_) throw InvalidArgument("Moved-from Slang SPIR-V kernel");
    const auto found = state_->uniforms.find(name);
    if (found == state_->uniforms.end() || data.size() != found->second.size) {
        throw InvalidArgument("Slang SPIR-V uniform name or size mismatch: " + std::string(name));
    }
    std::memcpy(state_->constants.data() + found->second.offset, data.data(), data.size());
    state_->assignedUniforms.insert(std::string(name));
}

void SlangSpirvKernel::dispatch(VulkanComputeBatch &batch, std::array<std::uint32_t, 3> groups) const {
    if (!state_) throw InvalidArgument("Moved-from Slang SPIR-V kernel");
    if (state_->assignedUniforms.size() != state_->uniforms.size())
        throw InvalidArgument("Slang SPIR-V uniforms have not all been assigned");
    std::vector<VulkanComputeBuffer> buffers;
    for (const auto &buffer : state_->buffers) {
        if (!buffer) throw InvalidArgument("Slang SPIR-V buffers have not all been assigned");
        buffers.push_back(*buffer);
    }
    batch.dispatch(state_->kernel, buffers, state_->constants, groups);
}

}  // namespace libcbct
