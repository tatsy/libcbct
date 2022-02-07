#ifdef _MSC_VER
#pragma once
#endif

#ifndef LIBCBCT_RECONSTRUCTION_BASE_H
#define LIBCBCT_RECONSTRUCTION_BASE_H

#include "Common/Api.h"
#include "Utils/Volume.h"
#include "Geometry/GeometryBase.h"

enum class RampFilter : int {
    RamLak,
    SheppLogan,
};

/**
 * @brief Interface class for CT reconstruction
 */
class LIBCBCT_API ReconstructionBase {
public:
    ReconstructionBase() = default;
    virtual ~ReconstructionBase() = default;

    virtual void reconstruct(const FloatVolume &sinogram, FloatVolume &tomogram, const Geometry &geometry) const = 0;

protected:

};

#endif  // LIBCBCT_RECONSTRUCTION_BASE_H
