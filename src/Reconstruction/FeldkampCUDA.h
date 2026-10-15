#ifdef _MSC_VER
#pragma once
#endif

#ifndef LIBCBCT_FELDKAMP_CUDA_H
#define LIBCBCT_FELDKAMP_CUDA_H

#include <filesystem>

#include "ReconstructionBase.h"

namespace libcbct {

// Execute Slang-generated PTX with CUDA memory and cuFFT.
class LIBCBCT_API FeldkampCUDA : public ReconstructionBase {
public:
    // The default directory is the kernel directory configured for this build.
    FeldkampCUDA(RampFilter filter = RampFilter::SheppLogan);
    explicit FeldkampCUDA(std::filesystem::path kernelDirectory, RampFilter filter = RampFilter::SheppLogan);
    ~FeldkampCUDA() = default;
    VolumeF32 reconstruct(const VolumeF32 &sinogram, const Geometry &geometry) const override;

private:
    std::filesystem::path kernelDirectory_;
    RampFilter filter_;
};

}  // namespace libcbct

#endif  // LIBCBCT_FELDKAMP_CUDA_H
