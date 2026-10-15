#pragma once

#if defined(LIBCBCT_WITH_VULKAN)

#include <filesystem>
#include <utility>

#include "ReconstructionBase.h"

namespace libcbct {

// FDK using shared Slang SPIR-V and a Vulkan radix-2 FFT.
class LIBCBCT_API FeldkampVulkan : public ReconstructionBase {
public:
    explicit FeldkampVulkan(std::filesystem::path kernelDirectory, RampFilter filter = RampFilter::SheppLogan,
                            int deviceIndex = -1)
        : kernelDirectory_(std::move(kernelDirectory))
        , filter_(filter)
        , deviceIndex_(deviceIndex) {
    }

    VolumeF32 reconstruct(const VolumeF32 &sinogram, const Geometry &geometry) const override;

private:
    std::filesystem::path kernelDirectory_;
    RampFilter filter_;
    int deviceIndex_;
};

}  // namespace libcbct

#endif
