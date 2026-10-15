#pragma once

#if defined(LIBCBCT_WITH_VULKAN)

#include "VulkanCompute.h"

namespace libcbct {

// Batched, unnormalized FP32 transforms of power-of-two detector rows.
// Forward input is real; inverse uses distinct complex input/output buffers.
// All work stays on GPU.
class LIBCBCT_API VulkanFftPlan {
public:
    VulkanFftPlan(const VulkanComputeContext &context, const std::filesystem::path &kernelDirectory, int length,
                  int rows);
    ~VulkanFftPlan();
    VulkanFftPlan(const VulkanFftPlan &) = delete;
    VulkanFftPlan &operator=(const VulkanFftPlan &) = delete;
    void forward(VulkanComputeBatch &batch, const VulkanComputeBuffer &realInput, const VulkanComputeBuffer &spectrum);
    void inverse(VulkanComputeBatch &batch, const VulkanComputeBuffer &spectrum,
                 const VulkanComputeBuffer &complexOutput);

private:
    struct State;
    std::unique_ptr<State> state_;
};

}  // namespace libcbct

#endif
