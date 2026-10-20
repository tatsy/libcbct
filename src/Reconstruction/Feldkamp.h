#pragma once

#include <optional>

#include "Execution.h"
#include "ReconstructionBase.h"

namespace libcbct {

struct FeldkampOptions {
    Backend backend = Backend::Cpu;
    RampFilter filter = RampFilter::SheppLogan;
    std::optional<Backend> weightBackend;
    std::optional<Backend> filterBackend;
    std::optional<Backend> backprojectionBackend;
};

// Operator-based FDK. Only CPU operators are currently registered. Explicit
// unsupported selections throw; no implicit fallback to another backend.
// Each reconstruct() has a fresh session; concurrent calls do not share workspaces.
class LIBCBCT_API Feldkamp : public ReconstructionBase {
public:
    explicit Feldkamp(Backend backend = Backend::Cpu, RampFilter filter = RampFilter::SheppLogan);
    explicit Feldkamp(FeldkampOptions options);
    VolumeF32 reconstruct(const VolumeF32 &sinogram, const Geometry &geometry) const override;

private:
    FeldkampOptions options_;
};

}  // namespace libcbct
