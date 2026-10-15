#include "VulkanCompute.h"

#include <vulkan/vulkan.hpp>
#include <vk_mem_alloc.h>

#include <algorithm>
#include <cstring>
#include <fstream>
#include <limits>
#include <mutex>
#include <string_view>
#include <utility>

namespace libcbct {
namespace {

void checkVma(VkResult result, const char *operation) {
    if (result != VK_SUCCESS) {
        throw Error(std::string(operation) + ": " + vk::to_string(static_cast<vk::Result>(result)));
    }
}

vk::UniqueInstance createInstance() {
    std::vector<const char *> extensions;
    vk::InstanceCreateFlags flags;
    for (const auto &extension : vk::enumerateInstanceExtensionProperties()) {
        if (std::string_view(extension.extensionName.data()) == VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME) {
            extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
            flags |= vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR;
        }
    }
    const vk::ApplicationInfo app("libcbct", 0, "libcbct", 0, VK_API_VERSION_1_1);
    try {
        return vk::createInstanceUnique(
            vk::InstanceCreateInfo({}, &app).setFlags(flags).setPEnabledExtensionNames(extensions));
    } catch (const vk::SystemError &error) {
        if (error.code().value() == static_cast<int>(vk::Result::eErrorIncompatibleDriver)) {
            throw VulkanUnavailable("A Vulkan 1.1 compute driver is required");
        }
        throw;
    }
}

std::uint32_t computeFamily(vk::PhysicalDevice device) {
    const auto families = device.getQueueFamilyProperties();
    for (std::uint32_t i = 0; i < families.size(); ++i) {
        if (families[i].queueCount && (families[i].queueFlags & vk::QueueFlagBits::eCompute)) return i;
    }
    return std::numeric_limits<std::uint32_t>::max();
}

bool hasExtension(vk::PhysicalDevice device, std::string_view name) {
    const auto extensions = device.enumerateDeviceExtensionProperties();
    return std::any_of(extensions.begin(), extensions.end(),
                       [&](const auto &extension) { return std::string_view(extension.extensionName.data()) == name; });
}

VulkanDeviceInfo describeDevice(vk::PhysicalDevice device, int index) {
    const auto properties = device.getProperties();
    bool atomicAdd = false;
    if (hasExtension(device, VK_EXT_SHADER_ATOMIC_FLOAT_EXTENSION_NAME)) {
        vk::PhysicalDeviceShaderAtomicFloatFeaturesEXT atomics;
        vk::PhysicalDeviceFeatures2 features;
        features.pNext = &atomics;
        device.getFeatures2(&features);
        atomicAdd = atomics.shaderBufferFloat32AtomicAdd;
    }
    return { index, properties.deviceName.data(), static_cast<bool>(device.getFeatures().shaderInt64), atomicAdd,
             properties.limits.maxStorageBufferRange };
}

void checkRange(std::size_t size, std::size_t offset, std::size_t bytes) {
    if (offset > size || bytes > size - offset) throw InvalidArgument("Vulkan buffer access is out of range");
}

}  // namespace

namespace detail {

struct VulkanDeviceState {
    vk::UniqueInstance instance;
    vk::PhysicalDevice physical;
    vk::PhysicalDeviceProperties properties;
    VulkanDeviceInfo info;
    std::uint32_t family = 0;
    vk::UniqueDevice device;
    vk::Queue queue;
    VmaAllocator allocator = nullptr;
    std::mutex queueMutex;

    ~VulkanDeviceState() {
        // All resource/batch objects retain this state and are destroyed first.
        if (allocator) vmaDestroyAllocator(allocator);
    }
};

struct VulkanBufferState {
    std::shared_ptr<VulkanDeviceState> device;
    VkBuffer buffer = VK_NULL_HANDLE;
    VmaAllocation allocation = nullptr;
    void *mapped = nullptr;
    std::size_t bytes = 0;

