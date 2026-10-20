#ifndef LIBCBCT_NORMALIZATION_H
#define LIBCBCT_NORMALIZATION_H

#include <algorithm>
#include <cmath>
#include <utility>

#include "Utils/Volume.h"

namespace libcbct {

namespace detail {

// Validate before modifying a volume or converting its values to integers.
inline std::pair<float, float> finiteMinMax(const VolumeF32 &volume) {
    if (volume.empty()) {
        throw InvalidArgument("Cannot normalize an empty volume");
    }
    float minVal = volume.ptr()[0], maxVal = minVal;
    for (size_t i = 0; i < volume.numel(); ++i) {
        const float value = volume.ptr()[i];
        if (!std::isfinite(value)) {
            throw InvalidArgument("Cannot normalize a volume containing NaN or infinity");
        }
        minVal = std::min(minVal, value);
        maxVal = std::max(maxVal, value);
    }
    return { minVal, maxVal };
}

}  // namespace detail

// Clip negative values and divide by the positive maximum. Nonpositive inputs become zero.
inline void normalizePositiveMaximum(VolumeF32 &volume) {
    const auto [minVal, maxVal] = detail::finiteMinMax(volume);
    volume.forEach([maxVal](float value) { return maxVal > 0.0f ? std::max(0.0f, value) / maxVal : 0.0f; });
}

}  // namespace libcbct

#endif  // LIBCBCT_NORMALIZATION_H
