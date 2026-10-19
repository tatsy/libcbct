#ifdef _MSC_VER
#pragma once
#endif

#ifndef LIBCBCT_FELDKAMP_CPU_H
#define LIBCBCT_FELDKAMP_CPU_H

#include "ReconstructionBase.h"

namespace libcbct {

class LIBCBCT_API FeldkampCpu : public ReconstructionBase {
public:
    FeldkampCpu(RampFilter filter = RampFilter::SheppLogan)
        : ReconstructionBase()
        , filter(filter) {
    }
    ~FeldkampCpu() = default;
    VolumeF32 reconstruct(const VolumeF32 &sinogram, const Geometry &geometry) const override;

private:
    RampFilter filter;
};

}  // namespace libcbct

#endif  // LIBCBCT_FELDKAMP_CPU_H