    ~VulkanBufferState() {
        if (buffer) vmaDestroyBuffer(device->allocator, buffer, allocation);
    }
};

struct VulkanKernelState {
    std::shared_ptr<VulkanDeviceState> device;
    std::uint32_t bindings;
    std::uint32_t pushBytes;
    vk::UniqueShaderModule shader;
    vk::UniqueDescriptorSetLayout descriptors;
    vk::UniquePipelineLayout layout;
    vk::UniquePipeline pipeline;
};

struct VulkanBatchState {
    std::shared_ptr<VulkanDeviceState> device;
    std::vector<std::shared_ptr<VulkanBufferState>> buffers;
    std::vector<std::shared_ptr<VulkanKernelState>> kernels;
    vk::UniqueCommandPool pool;
    vk::CommandBuffer command;
    std::vector<vk::UniqueDescriptorPool> descriptors;
    vk::UniqueFence fence;
    std::uint32_t setsAllocated = 0;
    bool submitted = false;
    bool inFlight = false;

    ~VulkanBatchState() {
        if (inFlight) {
            // Do not release resources still referenced by the queue on an exception.
            std::lock_guard lock(device->queueMutex);
            try {
                device->device->waitIdle();
            } catch (...) {
            }
        }
    }

    void requireRecording() const {
        if (submitted) throw InvalidArgument("A Vulkan batch can only be submitted once");
    }

    void barrier() const {
        const vk::MemoryBarrier memory(vk::AccessFlagBits::eMemoryWrite,
                                       vk::AccessFlagBits::eMemoryRead | vk::AccessFlagBits::eMemoryWrite);
        command.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands, {},
                                memory, {}, {});
    }
};

}  // namespace detail

std::vector<VulkanDeviceInfo> VulkanComputeContext::availableDevices() {
    const auto instance = createInstance();
    const auto devices = instance->enumeratePhysicalDevices();
    std::vector<VulkanDeviceInfo> result;
    for (std::size_t i = 0; i < devices.size(); ++i) {
        if (devices[i].getProperties().apiVersion >= VK_API_VERSION_1_1
            && computeFamily(devices[i]) != std::numeric_limits<std::uint32_t>::max()) {
            result.push_back(describeDevice(devices[i], static_cast<int>(i)));
        }
    }
    return result;
}

VulkanComputeContext::VulkanComputeContext(int deviceIndex) {
    if (deviceIndex < -1) throw InvalidArgument("Vulkan device index must be -1 or non-negative");
    auto state = std::make_shared<detail::VulkanDeviceState>();
    state->instance = createInstance();
    const auto devices = state->instance->enumeratePhysicalDevices();
    int selected = -1;
    for (std::size_t i = 0; i < devices.size(); ++i) {
        if (deviceIndex >= 0 && static_cast<int>(i) != deviceIndex) continue;
        const auto properties = devices[i].getProperties();
        if (properties.apiVersion < VK_API_VERSION_1_1
            || computeFamily(devices[i]) == std::numeric_limits<std::uint32_t>::max())
            continue;
        selected = static_cast<int>(i);
        if (deviceIndex >= 0 || properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu) break;
    }
    if (selected < 0) throw VulkanUnavailable("No Vulkan 1.1 compute device matches the requested index");
    state->physical = devices[selected];
    state->properties = state->physical.getProperties();
    state->info = describeDevice(state->physical, selected);
    state->family = computeFamily(state->physical);
    std::vector<const char *> extensions;
    if (hasExtension(state->physical, "VK_KHR_portability_subset")) extensions.push_back("VK_KHR_portability_subset");
    // Optional support is reported, but no FDK kernel requires FP32 atomics.
    const float priority = 1.0f;
    const vk::DeviceQueueCreateInfo queue({}, state->family, 1, &priority);
    vk::PhysicalDeviceFeatures features;
    features.shaderInt64 = state->info.shaderInt64;
    state->device = state->physical.createDeviceUnique(
        vk::DeviceCreateInfo({}, queue).setPEnabledExtensionNames(extensions).setPEnabledFeatures(&features));
    state->queue = state->device->getQueue(state->family, 0);
    VmaVulkanFunctions functions{};
    functions.vkGetInstanceProcAddr = &vkGetInstanceProcAddr;
    functions.vkGetDeviceProcAddr = &vkGetDeviceProcAddr;
    VmaAllocatorCreateInfo allocator{};
    allocator.instance = *state->instance;
    allocator.physicalDevice = state->physical;
    allocator.device = *state->device;
    allocator.vulkanApiVersion = VK_API_VERSION_1_1;
    allocator.pVulkanFunctions = &functions;
    checkVma(vmaCreateAllocator(&allocator, &state->allocator), "vmaCreateAllocator");
    state_ = std::move(state);
}

