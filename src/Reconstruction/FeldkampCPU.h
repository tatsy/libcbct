#ifdef _MSC_VER
#pragma once
#endif

#ifndef LIBCBCT_FELDKAMP_CPU_H
#define LIBCBCT_FELDKAMP_CPU_H

#include "ReconstructionBase.h"

class LIBCBCT_API FeldkampCPU : public ReconstructionBase {
public:
    FeldkampCPU(RampFilter filter = RampFilter::SheppLogan)
        : ReconstructionBase()
        , filter(filter) {
    }
    ~FeldkampCPU() = default;
    VolumeF32 reconstruct(const VolumeF32 &sinogram, const Geometry &geometry) const override;

private:
    RampFilter filter;
};

#endif  // LIBCBCT_FELDKAMP_CPU_H
