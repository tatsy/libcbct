#define LIBCBCT_API_EXPORT
#include "FeldkampCPU.h"

#define _USE_MATH_DEFINES
#include <cmath>
#include <vector>

#include <opencv2/opencv.hpp>

#include "Common/Constants.h"
#include "Common/Logging.h"
#include "Common/OpenMP.h"
#include "Common/ProgressBar.h"
#include "Utils/ImageUtils.h"

#include "pocketfft_hdronly.h"

namespace pfft = pocketfft;

VolumeF32 FeldkampCPU::reconstruct(const VolumeF32 &sinogram, const Geometry &geometry) const {
    const int detWidth = sinogram.size<0>();
    const int detHeight = sinogram.size<1>();
    const int nProj = sinogram.size<2>();

    // Allocate output volume
    const vec3i volSize = geometry.volSize;
    LIBCBCT_DEBUG("Volume size: %dx%dx%d", volSize.x, volSize.y, volSize.z);
    VolumeF32 tomogram(volSize.x, volSize.y, volSize.z);

    // Precompute filter kernel
    std::vector<float> H(detWidth);
    for (int x = 0; x < detWidth; x++) {
        const float q = std::min(x, detWidth - x) / (0.5f * detWidth);
        if (filter == RampFilter::RamLak) {
            H[x] = std::abs(q);
        } else if (filter == RampFilter::SheppLogan) {
            H[x] = (2.0f * (float)libcbct::kOneOverPi) * std::abs(std ::sin((float)libcbct::kHalfPi * q));
        } else {
            LIBCBCT_ERROR("Unknown ramp filter specified!");
        }
    }

    std::vector<float> tempInOut(detWidth * detHeight);
    std::vector<std::complex<float>> tempCplx(detWidth * detHeight);

    pfft::shape_t shape{ (size_t)detHeight, (size_t)detWidth };
    pfft::stride_t strideReal{ (int64_t)(detWidth * sizeof(float)), (int64_t)sizeof(float) };
    pfft::stride_t strideCplx{ (int64_t)(detWidth * sizeof(std::complex<float>)),
                               (int64_t)sizeof(std::complex<float>) };
    pfft::shape_t axes{ 1 };

    ProgressBar pbar(nProj);
    pbar.setDescription("RECON: ");
    for (int i = 0; i < nProj; i++) {
        // Copy sinogram to temp buffer
        float *const ptr = sinogram.ptr() + (detWidth * detHeight * (uint64_t)i);
        for (int y = 0; y < detHeight; ++y) {
            for (int x = 0; x < detWidth; ++x) {
                tempInOut[y * detWidth + x] = ptr[y * detWidth + x];
            }
        }

        // pocketfft c2c
        pfft::r2c(shape, strideReal, strideCplx, axes, true, tempInOut.data(), tempCplx.data(), 1.0f, 0);

        // Apply filter in frequency domain
        for (int y = 0; y < detHeight; y++) {
            for (int x = 0; x < detWidth; x++) {
                const float q = std::min(x, detWidth - x) / (0.5f * detWidth);
                tempCplx[y * detWidth + x] *= H[x];
            }
        }

        // pocketfft c2r
        pfft::c2r(shape, strideCplx, strideReal, axes, false, tempCplx.data(), tempInOut.data(), 1.0f / detWidth, 0);

        // Backprojection
        OMP_PARALLEL_FOR(int z = 0; z < volSize.z; z++) {
            for (int y = 0; y < volSize.y; y++) {
                for (int x = 0; x < volSize.x; x++) {
                    const float theta = (float)libcbct::kTwoPi * i / nProj;
                    const vec3f uvw = vox2pix(vec3i(x, y, z), theta, geometry);
                    if (uvw.x >= 0 && uvw.y >= 0 && uvw.x < detWidth && uvw.y < detHeight) {
                        tomogram(x, y, z) +=
                            bilerp(tempInOut.data(), detWidth, detHeight, uvw.x - 0.5f, uvw.y - 0.5f) * uvw.z / nProj;
                    }
                }
            }
        }
        pbar.step();
    }

    return tomogram;
}