const VulkanDeviceInfo &VulkanComputeContext::deviceInfo() const {
    if (!state_) throw InvalidArgument("Moved-from Vulkan context");
    return state_->info;
}

VulkanComputeBuffer::VulkanComputeBuffer(std::shared_ptr<detail::VulkanBufferState> state)
    : state_(std::move(state)) {
}

std::size_t VulkanComputeBuffer::size() const {
    if (!state_) throw InvalidArgument("Moved-from Vulkan buffer");
    return state_->bytes;
}

void VulkanComputeBuffer::write(std::span<const std::byte> data, std::size_t offset) const {
    checkRange(size(), offset, data.size());
    if (!state_->mapped) throw InvalidArgument("Vulkan buffer is not host visible");
    if (data.empty()) return;
    std::memcpy(static_cast<std::byte *>(state_->mapped) + offset, data.data(), data.size());
    checkVma(vmaFlushAllocation(state_->device->allocator, state_->allocation, offset, data.size()),
             "vmaFlushAllocation");
}

void VulkanComputeBuffer::read(std::span<std::byte> data, std::size_t offset) const {
    checkRange(size(), offset, data.size());
    if (!state_->mapped) throw InvalidArgument("Vulkan buffer is not host visible");
    if (data.empty()) return;
    checkVma(vmaInvalidateAllocation(state_->device->allocator, state_->allocation, offset, data.size()),
             "vmaInvalidateAllocation");
    std::memcpy(data.data(), static_cast<const std::byte *>(state_->mapped) + offset, data.size());
}

VulkanComputeBuffer VulkanComputeContext::createBuffer(std::size_t bytes, VulkanBufferMemory memory) const {
    if (!state_) throw InvalidArgument("Moved-from Vulkan context");
    if (!bytes) throw InvalidArgument("Vulkan buffers must be non-empty");
    auto buffer = std::make_shared<detail::VulkanBufferState>();
    buffer->device = state_;
    buffer->bytes = bytes;
    const VkBufferCreateInfo info =
        vk::BufferCreateInfo({}, bytes,
                             vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferSrc
                                 | vk::BufferUsageFlagBits::eTransferDst);
    VmaAllocationCreateInfo allocation{};
    allocation.usage =
        memory == VulkanBufferMemory::Device ? VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE : VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
    if (memory != VulkanBufferMemory::Device) {
        allocation.flags =
            VMA_ALLOCATION_CREATE_MAPPED_BIT
            | (memory == VulkanBufferMemory::Upload ? VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
                                                    : VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT);
    }
    VmaAllocationInfo details{};
    checkVma(vmaCreateBuffer(state_->allocator, &info, &allocation, &buffer->buffer, &buffer->allocation, &details),
             "vmaCreateBuffer");
    buffer->mapped = details.pMappedData;
    return VulkanComputeBuffer(std::move(buffer));
}

VulkanComputeKernel::VulkanComputeKernel(std::shared_ptr<detail::VulkanKernelState> state)
    : state_(std::move(state)) {
}

