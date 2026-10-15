#pragma once

#if defined(LIBCBCT_WITH_VULKAN)

#include <memory>
#include <string_view>
#include <type_traits>

#include "VulkanCompute.h"

namespace libcbct {

// Storage bindings and uniform offsets come from the matching Slang reflection.
// The initial adapter supports one compute entry point, one uniform structure,
// set-0 structured buffers, and a push-constant block. It rejects other layouts.
class LIBCBCT_API SlangSpirvKernel {
public:
    SlangSpirvKernel(const VulkanComputeContext &context, const std::filesystem::path &directory,
                     const std::string &entryPoint);
    ~SlangSpirvKernel();
    SlangSpirvKernel(SlangSpirvKernel &&) noexcept;
    SlangSpirvKernel &operator=(SlangSpirvKernel &&) noexcept;
    SlangSpirvKernel(const SlangSpirvKernel &) = delete;
    SlangSpirvKernel &operator=(const SlangSpirvKernel &) = delete;

    void setBuffer(std::string_view name, const VulkanComputeBuffer &buffer);
    void setUniformBytes(std::string_view name, std::span<const std::byte> data);
    template <typename T>
    void setUniform(std::string_view name, const T &value) {
        static_assert(std::is_trivially_copyable_v<T>);
        setUniformBytes(name, std::as_bytes(std::span(&value, 1)));
    }
    void dispatch(VulkanComputeBatch &batch, std::array<std::uint32_t, 3> groups) const;

private:
    struct State;
    std::unique_ptr<State> state_;
};

}  // namespace libcbct

#endif
