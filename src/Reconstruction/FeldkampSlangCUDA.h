#pragma once

#if defined(LIBCBCT_WITH_SLANG_CUDA)

#include <filesystem>
#include <utility>

#include "ReconstructionBase.h"

// Execute precompiled Slang PTX with CUDA memory and cuFFT.
class LIBCBCT_API FeldkampSlangCUDA : public ReconstructionBase {
public:
    explicit FeldkampSlangCUDA(std::filesystem::path kernelDirectory, RampFilter filter = RampFilter::SheppLogan)
        : kernelDirectory_(std::move(kernelDirectory))
        , filter_(filter) {
    }

    VolumeF32 reconstruct(const VolumeF32 &sinogram, const Geometry &geometry) const override;

private:
    std::filesystem::path kernelDirectory_;
    RampFilter filter_;
};

#endif  // LIBCBCT_WITH_SLANG_CUDA