VulkanComputeKernel VulkanComputeContext::createKernel(const std::filesystem::path &path, std::uint32_t storageBindings,
                                                       std::uint32_t pushConstantBytes,
                                                       std::array<std::uint32_t, 3> localSize,
                                                       const std::string &entryPoint) const {
    if (!state_) throw InvalidArgument("Moved-from Vulkan context");
    const auto &limits = state_->properties.limits;
    if (!storageBindings || storageBindings > 16 || storageBindings > limits.maxPerStageDescriptorStorageBuffers
        || storageBindings > limits.maxDescriptorSetStorageBuffers || pushConstantBytes % 4
        || pushConstantBytes > limits.maxPushConstantsSize) {
        throw InvalidArgument("Vulkan compute layout exceeds device limits");
    }
    std::uint64_t threads = 1;
    for (std::size_t i = 0; i < 3; ++i) {
        if (!localSize[i] || localSize[i] > limits.maxComputeWorkGroupSize[i])
            throw InvalidArgument("Vulkan local size exceeds device limits");
        threads *= localSize[i];
    }
    if (threads > limits.maxComputeWorkGroupInvocations || entryPoint.empty()
        || entryPoint.find('\0') != std::string::npos) {
        throw InvalidArgument("Invalid Vulkan compute entry point or local size");
    }
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) throw IOError("Cannot open SPIR-V file: " + path.string());
    const auto bytes = input.tellg();
    if (bytes < 20 || bytes % 4 != 0) throw InvalidArgument("Invalid SPIR-V file size: " + path.string());
    std::vector<std::uint32_t> code(static_cast<std::size_t>(bytes) / 4);
    input.seekg(0);
    if (!input.read(reinterpret_cast<char *>(code.data()), bytes))
        throw IOError("Cannot read SPIR-V file: " + path.string());
    if (code[0] != 0x07230203 || code[1] > 0x00010300)
        throw InvalidArgument("Expected SPIR-V 1.3 or earlier: " + path.string());
    auto kernel = std::make_shared<detail::VulkanKernelState>();
    kernel->device = state_;
    kernel->bindings = storageBindings;
    kernel->pushBytes = pushConstantBytes;
    kernel->shader =
        state_->device->createShaderModuleUnique(vk::ShaderModuleCreateInfo({}, code.size() * 4, code.data()));
    std::vector<vk::DescriptorSetLayoutBinding> bindings;
    for (std::uint32_t i = 0; i < storageBindings; ++i)
        bindings.emplace_back(i, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute);
    kernel->descriptors =
        state_->device->createDescriptorSetLayoutUnique(vk::DescriptorSetLayoutCreateInfo({}, bindings));
    const vk::PushConstantRange push(vk::ShaderStageFlagBits::eCompute, 0, pushConstantBytes);
    vk::PipelineLayoutCreateInfo layout({}, *kernel->descriptors);
    if (pushConstantBytes) layout.setPushConstantRanges(push);
    kernel->layout = state_->device->createPipelineLayoutUnique(layout);
    const vk::PipelineShaderStageCreateInfo shader({}, vk::ShaderStageFlagBits::eCompute, *kernel->shader,
                                                   entryPoint.c_str());
    kernel->pipeline =
        state_->device->createComputePipelineUnique({}, vk::ComputePipelineCreateInfo({}, shader, *kernel->layout))
            .value;
    return VulkanComputeKernel(std::move(kernel));
}

VulkanComputeBatch::VulkanComputeBatch(std::unique_ptr<detail::VulkanBatchState> state)
    : state_(std::move(state)) {
}
VulkanComputeBatch::~VulkanComputeBatch() = default;
VulkanComputeBatch::VulkanComputeBatch(VulkanComputeBatch &&) noexcept = default;
VulkanComputeBatch &VulkanComputeBatch::operator=(VulkanComputeBatch &&) noexcept = default;

VulkanComputeBatch VulkanComputeContext::createBatch() const {
    if (!state_) throw InvalidArgument("Moved-from Vulkan context");
    auto batch = std::make_unique<detail::VulkanBatchState>();
    batch->device = state_;
    batch->pool = state_->device->createCommandPoolUnique(
        vk::CommandPoolCreateInfo(vk::CommandPoolCreateFlagBits::eTransient, state_->family));
    batch->command =
        state_->device
            ->allocateCommandBuffers(vk::CommandBufferAllocateInfo(*batch->pool, vk::CommandBufferLevel::ePrimary, 1))
            .front();
    batch->fence = state_->device->createFenceUnique({});
    batch->command.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));
    batch->barrier();
    return VulkanComputeBatch(std::move(batch));
}

void VulkanComputeBatch::copy(const VulkanComputeBuffer &source, const VulkanComputeBuffer &destination,
                              std::size_t bytes, std::size_t sourceOffset, std::size_t destinationOffset) {
    if (!state_) throw InvalidArgument("Moved-from Vulkan batch");
    state_->requireRecording();
    checkRange(source.size(), sourceOffset, bytes);
    checkRange(destination.size(), destinationOffset, bytes);
    if (source.state_->device != state_->device || destination.state_->device != state_->device
        || source.state_ == destination.state_) {
        throw InvalidArgument("Vulkan copies require distinct buffers from this context");
    }
    if (!bytes) return;
    state_->buffers.push_back(source.state_);
    state_->buffers.push_back(destination.state_);
    state_->command.copyBuffer(source.state_->buffer, destination.state_->buffer,
                               vk::BufferCopy(sourceOffset, destinationOffset, bytes));
    state_->barrier();
}

