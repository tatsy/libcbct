#define LIBCBCT_API_EXPORT
#include "FeldkampCPU.h"

#define _USE_MATH_DEFINES
#include <cmath>
#include <vector>

#include <opencv2/opencv.hpp>

#include "Common/Logging.h"
#include "Common/OpenMP.h"
#include "Common/ProgressBar.h"
#include "Utils/ImageUtils.h"

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
            H[x] = (2.0f / M_PI) * std::abs(std ::sin(0.5f * M_PI * q));
        } else {
            LIBCBCT_ERROR("Unknown ramp filter specified!");
        }
    }

    cv::Mat tempCplx(detHeight, detWidth, CV_32FC2);
    cv::Mat tempOut(detHeight, detWidth, CV_32FC1);

    ProgressBar pbar(nProj);
    pbar.setDescription("RECON: ");
    for (int i = 0; i < nProj; i++) {
        float *const ptr = sinogram.ptr() + (detWidth * detHeight * (uint64_t)i);
        const cv::Mat tempIn(detHeight, detWidth, CV_32FC1, reinterpret_cast<void *>(ptr));

        cv::dft(tempIn, tempCplx, cv::DFT_ROWS | cv::DFT_COMPLEX_OUTPUT);
        for (int y = 0; y < detHeight; y++) {
            for (int x = 0; x < detWidth; x++) {
                const float q = std::min(x, detWidth - x) / (0.5f * detWidth);
                tempCplx.at<std::complex<float>>(y, x) *= H[x];
            }
        }
        cv::idft(tempCplx, tempOut, cv::DFT_ROWS | cv::DFT_COMPLEX_INPUT | cv::DFT_REAL_OUTPUT | cv::DFT_SCALE);

        OMP_PARALLEL_FOR(int z = 0; z < volSize.z; z++) {
            for (int y = 0; y < volSize.y; y++) {
                for (int x = 0; x < volSize.x; x++) {
                    const float theta = 2.0f * (float)M_PI * i / nProj;
                    const vec3f uvw = vox2pix(vec3i(x, y, z), theta, geometry);
                    if (uvw.x >= 0 && uvw.y >= 0 && uvw.x < detWidth && uvw.y < detHeight) {
                        tomogram(x, y, z) += bilerp(reinterpret_cast<float *>(tempOut.data), detWidth, detHeight,
                                                    uvw.x - 0.5f, uvw.y - 0.5f)
                                             * uvw.z / nProj;
                    }
                }
            }
        }
        pbar.step();
    }

    return tomogram;
}
