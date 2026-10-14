#define LIBCBCT_API_EXPORT
#include "FilterUtils.h"

#include <complex>

#include "Common/Constants.h"
#include "Common/Logging.h"
#include "pocketfft_hdronly.h"

namespace pfft = pocketfft;

size_t paddedFFTLength(size_t detWidth) {
    size_t nPad = 1;
    while (nPad < 2 * detWidth) nPad <<= 1;
    return nPad;
}

std::vector<float> makeRampResponse(size_t nPad, double dp, RampFilter type) {
    static const double kPi = (double)libcbct::kPi;

    std::vector<double> h(nPad, 0.0);
    for (size_t i = 0; i < nPad; i++) {
        const int n = (i <= nPad / 2) ? i : i - nPad;
        if (type == RampFilter::RamLak) {
            if (n == 0) {
                h[i] = 0.25 / (dp * dp);
            } else if (n % 2 != 0) {
                h[i] = -1.0 / (kPi * kPi * n * n * dp * dp);
            }
        } else if (type == RampFilter::SheppLogan) {
            h[i] = -2.0 / (kPi * kPi * dp * dp * (4.0 * n * n - 1.0));
        } else {
            LIBCBCT_ERROR("Unknown ramp filter specified!");
        }
    }

    std::vector<std::complex<double>> spec(nPad / 2 + 1);
    pfft::r2c(pfft::shape_t{ (size_t)nPad }, pfft::stride_t{ (ptrdiff_t)sizeof(double) },
              pfft::stride_t{ (ptrdiff_t)sizeof(std::complex<double>) }, 0, true, h.data(), spec.data(), 1.0);

    std::vector<float> H(nPad / 2 + 1);
    for (size_t k = 0; k < nPad / 2 + 1; k++) {
        H[k] = (float)(spec[k].real() * dp);
    }

    return H;
}

std::vector<float> makeCosineWeights(const Geometry &geometry, size_t detWidth, size_t detHeight) {
    std::vector<float> weights(detWidth * detHeight);
    const float sdd = geometry.sdd;
    for (size_t y = 0; y < detHeight; y++) {
        for (size_t x = 0; x < detWidth; x++) {
            // Pixel centers, consistent with project() and bilerp(..., u - 0.5f, v - 0.5f)
            const float u = (x + 0.5f - 0.5f * detWidth) * geometry.pixSize.x;
            const float v = (y + 0.5f - 0.5f * detHeight) * geometry.pixSize.y;
            weights[y * detWidth + x] = sdd / std::sqrt(sdd * sdd + u * u + v * v);
        }
    }
    return weights;
}
