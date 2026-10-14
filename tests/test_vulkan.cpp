#include <gtest/gtest.h>

#include <array>
#include <cstring>
#include <string_view>
#include <vector>

#include <vulkan/vulkan.hpp>
#include <vk_mem_alloc.h>

namespace {

struct Allocator {
    VmaAllocator value = nullptr;
    ~Allocator() {
        if (value) vmaDestroyAllocator(value);
    }
};

struct Buffer {
    VmaAllocator allocator;
    VkBuffer value = VK_NULL_HANDLE;
    VmaAllocation allocation = nullptr;
    ~Buffer() {
        if (value) vmaDestroyBuffer(allocator, value, allocation);
    }
};

TEST(VulkanDependencies, AllocatesMappedBufferUsingHppDeviceAndVma) {
    vk::UniqueInstance instance;
    try {
        std::vector<const char *> extensions;
        vk::InstanceCreateFlags flags;
#if defined(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME)
        for (const auto &extension : vk::enumerateInstanceExtensionProperties()) {
            if (std::string_view(extension.extensionName.data()) == VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME) {
                extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
                flags |= vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR;
            }
        }
#endif
        const vk::ApplicationInfo app("libcbct_vulkan_tests", 0, "libcbct", 0, VK_API_VERSION_1_0);
        instance = vk::createInstanceUnique(vk::InstanceCreateInfo(
            flags, &app, 0, nullptr, static_cast<uint32_t>(extensions.size()), extensions.data()));
    } catch (const vk::SystemError &error) {
        if (error.code().value() == static_cast<int>(vk::Result::eErrorIncompatibleDriver)) {
            GTEST_SKIP() << "No compatible Vulkan driver available: " << error.what();
        }
        throw;
    }

    vk::PhysicalDevice physicalDevice;
    uint32_t queueFamily = 0;
    for (const auto &candidate : instance->enumeratePhysicalDevices()) {
        const auto families = candidate.getQueueFamilyProperties();
        for (uint32_t i = 0; i < families.size(); ++i) {
            if (families[i].queueCount && (families[i].queueFlags & vk::QueueFlagBits::eCompute)) {
                physicalDevice = candidate;
                queueFamily = i;
                break;
            }
        }
        if (physicalDevice) break;
    }
    if (!physicalDevice) GTEST_SKIP() << "No Vulkan device with a compute queue available";

    std::vector<const char *> deviceExtensions;
    for (const auto &extension : physicalDevice.enumerateDeviceExtensionProperties()) {
        // MoltenVK requires this extension when it is advertised.
        if (std::string_view(extension.extensionName.data()) == "VK_KHR_portability_subset") {
            deviceExtensions.push_back("VK_KHR_portability_subset");
        }
    }
    const float priority = 1.0f;
    const vk::DeviceQueueCreateInfo queue({}, queueFamily, 1, &priority);
    const auto device = physicalDevice.createDeviceUnique(vk::DeviceCreateInfo(
        {}, 1, &queue, 0, nullptr, static_cast<uint32_t>(deviceExtensions.size()), deviceExtensions.data()));

    VmaVulkanFunctions functions{};
    functions.vkGetInstanceProcAddr = &vkGetInstanceProcAddr;
    functions.vkGetDeviceProcAddr = &vkGetDeviceProcAddr;
    VmaAllocatorCreateInfo allocatorInfo{};
    allocatorInfo.instance = static_cast<VkInstance>(*instance);
    allocatorInfo.physicalDevice = static_cast<VkPhysicalDevice>(physicalDevice);
    allocatorInfo.device = static_cast<VkDevice>(*device);
    allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_0;
    allocatorInfo.pVulkanFunctions = &functions;
    Allocator allocator;
    ASSERT_EQ(vmaCreateAllocator(&allocatorInfo, &allocator.value), VK_SUCCESS);

    const std::array<float, 4> expected{ 1.0f, 2.0f, 3.0f, 4.0f };
    const vk::BufferCreateInfo hppBuffer({}, sizeof(expected), vk::BufferUsageFlagBits::eStorageBuffer,
                                         vk::SharingMode::eExclusive);
    const VkBufferCreateInfo bufferInfo = hppBuffer;
    VmaAllocationCreateInfo allocationInfo{};
    allocationInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocationInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
    Buffer buffer{ allocator.value };
    VmaAllocationInfo allocation{};
    ASSERT_EQ(
        vmaCreateBuffer(allocator.value, &bufferInfo, &allocationInfo, &buffer.value, &buffer.allocation, &allocation),
        VK_SUCCESS);
    ASSERT_NE(allocation.pMappedData, nullptr);
    std::memcpy(allocation.pMappedData, expected.data(), sizeof(expected));
    ASSERT_EQ(vmaFlushAllocation(allocator.value, buffer.allocation, 0, sizeof(expected)), VK_SUCCESS);
    ASSERT_EQ(vmaInvalidateAllocation(allocator.value, buffer.allocation, 0, sizeof(expected)), VK_SUCCESS);
    std::array<float, 4> actual{};
    std::memcpy(actual.data(), allocation.pMappedData, sizeof(actual));
    EXPECT_EQ(actual, expected);
}

}  // namespace
