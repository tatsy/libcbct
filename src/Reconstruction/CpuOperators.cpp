#include "CpuOperators.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "Common/OpenMP.h"
#include "FilterUtils.h"
#include "Utils/ImageUtils.h"
#include "detail/FdkValidation.h"
#include "pocketfft_hdronly.h"

namespace libcbct {
namespace {

template <typename T, std::size_t Rank>
void requireView(const TensorView<T, Rank> &view, const ExecutionContext &context,
                 std::array<std::size_t, Rank> shape) {
    if (view.context() != context || view.backend() != Backend::Cpu) {
        throw InvalidArgument("Operator buffer belongs to a different execution context");
    }
    if (view.shape() != shape) {
        throw InvalidArgument("Operator buffer shape mismatch");
    }
}

template <typename A, typename B>
void requireDistinct(const A &a, const B &b) {
    if (a.overlaps(b)) {
        throw InvalidArgument("Operator input and output must not overlap");
    }
}

ConstProjectionView prepareWeights(CpuExecutionSession &session, const Geometry &geometry) {
    detail::validateFdkGeometry(geometry);
    const auto width = static_cast<std::size_t>(geometry.detSize.x);
    const auto height = static_cast<std::size_t>(geometry.detSize.y);
    const auto weights = makeCosineWeights(geometry, width, height);
    auto view = session.allocate<float>(std::array{ width, height });
    std::copy(weights.begin(), weights.end(), view.hostElements().begin());
    return view;
}

std::array<std::size_t, 2> validatedDetector(const Geometry &geometry, RampFilter filter) {
    detail::validateRampFilter(filter);
    detail::validateFdkGeometry(geometry);
    return { static_cast<std::size_t>(geometry.detSize.x), static_cast<std::size_t>(geometry.detSize.y) };
}

std::size_t validateFft(std::size_t length, std::size_t rows) {
    if (length < 2 || (length & (length - 1)) || !rows
        || length > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        throw InvalidArgument("CPU real FFT requires a power-of-two length >=2 and positive rows");
    }
    const auto bytes = detail::checkedProduct(length / 2 + 1, rows, sizeof(std::complex<float>));
    if (bytes > static_cast<std::size_t>(std::numeric_limits<std::ptrdiff_t>::max())) {
        throw InvalidArgument("CPU real FFT dimensions exceed addressable range");
    }
    return length;
}

ConstProjectionView prepareResponse(CpuExecutionSession &session, std::span<const float> response) {
    if (response.empty() || !std::all_of(response.begin(), response.end(), [](float x) { return std::isfinite(x); })) {
        throw InvalidArgument("Spectral response must be nonempty and finite");
    }
    auto view = session.allocate<float>(std::array{ response.size(), std::size_t{ 1 } });
    std::copy(response.begin(), response.end(), view.hostElements().begin());
    return view;
}

}  // namespace

CpuWeightAndPad::CpuWeightAndPad(CpuExecutionSession &session, const Geometry &geometry)
    : weights_(prepareWeights(session, geometry))
    , paddedWidth_(paddedFFTLength(weights_.shape()[0])) {
}

void CpuWeightAndPad::apply(ConstProjectionView projection, ProjectionView padded) const {
    const auto [width, height] = weights_.shape();
    requireView(projection, weights_.context(), weights_.shape());
    requireView(padded, weights_.context(), std::array{ paddedWidth_, height });
    requireDistinct(projection, padded);
    const auto input = projection.hostElements(), weights = weights_.hostElements();
    auto output = padded.hostElements();
    std::fill(output.begin(), output.end(), 0.0f);
    for (std::size_t y = 0; y < height; ++y) {
        for (std::size_t x = 0; x < width; ++x) {
            output[y * paddedWidth_ + x] = input[y * width + x] * weights[y * width + x];
        }
    }
}

struct CpuRealFft::Impl {
    std::size_t length, rows;
    pocketfft::detail::pocketfft_r<float> plan;
    ProjectionView scratch;
#ifndef POCKETFFT_NO_VECTORS
    static constexpr std::size_t lanes = pocketfft::detail::VLEN<float>::val;
    // Native FFT workspace needs the alignment required by pocketfft's SIMD type.
    pocketfft::detail::arr<pocketfft::detail::vtype_t<float>> vectorScratch;
#endif
    Impl(CpuExecutionSession &session, std::size_t n, std::size_t h)
        : length(validateFft(n, h))
        , rows(h)
        , plan(length)
        , scratch(session.allocate<float>(std::array{ length, std::size_t{ 1 } }))
#ifndef POCKETFFT_NO_VECTORS
        , vectorScratch(rows >= lanes ? length : 0)
#endif
    {
    }
};

CpuRealFft::CpuRealFft(CpuExecutionSession &session, std::size_t length, std::size_t rows)
    : impl_(std::make_unique<Impl>(session, length, rows)) {
}
CpuRealFft::~CpuRealFft() = default;

void CpuRealFft::forward(ConstProjectionView input, SpectrumView output) {
    auto &p = *impl_;
    const auto frequencies = p.length / 2 + 1;
    requireView(input, p.scratch.context(), std::array{ p.length, p.rows });
    requireView(output, p.scratch.context(), std::array{ frequencies, p.rows });
    requireDistinct(input, output);
    const auto source = input.hostElements();
    auto destination = output.hostElements();
    auto row = p.scratch.hostElements();
    std::size_t y = 0;
#ifndef POCKETFFT_NO_VECTORS
    // Preserve the legacy batched SIMD path, including partial batches below.
    for (; y + Impl::lanes <= p.rows; y += Impl::lanes) {
        auto *batch = p.vectorScratch.data();
        for (std::size_t x = 0; x < p.length; ++x) {
            for (std::size_t lane = 0; lane < Impl::lanes; ++lane) {
                batch[x][lane] = source[(y + lane) * p.length + x];
            }
        }
        p.plan.exec(batch, 1.0f, true);
        for (std::size_t lane = 0; lane < Impl::lanes; ++lane) {
            const auto base = (y + lane) * frequencies;
            destination[base] = { batch[0][lane], 0.0f };
            for (std::size_t k = 1; k < frequencies - 1; ++k) {
                destination[base + k] = { batch[2 * k - 1][lane], batch[2 * k][lane] };
            }
            destination[base + frequencies - 1] = { batch[p.length - 1][lane], 0.0f };
        }
    }
#endif
    for (; y < p.rows; ++y) {
        std::copy_n(source.data() + y * p.length, p.length, row.data());
        // The vendored pocketfft plan uses FFTPACK real packing:
        // DC, Re(1), Im(1), ..., Nyquist. Keep this ABI detail in the adapter.
        // pocketfft still allocates transient native scratch inside exec().
        p.plan.exec(row.data(), 1.0f, true);
        destination[y * frequencies] = { row[0], 0.0f };
        for (std::size_t k = 1; k < frequencies - 1; ++k) {
            destination[y * frequencies + k] = { row[2 * k - 1], row[2 * k] };
        }
        destination[y * frequencies + frequencies - 1] = { row[p.length - 1], 0.0f };
    }
}

void CpuRealFft::inverse(ConstSpectrumView input, ProjectionView output) {
    auto &p = *impl_;
    const auto frequencies = p.length / 2 + 1;
    requireView(input, p.scratch.context(), std::array{ frequencies, p.rows });
    requireView(output, p.scratch.context(), std::array{ p.length, p.rows });
    requireDistinct(input, output);
    const auto source = input.hostElements();
    auto destination = output.hostElements();
    auto row = p.scratch.hostElements();
    std::size_t y = 0;
#ifndef POCKETFFT_NO_VECTORS
    for (; y + Impl::lanes <= p.rows; y += Impl::lanes) {
        auto *batch = p.vectorScratch.data();
        for (std::size_t lane = 0; lane < Impl::lanes; ++lane) {
            const auto base = (y + lane) * frequencies;
            batch[0][lane] = source[base].real();
            for (std::size_t k = 1; k < frequencies - 1; ++k) {
                batch[2 * k - 1][lane] = source[base + k].real();
                batch[2 * k][lane] = source[base + k].imag();
            }
            batch[p.length - 1][lane] = source[base + frequencies - 1].real();
        }
        p.plan.exec(batch, 1.0f / p.length, false);
        for (std::size_t x = 0; x < p.length; ++x) {
            for (std::size_t lane = 0; lane < Impl::lanes; ++lane) {
                destination[(y + lane) * p.length + x] = batch[x][lane];
            }
        }
    }
#endif
    for (; y < p.rows; ++y) {
        row[0] = source[y * frequencies].real();
        for (std::size_t k = 1; k < frequencies - 1; ++k) {
            row[2 * k - 1] = source[y * frequencies + k].real();
            row[2 * k] = source[y * frequencies + k].imag();
        }
        row[p.length - 1] = source[y * frequencies + frequencies - 1].real();
        p.plan.exec(row.data(), 1.0f / p.length, false);
        std::copy(row.begin(), row.end(), destination.data() + y * p.length);
    }
}

CpuSpectralMultiply::CpuSpectralMultiply(CpuExecutionSession &session, std::span<const float> response)
    : response_(prepareResponse(session, response)) {
}

void CpuSpectralMultiply::apply(SpectrumView spectrum) const {
    const auto frequencies = response_.shape()[0];
    requireView(spectrum, response_.context(), std::array{ frequencies, spectrum.shape()[1] });
    auto values = spectrum.hostElements();
    const auto response = response_.hostElements();
    for (std::size_t y = 0; y < spectrum.shape()[1]; ++y) {
        for (std::size_t k = 0; k < frequencies; ++k) {
            values[y * frequencies + k] *= response[k];
        }
    }
}

void CpuCrop::apply(ConstProjectionView input, ProjectionView output) const {
    requireView(input, context_, input.shape());
    requireView(output, context_, std::array{ output.shape()[0], input.shape()[1] });
    if (output.shape()[0] > input.shape()[0]) {
        throw InvalidArgument("Crop output is wider than its input");
    }
    requireDistinct(input, output);
    const auto source = input.hostElements();
    auto destination = output.hostElements();
    for (std::size_t y = 0; y < output.shape()[1]; ++y) {
        std::copy_n(source.data() + y * input.shape()[0], output.shape()[0],
                    destination.data() + y * output.shape()[0]);
    }
}

CpuProjectionFilter::CpuProjectionFilter(CpuExecutionSession &session, const Geometry &geometry, RampFilter filter)
    : detectorShape_(validatedDetector(geometry, filter))
    , fft_(session, paddedFFTLength(detectorShape_[0]), detectorShape_[1])
    , multiply_(session, makeRampResponse(paddedFFTLength(detectorShape_[0]),
                                          geometry.pixSize.x * geometry.sod / geometry.sdd, filter))
    , crop_(session)
    , spectrum_(session.allocate<std::complex<float>>(
          std::array{ paddedFFTLength(detectorShape_[0]) / 2 + 1, detectorShape_[1] })) {
}

void CpuProjectionFilter::apply(ProjectionView padded, ProjectionView filtered) {
    // Validate the complete composite operation before modifying either caller buffer.
    requireView(padded, spectrum_.context(), std::array{ (spectrum_.shape()[0] - 1) * 2, detectorShape_[1] });
    requireView(filtered, spectrum_.context(), detectorShape_);
    requireDistinct(padded, filtered);
    fft_.forward(padded, spectrum_);
    multiply_.apply(spectrum_);
    fft_.inverse(spectrum_, padded);
    crop_.apply(padded, filtered);
}

CpuFdkBackprojection::CpuFdkBackprojection(const CpuExecutionSession &session, const Geometry &geometry)
    : context_(session.context())
    , geometry_(geometry) {
    detail::validateFdkGeometry(geometry_);
}

void CpuFdkBackprojection::accumulate(ConstProjectionView filtered, VolumeView volume, float theta, float scale) const {
    const auto width = static_cast<std::size_t>(geometry_.detSize.x);
    const auto height = static_cast<std::size_t>(geometry_.detSize.y);
    const auto size = geometry_.volSize;
    requireView(filtered, context_, std::array{ width, height });
    requireView(volume, context_,
                std::array{ static_cast<std::size_t>(size.x), static_cast<std::size_t>(size.y),
                            static_cast<std::size_t>(size.z) });
    requireDistinct(filtered, volume);
    if (!std::isfinite(theta) || !std::isfinite(scale)) {
        throw InvalidArgument("FDK angle and scale must be finite");
    }
    const float *projection = filtered.hostElements().data();
    float *output = volume.hostElements().data();
    OMP_PARALLEL_FOR(int z = 0; z < size.z; ++z) {
        for (int y = 0; y < size.y; ++y) {
            for (int x = 0; x < size.x; ++x) {
                const auto uvw = vox2pix(vec3i(x, y, z), theta, geometry_);
                if (uvw.x >= 0 && uvw.y >= 0 && uvw.x < width && uvw.y < height) {
                    const auto index = (static_cast<std::size_t>(z) * size.y + y) * size.x + x;
                    output[index] += bilerp(projection, static_cast<int>(width), static_cast<int>(height), uvw.x - 0.5f,
                                            uvw.y - 0.5f)
                                     * uvw.z * scale;
                }
            }
        }
    }
}

}  // namespace libcbct
