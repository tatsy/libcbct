#pragma once

#include "Execution.h"

namespace libcbct {

class WeightAndPad {
public:
    virtual ~WeightAndPad() = default;
    // Float32 [detWidth,detHeight] -> [nPad,detHeight]. Overwrites all output,
    // including zeros to the right. Distinct buffers; pixel-center cosine weights.
    virtual void apply(ConstProjectionView projection, ProjectionView padded) const = 0;
};

class ProjectionFilter {
public:
    virtual ~ProjectionFilter() = default;
    // Float32 [nPad,detHeight] is overwritten by the normalized inverse FFT.
    // Float32 [detWidth,detHeight] receives its left crop, also by overwrite.
    // Distinct buffers. Forward FFT is unnormalized; inverse includes 1/nPad.
    virtual void apply(ProjectionView padded, ProjectionView filtered) = 0;
};

class FdkBackprojection {
public:
    virtual ~FdkBackprojection() = default;
    // Float32 detector [x,y] -> ADD to float32 volume [x,y,z]. Theta is radians;
    // scale is explicit (normally pi/N). Includes (SOD/L)^2, voxel/pixel centers,
    // half-pixel bilinear extrapolation, and a half-open detector domain.
    // Distinct buffers. This is FDK-specific, not a general adjoint projector.
    virtual void accumulate(ConstProjectionView filtered, VolumeView volume, float theta, float scale) const = 0;
};

}  // namespace libcbct
