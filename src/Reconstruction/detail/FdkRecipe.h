#pragma once

#include "Common/Constants.h"
#include "Common/ProgressBar.h"
#include "Reconstruction/Operators.h"

namespace libcbct::detail {

// All resources are prepared by the execution adapter. No backend selection,
// allocation, transfers, synchronization, or FFT/shader ABI appears in this recipe.
inline void executeFdk(ConstVolumeView projections, ProjectionView padded, ProjectionView filtered,
                       VolumeView volume, const WeightAndPad &weight, ProjectionFilter &filter,
                       const FdkBackprojection &backprojection) {
    const auto count = projections.shape()[2];
    ProgressBar progress(static_cast<int>(count));
    progress.setDescription("RECON: ");
    for (std::size_t i = 0; i < count; ++i) {
        weight.apply(projections.slice(i), padded);
        filter.apply(padded, filtered);
        const float theta = static_cast<float>(kTwoPi) * i / count;
        const float scale = static_cast<float>(kPi) / count;
        backprojection.accumulate(filtered, volume, theta, scale);
        progress.step();
    }
}

}  // namespace libcbct::detail
