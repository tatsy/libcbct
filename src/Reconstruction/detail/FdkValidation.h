#pragma once

#include <cmath>
#include <complex>
#include <limits>

#include "Reconstruction/FilterUtils.h"
#include "Utils/CheckedSize.h"

namespace libcbct::detail {

inline std::size_t validateFdkGeometry(const Geometry &geometry) {
    const auto positiveFinite = [](float value) { return std::isfinite(value) && value > 0.0f; };
    if (geometry.detSize.x < 2 || geometry.detSize.y < 2 || geometry.volSize.x <= 0
        || geometry.volSize.y <= 0 || geometry.volSize.z <= 0 || !positiveFinite(geometry.pixSize.x)
        || !positiveFinite(geometry.pixSize.y) || !positiveFinite(geometry.sod) || !positiveFinite(geometry.sdd))
        throw InvalidArgument("FDK requires detectors of at least 2x2 and positive finite geometry");

    // bilerp uses signed int indexing. Guard paddedFFTLength's doubling too.
    const auto width = static_cast<std::size_t>(geometry.detSize.x);
    const auto height = static_cast<std::size_t>(geometry.detSize.y);
    if (checkedProduct(width, height) > static_cast<std::size_t>(std::numeric_limits<int>::max())
        || width > static_cast<std::size_t>(std::numeric_limits<int>::max()) / 2)
        throw InvalidArgument("FDK detector dimensions exceed supported indexing");
    const auto padded = paddedFFTLength(width);
    if (padded > static_cast<std::size_t>(std::numeric_limits<int>::max()))
        throw InvalidArgument("FDK filter length exceeds supported indexing");
    const auto volumeBytes = checkedProduct(geometry.volSize.x, geometry.volSize.y, geometry.volSize.z, sizeof(float));
    const auto spectrumBytes = checkedProduct(padded / 2 + 1, height, sizeof(std::complex<float>));
    const auto realBytes = checkedProduct(padded, height, sizeof(float));
    const auto limit = static_cast<std::size_t>(std::numeric_limits<std::ptrdiff_t>::max());
    if (volumeBytes > limit || spectrumBytes > limit || realBytes > limit)
        throw InvalidArgument("FDK buffers exceed addressable range");
    // Preserve the legacy float evaluation order, including intermediate checks.
    const float pitch = geometry.pixSize.x * geometry.sod / geometry.sdd;
    const float spacing = (geometry.detSize.x * geometry.pixSize.x) * (geometry.sod / geometry.sdd)
                          / geometry.volSize.x;
    if (!positiveFinite(pitch) || !positiveFinite(spacing) || !positiveFinite(geometry.sdd * geometry.sdd))
        throw InvalidArgument("FDK derived geometry is not representable in float32");
    return padded;
}

inline void validateRampFilter(RampFilter filter) {
    if (filter != RampFilter::RamLak && filter != RampFilter::SheppLogan)
        throw InvalidArgument("Unknown FDK ramp filter");
}

}  // namespace libcbct::detail
