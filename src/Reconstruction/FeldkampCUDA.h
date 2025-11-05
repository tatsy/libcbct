#ifdef _MSC_VER
#pragma once
#endif

#ifndef LIBCBCT_FELDKAMP_CUDA_H
#define LIBCBCT_FELDKAMP_CUDA_H

#include "ReconstructionBase.h"

class LIBCBCT_API FeldkampCUDA : public ReconstructionBase {
public:
    FeldkampCUDA(RampFilter filter = RampFilter::SheppLogan)
        : ReconstructionBase()
        , filter(filter) {
    }
    ~FeldkampCUDA() = default;

    void reconstruct(const VolumeF32 &sinogram, VolumeF32 &tomogram, const Geometry &geometry) const override;

private:
    RampFilter filter;
};

#endif  // LIBCBCT_FELDKAMP_CUDA_H