void VulkanComputeBatch::fill(const VulkanComputeBuffer &buffer, std::uint32_t value) {
    if (!state_) throw InvalidArgument("Moved-from Vulkan batch");
    state_->requireRecording();
    if (!buffer.state_ || buffer.state_->device != state_->device || buffer.size() % 4)
        throw InvalidArgument("Vulkan fill requires a buffer from this context with a size divisible by four");
    state_->buffers.push_back(buffer.state_);
    state_->command.fillBuffer(buffer.state_->buffer, 0, buffer.size(), value);
    state_->barrier();
}

void VulkanComputeBatch::dispatch(const VulkanComputeKernel &kernel, std::span<const VulkanComputeBuffer> buffers,
                                  std::span<const std::byte> pushConstants, std::array<std::uint32_t, 3> groups) {
    if (!state_) throw InvalidArgument("Moved-from Vulkan batch");
    state_->requireRecording();
    if (!kernel.state_ || kernel.state_->device != state_->device || buffers.size() != kernel.state_->bindings
        || pushConstants.size() != kernel.state_->pushBytes) {
        throw InvalidArgument("Vulkan kernel context or argument layout mismatch");
    }
    for (std::size_t i = 0; i < 3; ++i) {
        if (!groups[i] || groups[i] > state_->device->properties.limits.maxComputeWorkGroupCount[i])
            throw InvalidArgument("Vulkan dispatch exceeds device limits");
    }
    std::vector<vk::DescriptorBufferInfo> infos;
    for (const auto &buffer : buffers) {
        if (!buffer.state_ || buffer.state_->device != state_->device
            || buffer.size() > state_->device->info.maxStorageBufferBytes) {
            throw InvalidArgument("Vulkan storage buffer is incompatible with the context or device range limit");
        }
        infos.emplace_back(buffer.state_->buffer, 0, buffer.size());
    }
    // Grow pools in bounded chunks instead of imposing a limit on batch length.
    if (state_->setsAllocated % 128 == 0) {
        const vk::DescriptorPoolSize size(vk::DescriptorType::eStorageBuffer, 128 * 16);
        state_->descriptors.push_back(
            state_->device->device->createDescriptorPoolUnique(vk::DescriptorPoolCreateInfo({}, 128, size)));
    }
    const auto set = state_->device->device
                         ->allocateDescriptorSets(
                             vk::DescriptorSetAllocateInfo(*state_->descriptors.back(), *kernel.state_->descriptors))
                         .front();
    ++state_->setsAllocated;
    std::vector<vk::WriteDescriptorSet> writes;
    for (std::uint32_t i = 0; i < infos.size(); ++i) {
        writes.emplace_back(set, i, 0, 1, vk::DescriptorType::eStorageBuffer, nullptr, &infos[i]);
        state_->buffers.push_back(buffers[i].state_);
    }
    state_->kernels.push_back(kernel.state_);
    state_->device->device->updateDescriptorSets(writes, {});
    state_->command.bindPipeline(vk::PipelineBindPoint::eCompute, *kernel.state_->pipeline);
    state_->command.bindDescriptorSets(vk::PipelineBindPoint::eCompute, *kernel.state_->layout, 0, set, {});
    if (!pushConstants.empty())
        state_->command.pushConstants(*kernel.state_->layout, vk::ShaderStageFlagBits::eCompute, 0,
                                      static_cast<std::uint32_t>(pushConstants.size()), pushConstants.data());
    state_->command.dispatch(groups[0], groups[1], groups[2]);
    state_->barrier();
}

void VulkanComputeBatch::submitAndWait() {
    if (!state_) throw InvalidArgument("Moved-from Vulkan batch");
    state_->requireRecording();
    const vk::MemoryBarrier host(vk::AccessFlagBits::eMemoryWrite, vk::AccessFlagBits::eHostRead);
    state_->command.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eHost, {}, host,
                                    {}, {});
    state_->command.end();
    state_->submitted = true;
    std::lock_guard lock(state_->device->queueMutex);
    state_->device->queue.submit(vk::SubmitInfo({}, {}, state_->command), *state_->fence);
    state_->inFlight = true;
    const auto result =
        state_->device->device->waitForFences(*state_->fence, true, std::numeric_limits<std::uint64_t>::max());
    if (result != vk::Result::eSuccess) throw Error("Vulkan compute fence: " + vk::to_string(result));
    state_->inFlight = false;
}

}  // namespace libcbct
