#pragma once

#include <memory>

#include "Operators.h"
#include "ReconstructionBase.h"

namespace libcbct {

class LIBCBCT_API CpuWeightAndPad final : public WeightAndPad {
public:
    CpuWeightAndPad(CpuExecutionSession &session, const Geometry &geometry);
    void apply(ConstProjectionView projection, ProjectionView padded) const override;
    std::size_t paddedWidth() const noexcept { return paddedWidth_; }
private:
    ConstProjectionView weights_;
    std::size_t paddedWidth_;
};

// Batched real transforms along x. Real layout [length,rows], complex layout
// [length/2+1,rows] in natural Hermitian order. Length must be a power of two >=2.
// Both methods overwrite output and preserve input; overlapping buffers rejected.
// Retains one pocketfft plan and a row workspace, without process-global caching.
class LIBCBCT_API CpuRealFft {
public:
    CpuRealFft(CpuExecutionSession &session, std::size_t length, std::size_t rows);
    ~CpuRealFft();
    CpuRealFft(const CpuRealFft &) = delete;
    CpuRealFft &operator=(const CpuRealFft &) = delete;
    void forward(ConstProjectionView input, SpectrumView output);
    void inverse(ConstSpectrumView input, ProjectionView output);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

class LIBCBCT_API CpuSpectralMultiply {
public:
    CpuSpectralMultiply(CpuExecutionSession &session, std::span<const float> response);
    // In-place multiplication of both components, broadcasting response over rows.
    void apply(SpectrumView spectrum) const;
private:
    ConstProjectionView response_;
};

class LIBCBCT_API CpuCrop {
public:
    explicit CpuCrop(const CpuExecutionSession &session) : context_(session.context()) {}
    // Copy the leftmost output.shape()[0] columns; equal row counts, no aliasing.
    void apply(ConstProjectionView input, ProjectionView output) const;
private:
    ExecutionContext context_;
};

class LIBCBCT_API CpuProjectionFilter final : public ProjectionFilter {
public:
    CpuProjectionFilter(CpuExecutionSession &session, const Geometry &geometry, RampFilter filter);
    void apply(ProjectionView padded, ProjectionView filtered) override;
private:
    std::array<std::size_t, 2> detectorShape_;
    CpuRealFft fft_;
    CpuSpectralMultiply multiply_;
    CpuCrop crop_;
    SpectrumView spectrum_;
};

class LIBCBCT_API CpuFdkBackprojection final : public FdkBackprojection {
public:
    CpuFdkBackprojection(const CpuExecutionSession &session, const Geometry &geometry);
    void accumulate(ConstProjectionView filtered, VolumeView volume, float theta, float scale) const override;
private:
    ExecutionContext context_;
    Geometry geometry_;
};

}  // namespace libcbct
