#pragma once

#if defined(LIBCBCT_WITH_VULKAN)

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "Common/Api.h"
#include "Common/Exception.h"

namespace libcbct {

class VulkanUnavailable : public Error {
public:
    using Error::Error;
};

struct VulkanDeviceInfo {
    int index;
    std::string name;
    bool shaderInt64;
    bool bufferFloat32AtomicAdd;
    std::uint64_t maxStorageBufferBytes;
};

namespace detail {
struct VulkanDeviceState;
struct VulkanBufferState;
struct VulkanKernelState;
struct VulkanBatchState;
}  // namespace detail

enum class VulkanBufferMemory { Device, Upload, Readback };

// Buffers and kernels retain their device. Batches retain every recorded resource.
class LIBCBCT_API VulkanComputeBuffer {
public:
    std::size_t size() const;
    bool operator==(const VulkanComputeBuffer &) const = default;
    // Mapped staging buffers only. Complete submitted work before host access.
    void write(std::span<const std::byte> data, std::size_t offset = 0) const;
    void read(std::span<std::byte> data, std::size_t offset = 0) const;

private:
    friend class VulkanComputeContext;
    friend class VulkanComputeBatch;
    explicit VulkanComputeBuffer(std::shared_ptr<detail::VulkanBufferState> state);
    std::shared_ptr<detail::VulkanBufferState> state_;
};

class LIBCBCT_API VulkanComputeKernel {
private:
    friend class VulkanComputeContext;
    friend class VulkanComputeBatch;
    explicit VulkanComputeKernel(std::shared_ptr<detail::VulkanKernelState> state);
    std::shared_ptr<detail::VulkanKernelState> state_;
};

// One compute queue; calls on a batch must be externally serialized.
// Transfers and dispatches have memory barriers between them. Submission waits
// for a fence, making staging-buffer reuse and exception cleanup safe.
class LIBCBCT_API VulkanComputeBatch {
public:
    ~VulkanComputeBatch();
    VulkanComputeBatch(VulkanComputeBatch &&) noexcept;
    VulkanComputeBatch &operator=(VulkanComputeBatch &&) noexcept;
    VulkanComputeBatch(const VulkanComputeBatch &) = delete;
    VulkanComputeBatch &operator=(const VulkanComputeBatch &) = delete;

    void copy(const VulkanComputeBuffer &source, const VulkanComputeBuffer &destination, std::size_t bytes,
              std::size_t sourceOffset = 0, std::size_t destinationOffset = 0);
    void fill(const VulkanComputeBuffer &buffer, std::uint32_t value = 0);
    // Bind storage buffers consecutively at set 0, bindings 0..N-1.
    void dispatch(const VulkanComputeKernel &kernel, std::span<const VulkanComputeBuffer> buffers,
                  std::span<const std::byte> pushConstants, std::array<std::uint32_t, 3> groups);
    void submitAndWait();

private:
    friend class VulkanComputeContext;
    explicit VulkanComputeBatch(std::unique_ptr<detail::VulkanBatchState> state);
    std::unique_ptr<detail::VulkanBatchState> state_;
};

class LIBCBCT_API VulkanComputeContext {
public:
    // -1 chooses a compute-capable device, preferring a discrete GPU.
    explicit VulkanComputeContext(int deviceIndex = -1);
    static std::vector<VulkanDeviceInfo> availableDevices();
    const VulkanDeviceInfo &deviceInfo() const;
    VulkanComputeBuffer createBuffer(std::size_t bytes, VulkanBufferMemory memory = VulkanBufferMemory::Device) const;
    // This foundation accepts SPIR-V 1.3 (Vulkan 1.1) or earlier. The caller
    // supplies the descriptor/push-constant layout matching that compilation.
    VulkanComputeKernel createKernel(const std::filesystem::path &spirv, std::uint32_t storageBindings,
                                     std::uint32_t pushConstantBytes,
                                     std::array<std::uint32_t, 3> localSize = { 16, 16, 1 },
                                     const std::string &entryPoint = "main") const;
    VulkanComputeBatch createBatch() const;

private:
    std::shared_ptr<detail::VulkanDeviceState> state_;
};

}  // namespace libcbct

#endif  // LIBCBCT_WITH_VULKAN
