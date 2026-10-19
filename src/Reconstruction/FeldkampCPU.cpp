#include "FeldkampCPU.h"

#include <cmath>
#include <vector>

#include "Common/Constants.h"
#include "Common/Logging.h"
#include "Common/OpenMP.h"
#include "Common/ProgressBar.h"
#include "Utils/ImageUtils.h"
#include "Reconstruction/FilterUtils.h"

#include "pocketfft_hdronly.h"

namespace libcbct {

namespace pfft = pocketfft;

VolumeF32 FeldkampCPU::reconstruct(const VolumeF32 &sinogram, const Geometry &geometry) const {
    const size_t detWidth = sinogram.size<0>();
    const size_t detHeight = sinogram.size<1>();
    const size_t nProj = sinogram.size<2>();

    // Allocate output volume
    const vec3i volSize = geometry.volSize;
    LIBCBCT_DEBUG("Volume size: {}x{}x{}", volSize.x, volSize.y, volSize.z);
    VolumeF32 tomogram(volSize.x, volSize.y, volSize.z);

    // Precompute cosine weights
    const std::vector<float> cosWeights = makeCosineWeights(geometry, detWidth, detHeight);

    // Reconstruction
    const size_t nPad = paddedFFTLength(detWidth);
    const size_t nFreq = nPad / 2 + 1;
    const double dp = geometry.pixSize.x * geometry.sod / geometry.sdd;
    const std::vector<float> H = makeRampResponse(nPad, dp, filter);

    std::vector<float> padded(detHeight * nPad);
    std::vector<std::complex<float>> spec(detHeight * nFreq);
    std::vector<float> filtered(detWidth * detHeight);

    pfft::shape_t shape{ (size_t)detHeight, (size_t)nPad };
    pfft::stride_t strideReal{ (ptrdiff_t)(nPad * sizeof(float)), (ptrdiff_t)sizeof(float) };
    pfft::stride_t strideCplx{ (ptrdiff_t)(nFreq * sizeof(std::complex<float>)),
                               (ptrdiff_t)sizeof(std::complex<float>) };

    ProgressBar pbar(nProj);
    pbar.setDescription("RECON: ");
    for (size_t i = 0; i < nProj; i++) {
        // Copy sinogram slice and apply cosine weights
        std::fill(padded.begin(), padded.end(), 0.0f);
        const float *const ptr = sinogram.ptr() + (detWidth * detHeight * (uint64_t)i);
        for (size_t y = 0; y < detHeight; y++) {
            for (size_t x = 0; x < detWidth; x++) {
                padded[y * nPad + x] = ptr[y * detWidth + x] * cosWeights[y * detWidth + x];
            }
        }

        // pocketfft c2c
        pfft::r2c(shape, strideReal, strideCplx, pfft::shape_t{ 1 }, true, padded.data(), spec.data(), 1.0f, 0);

        // Apply filter in frequency domain
        for (size_t y = 0; y < detHeight; y++) {
            for (size_t x = 0; x < nFreq; x++) {
                spec[y * nFreq + x] *= H[x];
            }
        }

        // pocketfft c2r
        pfft::c2r(shape, strideCplx, strideReal, pfft::shape_t{ 1 }, false, spec.data(), padded.data(), 1.0f / nPad, 0);
        for (size_t y = 0; y < detHeight; y++) {
            std::copy_n(&padded[y * nPad], detWidth, &filtered[y * detWidth]);
        }

        // Backprojection
        const float theta = (float)libcbct::kTwoPi * i / nProj;
        const float scale = (float)libcbct::kPi / nProj;
        OMP_PARALLEL_FOR(int z = 0; z < volSize.z; z++) {
            for (int y = 0; y < volSize.y; y++) {
                for (int x = 0; x < volSize.x; x++) {
                    const vec3f uvw = vox2pix(vec3i(x, y, z), theta, geometry);
                    if (uvw.x >= 0 && uvw.y >= 0 && uvw.x < detWidth && uvw.y < detHeight) {
                        tomogram(x, y, z) +=
                            bilerp(filtered.data(), detWidth, detHeight, uvw.x - 0.5f, uvw.y - 0.5f) * uvw.z * scale;
                    }
                }
            }
        }
        pbar.step();
    }

    return tomogram;
}

}  // namespace libcbct
