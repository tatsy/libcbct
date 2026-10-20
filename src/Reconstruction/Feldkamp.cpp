#include "Feldkamp.h"

#include <limits>
#include <string>

#include "CpuOperators.h"
#include "detail/FdkRecipe.h"
#include "detail/FdkValidation.h"

namespace libcbct {
namespace {

void validateBackend(Backend backend) {
    if (backend != Backend::Cpu && backend != Backend::Cuda && backend != Backend::Vulkan) {
        throw InvalidArgument("Unknown execution backend");
    }
}

void requireCpu(Backend backend, const char *operation) {
    validateBackend(backend);
    if (backend != Backend::Cpu) {
        throw InvalidArgument(std::string(operation) + " operator is unavailable for the requested backend; "
                              "operator-based Feldkamp currently supports CPU only");
    }
}

}  // namespace

Feldkamp::Feldkamp(Backend backend, RampFilter filter)
    : Feldkamp(FeldkampOptions{ backend, filter, {}, {}, {} }) {
}

Feldkamp::Feldkamp(FeldkampOptions options)
    : options_(options) {
    validateBackend(options_.backend);
    detail::validateRampFilter(options_.filter);
    requireCpu(options_.weightBackend.value_or(options_.backend), "WeightAndPad");
    requireCpu(options_.filterBackend.value_or(options_.backend), "ProjectionFilter");
    requireCpu(options_.backprojectionBackend.value_or(options_.backend), "FdkBackprojection");
}

VolumeF32 Feldkamp::reconstruct(const VolumeF32 &sinogram, const Geometry &geometry) const {
    const auto paddedWidth = detail::validateFdkGeometry(geometry);
    const auto width = static_cast<std::size_t>(geometry.detSize.x);
    const auto height = static_cast<std::size_t>(geometry.detSize.y);
    if (sinogram.size<0>() != width || sinogram.size<1>() != height || !sinogram.size<2>()
        || sinogram.size<2>() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        throw InvalidArgument("FDK sinogram must match detector geometry and contain a supported positive view count");
    }

    // Backend/resource setup is outside the shared recipe. Borrow host input and
    // output so CPU reconstruction needs no projection or volume transfers.
    CpuExecutionSession session;
    CpuWeightAndPad weight(session, geometry);
    CpuProjectionFilter filter(session, geometry, options_.filter);
    CpuFdkBackprojection backprojection(session, geometry);
    auto padded = session.allocate<float>(std::array{ paddedWidth, height });
    auto filtered = session.allocate<float>(std::array{ width, height });
    VolumeF32 volume(geometry.volSize.x, geometry.volSize.y, geometry.volSize.z);
    detail::executeFdk(session.makeView(sinogram), padded, filtered, session.makeView(volume), weight, filter,
                       backprojection);
    return volume;
}

}  // namespace libcbct
