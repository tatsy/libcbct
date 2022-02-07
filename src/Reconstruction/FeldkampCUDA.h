#ifdef _MSC_VER
#pragma once
#endif

#ifndef LIBCBCT_FELDKAMP_CUDA_H
#define LIBCBCT_FELDKAMP_CUDA_H

#include "ReconstructionBase.h"

class LIBCBCT_API FeldkampCUDA : public ReconstructionBase {
public:
    FeldkampCUDA(RampFilter filter = RampFilter::RamLak)
        : ReconstructionBase()
        , filter(filter) {
    }
    ~FeldkampCUDA() = default;

    void reconstruct(const FloatVolume &sinogram, FloatVolume &tomogram, const Geometry &geometry) const override;

private:
    RampFilter filter;
};

#endif  // LIBCBCT_FELDKAMP_CUDA_H
